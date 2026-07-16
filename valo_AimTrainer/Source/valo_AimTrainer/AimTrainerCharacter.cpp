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
#include "WeaponBase.h" // 追加: 武器クラスのインクルード

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
	Movement->MaxAcceleration = MoveAcceleration;
	Movement->BrakingDecelerationWalking = MoveBrakingDeceleration;
	Movement->GroundFriction = MoveGroundFriction;

	// 制動プロファイルの線形化(fix4)
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

	// ==============================
	// 武器スポーンとアタッチ (Phase 2 追加)
	// ==============================
	if (DefaultWeaponClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		CurrentWeapon = GetWorld()->SpawnActor<AWeaponBase>(DefaultWeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

		if (CurrentWeapon)
		{
			// キャラクターのメッシュにアタッチ
			CurrentWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("WeaponSocket"));
		}
	}
}

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

	// ==============================
	// 射撃入力の追加 (Phase 2)
	// ==============================
	if (FireAction)
	{
		EIC->BindAction(FireAction, ETriggerEvent::Started, this, &AAimTrainerCharacter::OnFireStart);
		EIC->BindAction(FireAction, ETriggerEvent::Completed, this, &AAimTrainerCharacter::OnFireStop);
	}

	// --- 計測用デバッグキー(fix6) ---
	if (TestingComponent)
	{
		PlayerInputComponent->BindKey(EKeys::F5, IE_Pressed, TestingComponent.Get(), &UMovementTestingComponent::StartSession);
		PlayerInputComponent->BindKey(EKeys::F6, IE_Pressed, TestingComponent.Get(), &UMovementTestingComponent::EndSession);
		PlayerInputComponent->BindKey(EKeys::F7, IE_Pressed, TestingComponent.Get(), &UMovementTestingComponent::PrintReport);
	}
}

void AAimTrainerCharacter::Input_Move(const FInputActionValue& Value)
{
	if (!Controller) return;

	const FVector2D MoveValue = Value.Get<FVector2D>();
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDir, MoveValue.Y);
	AddMovementInput(RightDir, MoveValue.X);
}

void AAimTrainerCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D LookValue = Value.Get<FVector2D>();
	AddControllerYawInput(LookValue.X * MouseSensitivity);
	const float PitchSign = bInvertLookY ? -1.f : 1.f;
	AddControllerPitchInput(LookValue.Y * PitchSign * MouseSensitivity);
}

// ==============================
// 武器入力の実装 (Phase 2 追加)
// ==============================
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