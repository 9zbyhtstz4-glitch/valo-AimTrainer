// AimTrainerCharacter の実装
#include "AimTrainerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

namespace
{
	/**
	 * 入力セットアップの設定不備を「ログ」と「画面」の両方へ確実に出す。
	 * ログのみの警告では今回のような無反応バグの原因究明が難しいため、
	 * PIE中は画面左上へ赤字で15秒間表示する(Shippingビルドでは画面表示なし)。
	 */
	void ReportInputConfigProblem(const FString& Message)
	{
		UE_LOG(LogTemp, Error, TEXT("[AimTrainer] %s"), *Message);
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				/*Key=*/INDEX_NONE, /*TimeToDisplay=*/15.f, FColor::Red,
				FString::Printf(TEXT("[AimTrainer] %s"), *Message));
		}
#endif
	}
}

AAimTrainerCharacter::AAimTrainerCharacter()
{
	// 視点・移動・ジャンプ・しゃがみともイベント/コンポーネント駆動のため、本クラスのTickは不要
	PrimaryActorTick.bCanEverTick = false;

	// --- カプセル(当たり判定)の基本サイズ ---
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// --- FPS視点カメラ ---
	// 目線高さ(カプセル中心から+64uu)に取り付ける。
	// bUsePawnControlRotation によりピッチ/ヨーがコントローラ回転へ追従する。
	// しゃがみ時はACharacter標準処理でカプセルが縮み、カメラも自動的に低くなる。
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	// --- 回転設定 ---
	// FPSなので体(アクター)のヨーはコントローラへ追従させる。
	// ピッチ/ロールはカメラ側のみで表現する。
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// --- 移動/ジャンプ/しゃがみの既定値をエディタ表示と一致させる ---
	// (Blueprint側で編集された場合は BeginPlay で再反映される)
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MaxWalkSpeedCrouched = CrouchedSpeed;
	Movement->JumpZVelocity = JumpZVelocity;
	Movement->GravityScale = GravityScale;

	// しゃがみを有効化する。
	// これを立てないと ACharacter::Crouch() が無視される点に注意。
	Movement->GetNavAgentPropertiesRef().bCanCrouch = true;

	// 連続ジャンプ(空中での多段ジャンプ)を許可しない。
	// ※ACharacter の既定値も1だが、仕様として明示しておく。
	JumpMaxCount = 1;
}

void AAimTrainerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Blueprintやインスタンスで編集されたプロパティを移動コンポーネントへ反映する。
	// 立ち速度(MaxWalkSpeed)としゃがみ速度(MaxWalkSpeedCrouched)は別プロパティで管理し、
	// しゃがみ状態に応じた上限の切り替えは CharacterMovementComponent が自動で行う。
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MaxWalkSpeedCrouched = CrouchedSpeed;
	Movement->JumpZVelocity = JumpZVelocity;
	Movement->GravityScale = GravityScale;
}

void AAimTrainerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// --- マッピングコンテキストの登録 ---
	// UE5.4以降のテンプレートと同じく、コントローラ確定後に必ず呼ばれる本関数で登録する。
	// SetupPlayerInputComponent 内での登録は「その時点でコントローラが取れること」が
	// 前提になるため、登録漏れの単一障害点になり得る。ここに一本化する。
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				// 本関数は再Possess等で複数回呼ばれ得るため、二重登録を避けてから登録する
				Subsystem->RemoveMappingContext(DefaultMappingContext);
				Subsystem->AddMappingContext(DefaultMappingContext, /*Priority=*/0);

				UE_LOG(LogTemp, Log, TEXT("[AimTrainer] IMC を登録しました: %s"),
					*GetNameSafe(DefaultMappingContext));
			}
			else
			{
				// ★入力が一切効かない場合の最有力原因その1★
				ReportInputConfigProblem(TEXT(
					"DefaultMappingContext が None です。入力は一切動作しません。"
					"BP_AimTrainerCharacter の Details > AimTrainer|Input で IMC_Default を割り当ててください。"));
			}
		}
	}
}

void AAimTrainerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// --- 入力バインド ---
	// ※IMCの登録は NotifyControllerChanged で行う(本関数はバインド専任)。
	// DefaultInput.ini で EnhancedInputComponent を既定化済みのため通常はキャスト成功する。
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		ReportInputConfigProblem(TEXT(
			"EnhancedInputComponent へのキャストに失敗しました。"
			"DefaultInput.ini の DefaultInputComponentClass を確認してください。"));
		return;
	}

	// 移動: キーを押している間は毎Tick発火する Triggered にバインドする。
	// AddMovementInput は「そのTickの移動意図」を渡すだけで移動量を直接加算しないため、
	// 発火頻度(フレームレート)が変わっても移動速度は変わらない。
	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAimTrainerCharacter::Input_Move);
	}
	else
	{
		ReportInputConfigProblem(TEXT("MoveAction が None です(WASD移動不可)。BP_AimTrainerCharacter で IA_Move を割り当ててください。"));
	}

	// ジャンプ: ACharacter 標準関数へ直接バインドする(独自のジャンプ処理は作らない)。
	//  - Started   = 押した瞬間に1回だけ発火 → Jump()
	//  - Completed = 離した瞬間に発火       → StopJumping()
	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	else
	{
		ReportInputConfigProblem(TEXT("JumpAction が None です(ジャンプ不可)。BP_AimTrainerCharacter で IA_Jump を割り当ててください。"));
	}

	// しゃがみ(ホールド式): ACharacter 標準関数へ直接バインドする。
	// 末尾の false は Crouch/UnCrouch の引数 bClientSimulation(サーバー主導の複製用)。
	if (CrouchAction)
	{
		EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &ACharacter::Crouch, false);
		EIC->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ACharacter::UnCrouch, false);
	}
	else
	{
		ReportInputConfigProblem(TEXT("CrouchAction が None です(しゃがみ不可)。BP_AimTrainerCharacter で IA_Crouch を割り当ててください。"));
	}

	// 視点操作: マウス移動中は毎フレーム発火する Triggered にバインドする
	if (LookAction)
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AAimTrainerCharacter::Input_Look);
	}
	else
	{
		ReportInputConfigProblem(TEXT("LookAction が None です(視点操作不可)。BP_AimTrainerCharacter で IA_Look を割り当ててください。"));
	}
}

void AAimTrainerCharacter::Input_Move(const FInputActionValue& Value)
{
	if (!Controller)
	{
		return;
	}

	// IA_Move は Axis2D(X=右+, Y=前+)。値の組み立てはIMC側のモディファイアが行う。
	const FVector2D MoveValue = Value.Get<FVector2D>();

	// 視点のヨーのみを基準に、水平な前方向・右方向を求める。
	// (ピッチを含めると見上げ中に前進入力が浮き上がる方向へ向くため除外する)
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// 移動の「意図」だけを渡す。実際の加速・速度上限(立ち/しゃがみで自動切替)・衝突は
	// CharacterMovementComponent が DeltaTime を用いて計算するため、
	// 60fps/144fps以上でも最終的な移動速度は一定になる(フレームレート非依存)。
	AddMovementInput(ForwardDir, MoveValue.Y);
	AddMovementInput(RightDir, MoveValue.X);
}

void AAimTrainerCharacter::Input_Look(const FInputActionValue& Value)
{
	// IA_Look は Axis2D(X=マウス左右, Y=マウス上下)
	const FVector2D LookValue = Value.Get<FVector2D>();

	// ヨー: マウス右で右を向く(そのままの符号でよい)
	AddControllerYawInput(LookValue.X * MouseSensitivity);

	// ピッチ: マウス前方移動は生値が正(+Y)。
	// 本プロジェクトは DefaultInput.ini で bEnableLegacyInputScales=True のため
	// AddControllerPitchInput には旧来の InputPitchScale(負値)が掛かる。
	// そのため既定(非反転)では -1 を掛けることで「マウス上=見上げる」になる。
	// 環境差で上下が逆に感じる場合は bInvertLookY で切り替えられる。
	const float PitchSign = bInvertLookY ? 1.f : -1.f;
	AddControllerPitchInput(LookValue.Y * PitchSign * MouseSensitivity);
}
