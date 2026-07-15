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
	// fix3: しゃがみ時のカメラ高さ補間のためTickを有効化する。
	// Tick内の処理はFInterpTo一回のみで、負荷は無視できる。
	PrimaryActorTick.bCanEverTick = true;

	// --- カプセル(当たり判定)の基本サイズ ---
	// 立ち: 半径42 / 半分高さ96(全高192cm)。VALORANTのエージェント身長感に相当。
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// --- FPS視点カメラ ---
	// 立ち目線=地上160uu(カプセル中心+64)。ピッチ/ヨーはコントローラ回転へ追従。
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, StandingCameraHeight));
	FirstPersonCamera->bUsePawnControlRotation = true;

	// --- 回転設定 ---
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// --- チューニング値の適用(エディタ表示用の既定値) ---
	ApplyCharacterSettings();

	// しゃがみを有効化する(これを立てないと Crouch() が無視される)
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;

	// 連続ジャンプ(空中での多段ジャンプ)を許可しない
	JumpMaxCount = 1;
}

void AAimTrainerCharacter::ApplyCharacterSettings()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();

	// --- 速度(VALORANT: 走行5.4m/s、しゃがみ約半分) ---
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MaxWalkSpeedCrouched = CrouchedSpeed;

	// --- 加速・減速・摩擦(fix3の中核) ---
	// UE既定(加速2048/制動2048/摩擦8)は慣性が強く「滑る」ため、
	// VALORANTの「即応・低慣性・切り返しが利く」感覚へ寄せる。詳細はヘッダの各コメント参照。
	Movement->MaxAcceleration = MoveAcceleration;
	Movement->BrakingDecelerationWalking = MoveBrakingDeceleration;
	Movement->GroundFriction = MoveGroundFriction;

	// 制動専用摩擦を有効化。BrakingFrictionFactor(既定2)を1へ固定し、
	// MoveBrakingFriction の値がそのまま効くようにして調整を単純化する。
	Movement->bUseSeparateBrakingFriction = true;
	Movement->BrakingFriction = MoveBrakingFriction;
	Movement->BrakingFrictionFactor = 1.f;

	// --- 空中 ---
	Movement->AirControl = AirControlAmount;

	// --- ジャンプ・重力(VALORANT: 短く鋭いジャンプ) ---
	Movement->JumpZVelocity = JumpZVelocity;
	Movement->GravityScale = GravityScale;

	// --- しゃがみカプセル(立ち96 → しゃがみ60。半径は不変) ---
	Movement->SetCrouchedHalfHeight(CrouchedCapsuleHalfHeight);

	// --- カメラFOV(VALORANT: 水平103固定) ---
	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetFieldOfView(CameraFOV);
	}
}

void AAimTrainerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Blueprintやインスタンスで編集されたプロパティを実行時コンポーネントへ反映する
	ApplyCharacterSettings();
}

// ==============================
// しゃがみのスムーズ化(fix3)
// ==============================
// 従来はカプセル縮小と同時にカメラのワールド高さが1フレームで約50uu落ちて
// 「視点が急激に下がる」違和感があった。fix3では
//   (1) OnStartCrouch/OnEndCrouch でカプセル中心の移動量ぶんだけカメラの相対Zを
//       即時に打ち消し、視点のワールド高さを変化させない
//   (2) その後 Tick の FInterpTo で目標高さへ滑らかに移行する
// の2段構えで、VALORANTのようにスッと沈む/立つカメラを実現する。
// カプセル(当たり判定)自体はACharacter標準どおり即時に変形するため、
// 天井判定・しゃがみ速度などの既存挙動は一切変わらない。

float AAimTrainerCharacter::GetTargetCameraHeight() const
{
	return bIsCrouched ? CrouchedCameraHeight : StandingCameraHeight;
}

void AAimTrainerCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// カプセル中心は ScaledHalfHeightAdjust だけ下がったので、
	// 同量をカメラの相対Zへ足して視点のワールド高さを維持する(以後Tickで補間)。
	if (FirstPersonCamera)
	{
		FVector CamLoc = FirstPersonCamera->GetRelativeLocation();
		CamLoc.Z += ScaledHalfHeightAdjust;
		FirstPersonCamera->SetRelativeLocation(CamLoc);
	}
}

void AAimTrainerCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// カプセル中心は ScaledHalfHeightAdjust だけ上がったので、逆に引いて維持する。
	if (FirstPersonCamera)
	{
		FVector CamLoc = FirstPersonCamera->GetRelativeLocation();
		CamLoc.Z -= ScaledHalfHeightAdjust;
		FirstPersonCamera->SetRelativeLocation(CamLoc);
	}
}

void AAimTrainerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// カメラの相対高さを目標値へ補間する(しゃがみ/立ちの遷移中のみ実際に動く)。
	// FInterpToは指数的に収束するため、VALORANTのしゃがみモーションのような
	// 「最初速く、終わり際は緩やか」なカーブになる。
	if (FirstPersonCamera)
	{
		FVector CamLoc = FirstPersonCamera->GetRelativeLocation();
		const float TargetZ = GetTargetCameraHeight();

		if (!FMath::IsNearlyEqual(CamLoc.Z, TargetZ, 0.01f))
		{
			CamLoc.Z = FMath::FInterpTo(CamLoc.Z, TargetZ, DeltaSeconds, CrouchCameraInterpSpeed);
			FirstPersonCamera->SetRelativeLocation(CamLoc);
		}
	}
}

// ==============================
// 入力
// ==============================

void AAimTrainerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// --- マッピングコンテキストの登録(UE5テンプレート標準タイミング) ---
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				// 再Possess等で複数回呼ばれ得るため、二重登録を避けてから登録する
				Subsystem->RemoveMappingContext(DefaultMappingContext);
				Subsystem->AddMappingContext(DefaultMappingContext, /*Priority=*/0);

				UE_LOG(LogTemp, Log, TEXT("[AimTrainer] IMC を登録しました: %s"),
					*GetNameSafe(DefaultMappingContext));
			}
			else
			{
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

	// --- 入力バインド(IMCの登録は NotifyControllerChanged で実施済み) ---
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		ReportInputConfigProblem(TEXT(
			"EnhancedInputComponent へのキャストに失敗しました。"
			"DefaultInput.ini の DefaultInputComponentClass を確認してください。"));
		return;
	}

	// 移動: 押下中は毎Tick発火する Triggered。移動計算はCMCがDeltaTimeで行うためFPS非依存。
	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAimTrainerCharacter::Input_Move);
	}
	else
	{
		ReportInputConfigProblem(TEXT("MoveAction が None です(WASD移動不可)。BP_AimTrainerCharacter で IA_Move を割り当ててください。"));
	}

	// ジャンプ: ACharacter標準関数へ直接バインド(Started=押下/Completed=解放)
	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	else
	{
		ReportInputConfigProblem(TEXT("JumpAction が None です(ジャンプ不可)。BP_AimTrainerCharacter で IA_Jump を割り当ててください。"));
	}

	// しゃがみ(ホールド式): ACharacter標準関数へ直接バインド。
	// 末尾の false は bClientSimulation(サーバー主導の複製用)。
	if (CrouchAction)
	{
		EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &ACharacter::Crouch, false);
		EIC->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ACharacter::UnCrouch, false);
	}
	else
	{
		ReportInputConfigProblem(TEXT("CrouchAction が None です(しゃがみ不可)。BP_AimTrainerCharacter で IA_Crouch を割り当ててください。"));
	}

	// 視点操作
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

	// 視点のヨーのみを基準に、水平な前方向・右方向を求める
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// 移動の「意図」だけを渡す。加速・制動・摩擦(fix3で調整)はCMCが計算する。
	AddMovementInput(ForwardDir, MoveValue.Y);
	AddMovementInput(RightDir, MoveValue.X);
}

void AAimTrainerCharacter::Input_Look(const FInputActionValue& Value)
{
	// IA_Look は Axis2D(X=マウス左右, Y=マウス上下)
	const FVector2D LookValue = Value.Get<FVector2D>();

	// 【fix3: 感度スケールについて】
	// DefaultInput.ini で
	//   ・bEnableLegacyInputScales=False(旧来のYaw×2.5/Pitch×-2.5を廃止)
	//   ・bEnableMouseSmoothing=False(スムージング廃止=生入力)
	//   ・bEnableFOVScaling=False(FOVによる感度変化を廃止)
	// とした結果、回転角 = カウント数 × 0.07(ini側のMouse2D係数) × MouseSensitivity。
	// これはVALORANTの感度式(0.07°/カウント×感度)と同一なので、
	// MouseSensitivity に普段のVALORANT感度をそのまま入力すれば振り向きが一致する。

	// ヨー: マウス右で右を向く
	AddControllerYawInput(LookValue.X * MouseSensitivity);

	// ピッチ: LegacyInputScales廃止により符号系がfix2までと逆転している点に注意。
	// 旧: Pitchに-2.5が掛かるため C++側で-1を掛けて相殺していた。
	// 新: スケール無し(+1)のため、マウス上(+Y)をそのまま足せば見上げる方向になる。
	const float PitchSign = bInvertLookY ? -1.f : 1.f;
	AddControllerPitchInput(LookValue.Y * PitchSign * MouseSensitivity);
}
