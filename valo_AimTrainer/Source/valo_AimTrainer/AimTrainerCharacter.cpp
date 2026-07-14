// AimTrainerCharacter の実装
#include "AimTrainerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

AAimTrainerCharacter::AAimTrainerCharacter()
{
	// 視点・移動・ジャンプともイベント/コンポーネント駆動のため、本クラスのTickは不要
	PrimaryActorTick.bCanEverTick = false;

	// --- カプセル(当たり判定)の基本サイズ ---
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// --- FPS視点カメラ ---
	// 目線高さ(カプセル中心から+64uu)に取り付ける。
	// bUsePawnControlRotation によりピッチ/ヨーがコントローラ回転へ追従する。
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

	// --- 移動/ジャンプの既定値をエディタ表示と一致させる ---
	// (Blueprint側で編集された場合は BeginPlay で再反映される)
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->JumpZVelocity = JumpZVelocity;
	Movement->GravityScale = GravityScale;

	// 連続ジャンプ(空中での多段ジャンプ)を許可しない。
	// ※ACharacter の既定値も1だが、仕様として明示しておく。
	JumpMaxCount = 1;
}

void AAimTrainerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Blueprintやインスタンスで編集されたプロパティを移動コンポーネントへ反映する。
	// ジャンプ・落下の物理計算は CharacterMovementComponent の責務
	// (初速 JumpZVelocity を与えた後、重力を DeltaTime で積分する)。
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->JumpZVelocity = JumpZVelocity;
	Movement->GravityScale = GravityScale;
}

void AAimTrainerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// --- マッピングコンテキストの登録 ---
	// BP_AimTrainerCharacter で割り当てた IMC_Default をローカルプレイヤーへ登録する。
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, /*Priority=*/0);
			}
			else
			{
				// アセット未割り当ての検出用(C++クラスを直接スポーンした場合など)
				UE_LOG(LogTemp, Warning,
					TEXT("[AimTrainer] DefaultMappingContext が未割り当てです。BP_AimTrainerCharacter で IMC_Default を設定してください。"));
			}
		}
	}

	// --- 入力バインド ---
	// DefaultInput.ini で EnhancedInputComponent を既定化済みのため通常はキャスト成功する。
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[AimTrainer] EnhancedInputComponent が見つかりません。DefaultInput.ini の設定を確認してください。"));
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
		UE_LOG(LogTemp, Warning,
			TEXT("[AimTrainer] MoveAction が未割り当てです。BP_AimTrainerCharacter で IA_Move を設定してください。"));
	}

	// ジャンプ: ACharacter 標準関数へ直接バインドする(独自のジャンプ処理は作らない)。
	//  - Started   = 押した瞬間に1回だけ発火 → Jump()
	//  - Completed = 離した瞬間に発火       → StopJumping()
	// Started はホールド中に再発火しないため、押しっぱなしによる連続ジャンプは発生しない。
	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[AimTrainer] JumpAction が未割り当てです。BP_AimTrainerCharacter で IA_Jump を設定してください。"));
	}

	// 視点操作: マウス移動中は毎フレーム発火する Triggered にバインドする
	if (LookAction)
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AAimTrainerCharacter::Input_Look);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[AimTrainer] LookAction が未割り当てです。BP_AimTrainerCharacter で IA_Look を設定してください。"));
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

	// 移動の「意図」だけを渡す。実際の加速・速度上限(MaxWalkSpeed)・衝突は
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
