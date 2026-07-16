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
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "MovementTestingComponent.h"

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

	// fix6: 検証コンポーネント(計測/統計/CSV/レポート)。常駐するが
	// セッション外はHUD以外何もしないため実行コストは無視できる。
	TestingComponent = CreateDefaultSubobject<UMovementTestingComponent>(TEXT("MovementTesting"));
}

void AAimTrainerCharacter::ApplyCharacterSettings()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();

	// --- 速度 ---
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MaxWalkSpeedCrouched = CrouchedSpeed;

	// --- 加速・制動(fix5: ユーザー提供録画の光学解析による実測値。ヘッダ参照) ---
	// 加速3000: 実測 20-80%=106ms / 10-90%=149ms からの逆算
	// 制動7500: 実測 解放停止 20-80%=43ms からの逆算
	// 摩擦4.5 : 実測 逆キー減速 20-80%=62ms からの逆算
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

		// InterpEaseInOut(指数1.3): 実測したVALORANTの遷移速度カーブ
		// (ピーク/平均比1.2)に合わせた緩めのS字。DeltaTimeを直接乗じないため
		// フレームレートに依らず同じ時間・同じ軌跡で完了する。
		const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Alpha, CrouchEaseExponent);

		FVector CamLoc = FirstPersonCamera->GetRelativeLocation();
		CamLoc.Z = FMath::Lerp(CameraBlendStartZ, GetTargetCameraHeight(), Eased);
		FirstPersonCamera->SetRelativeLocation(CamLoc);

		if (Alpha >= 1.f)
		{
			CameraBlendElapsed = -1.f; // 完了
		}
	}

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

	// --- 計測用デバッグキー(fix6) ---
	// 検証専用機能のため、Enhanced Inputアセットを増やさずレガシー直バインドを使う。
	// F5=計測セッション開始 / F6=終了+レポート / F7=途中レポート
	if (TestingComponent)
	{
		PlayerInputComponent->BindKey(EKeys::F5, IE_Pressed, TestingComponent.Get(), &UMovementTestingComponent::StartSession);
		PlayerInputComponent->BindKey(EKeys::F6, IE_Pressed, TestingComponent.Get(), &UMovementTestingComponent::EndSession);
		PlayerInputComponent->BindKey(EKeys::F7, IE_Pressed, TestingComponent.Get(), &UMovementTestingComponent::PrintReport);
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


#include "AimTrainerCharacter.h"
#include "WeaponBase.h" // 追加
#include "EnhancedInputComponent.h" // Enhanced Input用
#include "EnhancedInputSubsystems.h" // Enhanced Input用

// ... (既存のコンストラクタ等はそのまま) ...

void AAimTrainerCharacter::BeginPlay()
{
	Super::BeginPlay();

	/* --- 武器スポーンとアタッチの追加処理 --- */
	if (DefaultWeaponClass)
	{
		// 武器をワールドにスポーン
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		CurrentWeapon = GetWorld()->SpawnActor<AWeaponBase>(DefaultWeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

		if (CurrentWeapon)
		{
			// キャラクターのメッシュにアタッチする（ソケット名はエディタのスケルトンに合わせて調整してください）
			// ※FPS視点の場合、一人称用メッシュ(Mesh1P)等があればそちらにアタッチします。
			CurrentWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("WeaponSocket"));
		}
	}
}

void AAimTrainerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	/* --- 入力バインドの追加 (Enhanced Input) --- */
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// 射撃（左クリック）のバインド
		if (FireAction)
		{
			// クリック開始時に StartFire
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &AAimTrainerCharacter::OnFireStart);
			// クリック終了時に StopFire
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &AAimTrainerCharacter::OnFireStop);
		}
	}
}

void AAimTrainerCharacter::OnFireStart()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->StartFire();
	}
}

void AAimTrainerCharacter::OnFireStop()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->StopFire();
	}
}