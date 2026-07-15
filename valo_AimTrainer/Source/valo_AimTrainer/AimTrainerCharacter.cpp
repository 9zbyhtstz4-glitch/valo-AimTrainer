// AimTrainerCharacter の実装
#include "AimTrainerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

// 移動応答の実測HUD。コンソールで「at.MoveDebug 1」で有効化。
// VALORANT側の実測(録画のフレーム数え等)と突き合わせるための計測ツール。
static TAutoConsoleVariable<int32> CVarAimTrainerMoveDebug(
	TEXT("at.MoveDebug"),
	0,
	TEXT("1=AimTrainerの移動計測HUDを表示(0->最高速 / キー解放->停止 / 逆キー->停止 の各ms)"),
	ECVF_Default);

namespace
{
	/** 入力セットアップの設定不備を「ログ」と「画面」の両方へ確実に出す */
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
	// しゃがみカメラのブレンドと計測HUDのためTickを有効化(処理は軽量)
	PrimaryActorTick.bCanEverTick = true;

	// --- カプセル: 立ち 半径42 / 半分高さ96(全高192cm) ---
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// --- FPS視点カメラ: 立ち目線=地上160uu ---
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, StandingCameraHeight));
	FirstPersonCamera->bUsePawnControlRotation = true;

	// --- 回転設定 ---
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// --- チューニング値の適用 ---
	ApplyCharacterSettings();

	// しゃがみ有効化(立てないと Crouch() が無視される)
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;

	// 多段ジャンプ禁止
	JumpMaxCount = 1;
}

void AAimTrainerCharacter::ApplyCharacterSettings()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();

	// --- 速度 ---
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MaxWalkSpeedCrouched = CrouchedSpeed;

	// --- 加速・制動(fix4の中核。根拠はヘッダの各プロパティコメント参照) ---
	// 加速8192: 0.066秒で最高速(「押した瞬間ほぼ最高速」の証言に整合)
	// 制動8192: 解放でも0.066秒で停止 → 逆キーとの差は約20msとなり
	//           「カウンターストレイフの短縮は数ms程度」の証言に整合
	Movement->MaxAcceleration = MoveAcceleration;
	Movement->BrakingDecelerationWalking = MoveBrakingDeceleration;
	Movement->GroundFriction = MoveGroundFriction;

	// 制動プロファイルの線形化(fix4):
	// BrakingFriction=0 とし速度比例項を排除 → 一定減速のみの直線的な停止になり、
	// 「短い滑り→ピタッと停止」というVALORANTの見た目に一致させる。
	// BrakingFrictionFactor は1に固定し、MoveBrakingFriction の値がそのまま効くようにする。
	Movement->bUseSeparateBrakingFriction = true;
	Movement->BrakingFriction = MoveBrakingFriction;
	Movement->BrakingFrictionFactor = 1.f;

	// --- 空中 ---
	Movement->AirControl = AirControlAmount;

	// --- ジャンプ・重力(fix4では変更なし) ---
	Movement->JumpZVelocity = JumpZVelocity;
	Movement->GravityScale = GravityScale;

	// --- しゃがみカプセル ---
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

	// Blueprintやインスタンスで編集されたプロパティを実行時コンポーネントへ反映
	ApplyCharacterSettings();
}

// ==============================
// しゃがみカメラ: 固定時間ブレンド(fix4)
// ==============================
// fix3のFInterpTo(指数補間)は終端が漸近するため「いつ完了したか」が曖昧で、
// VALORANTの「一定時間で必ず完了する」しゃがみモーションと質感が異なった。
// fix4では (1)OnStart/OnEndCrouchでカプセル中心移動をカメラ相対Zへ即時打ち消し
//          (2)固定時間(CrouchTransitionTime)+SmoothStepのS字カーブで目標へ移行
// とする。途中でしゃがみ↔立ちが反転しても、現在位置を起点に再ブレンドする。
// カプセル(当たり判定)は従来どおり即時変形のため、天井判定等の挙動は不変。

float AAimTrainerCharacter::GetTargetCameraHeight() const
{
	return bIsCrouched ? CrouchedCameraHeight : StandingCameraHeight;
}

void AAimTrainerCharacter::StartCameraHeightBlend()
{
	if (FirstPersonCamera)
	{
		CameraBlendStartZ = FirstPersonCamera->GetRelativeLocation().Z;
		CameraBlendElapsed = 0.f;
	}
}

void AAimTrainerCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// カプセル中心が ScaledHalfHeightAdjust 下がったぶんを相対Zへ足し、
	// 視点のワールド高さを一瞬も変化させない(その後ブレンドで沈む)
	if (FirstPersonCamera)
	{
		FVector CamLoc = FirstPersonCamera->GetRelativeLocation();
		CamLoc.Z += ScaledHalfHeightAdjust;
		FirstPersonCamera->SetRelativeLocation(CamLoc);
	}
	StartCameraHeightBlend();
}

void AAimTrainerCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// カプセル中心が上がったぶんを逆に引いて視点高さを維持(その後ブレンドで立つ)
	if (FirstPersonCamera)
	{
		FVector CamLoc = FirstPersonCamera->GetRelativeLocation();
		CamLoc.Z -= ScaledHalfHeightAdjust;
		FirstPersonCamera->SetRelativeLocation(CamLoc);
	}
	StartCameraHeightBlend();
}

void AAimTrainerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// --- しゃがみカメラの固定時間ブレンド ---
	if (FirstPersonCamera && CameraBlendElapsed >= 0.f)
	{
		CameraBlendElapsed += DeltaSeconds;

		const float Duration = FMath::Max(CrouchTransitionTime, 0.01f);
		const float Alpha = FMath::Clamp(CameraBlendElapsed / Duration, 0.f, 1.f);

		// SmoothStep: 入り/抜きが滑らかなS字。DeltaTimeを直接乗じないため
		// フレームレートに依らず同じ時間・同じ軌跡で完了する。
		const float Eased = FMath::SmoothStep(0.f, 1.f, Alpha);

		FVector CamLoc = FirstPersonCamera->GetRelativeLocation();
		CamLoc.Z = FMath::Lerp(CameraBlendStartZ, GetTargetCameraHeight(), Eased);
		FirstPersonCamera->SetRelativeLocation(CamLoc);

		if (Alpha >= 1.f)
		{
			CameraBlendElapsed = -1.f; // 完了
		}
	}

	// --- 移動計測HUD(at.MoveDebug 1 のときのみ) ---
	UpdateMovementDebug();
}

// ==============================
// 移動計測HUD(fix4)
// ==============================
// VALORANTとの差分を「体感」でなく「ミリ秒」で比較するための計測ツール。
// 表示値をVALORANT側の実測(高fps録画のフレーム数え等)と突き合わせ、
// MoveAcceleration / MoveBrakingDeceleration を調整する運用を想定。

void AAimTrainerCharacter::UpdateMovementDebug()
{
#if !UE_BUILD_SHIPPING
	if (CVarAimTrainerMoveDebug.GetValueOnGameThread() == 0 || !GEngine || !GetWorld())
	{
		return;
	}

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const FVector Accel = Movement->GetCurrentAcceleration();
	FVector Vel = GetVelocity();
	Vel.Z = 0.f;

	const float Speed = Vel.Size();
	const float TopSpeed = FMath::Max(Movement->MaxWalkSpeed, 1.f);
	const float Now = GetWorld()->GetTimeSeconds();
	const bool bHasInput = !Accel.IsNearlyZero();

	// 現在速度(キー固定=毎フレーム上書き表示)
	GEngine->AddOnScreenDebugMessage(9001, 0.f, FColor::Cyan,
		FString::Printf(TEXT("[MoveDebug] Speed %4.0f uu/s (%3.0f%%)"), Speed, Speed / TopSpeed * 100.f));

	// --- 計測1: 入力開始 → 最高速95%到達 ---
	if (bHasInput && !bDbgPrevHasInput && Speed < 30.f)
	{
		DbgAccelStartTime = Now;
	}
	if (!bHasInput)
	{
		DbgAccelStartTime = -1.f; // 途中でキーを離したら計測破棄
	}
	if (DbgAccelStartTime >= 0.f && Speed >= TopSpeed * 0.95f)
	{
		GEngine->AddOnScreenDebugMessage(9002, 5.f, FColor::Green,
			FString::Printf(TEXT("[MoveDebug] 0 -> 95%%最高速 : %4.0f ms"), (Now - DbgAccelStartTime) * 1000.f));
		DbgAccelStartTime = -1.f;
	}

	// --- 計測2: キー解放 → 停止 ---
	if (!bHasInput && bDbgPrevHasInput && Speed > TopSpeed * 0.5f)
	{
		DbgBrakeStartTime = Now;
	}
	if (bHasInput)
	{
		DbgBrakeStartTime = -1.f; // 制動中に再入力したら計測破棄
	}
	if (DbgBrakeStartTime >= 0.f && Speed <= 5.f)
	{
		GEngine->AddOnScreenDebugMessage(9003, 5.f, FColor::Yellow,
			FString::Printf(TEXT("[MoveDebug] キー解放 -> 停止 : %4.0f ms"), (Now - DbgBrakeStartTime) * 1000.f));
		DbgBrakeStartTime = -1.f;
	}

	// --- 計測3: 逆キー入力 → 速度反転点(カウンターストレイフの停止時間) ---
	const FVector VelDir = Vel.GetSafeNormal();
	const FVector AccelDir = Accel.GetSafeNormal();
	const float MoveDot = FVector::DotProduct(VelDir, AccelDir);

	if (DbgFlipStartTime < 0.f && bHasInput && Speed > TopSpeed * 0.5f && MoveDot < -0.7f)
	{
		DbgFlipStartTime = Now; // ほぼ真逆の入力を検出
	}
	if (DbgFlipStartTime >= 0.f)
	{
		if (!bHasInput)
		{
			DbgFlipStartTime = -1.f; // キーを離したら計測破棄(計測2に該当)
		}
		else if (Speed <= 20.f || MoveDot > 0.2f)
		{
			// 速度がほぼゼロ、または速度が入力方向へ反転した時点=射撃可能になる瞬間
			GEngine->AddOnScreenDebugMessage(9004, 5.f, FColor::Orange,
				FString::Printf(TEXT("[MoveDebug] 逆キー -> 停止 : %4.0f ms"), (Now - DbgFlipStartTime) * 1000.f));
			DbgFlipStartTime = -1.f;
		}
	}

	bDbgPrevHasInput = bHasInput;
#endif
}

// ==============================
// 入力
// ==============================

void AAimTrainerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
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

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		ReportInputConfigProblem(TEXT(
			"EnhancedInputComponent へのキャストに失敗しました。"
			"DefaultInput.ini の DefaultInputComponentClass を確認してください。"));
		return;
	}

	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAimTrainerCharacter::Input_Move);
	}
	else
	{
		ReportInputConfigProblem(TEXT("MoveAction が None です(WASD移動不可)。BP_AimTrainerCharacter で IA_Move を割り当ててください。"));
	}

	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	else
	{
		ReportInputConfigProblem(TEXT("JumpAction が None です(ジャンプ不可)。BP_AimTrainerCharacter で IA_Jump を割り当ててください。"));
	}

	if (CrouchAction)
	{
		EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &ACharacter::Crouch, false);
		EIC->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ACharacter::UnCrouch, false);
	}
	else
	{
		ReportInputConfigProblem(TEXT("CrouchAction が None です(しゃがみ不可)。BP_AimTrainerCharacter で IA_Crouch を割り当ててください。"));
	}

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

	const FVector2D MoveValue = Value.Get<FVector2D>();

	// 視点のヨーのみを基準に、水平な前方向・右方向を求める
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// 移動の「意図」だけを渡す。加速・制動・摩擦(fix4で調整)はCMCがDeltaTimeで計算する
	AddMovementInput(ForwardDir, MoveValue.Y);
	AddMovementInput(RightDir, MoveValue.X);
}

void AAimTrainerCharacter::Input_Look(const FInputActionValue& Value)
{
	// 【感度式(fix3で確定・fix4検証済み)】
	// 回転角 = マウスカウント × 0.07(DefaultInput.iniのMouse2D係数) × MouseSensitivity
	// これはVALORANTの感度式と同一。Enhanced Inputが渡す値は
	// 「前フレームからのカウント差分の合計」であり、ここでDeltaTimeを乗じないため
	// 総回転角はフレームレートに依存しない(60fpsでも240fpsでも同じ距離=同じ角度)。
	// スムージング(bEnableMouseSmoothing=False)とFOVスケーリングも無効化済み。
	const FVector2D LookValue = Value.Get<FVector2D>();

	AddControllerYawInput(LookValue.X * MouseSensitivity);

	const float PitchSign = bInvertLookY ? -1.f : 1.f;
	AddControllerPitchInput(LookValue.Y * PitchSign * MouseSensitivity);
}
