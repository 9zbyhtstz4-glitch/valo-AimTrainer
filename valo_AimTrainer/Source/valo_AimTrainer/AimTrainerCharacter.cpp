#include "AimTrainerCharacter.h"
#include "AimTrainerSettingsSubsystem.h"
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
#include "AimTrainerStatsComponent.h" // 【追加】
#include "WeaponBase.h"

namespace
{
	void ReportInputConfigProblem(const FString& Message)
	{
		UE_LOG(LogTemp, Error, TEXT("[AimTrainer] %s"), *Message);
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 15.f, FColor::Red, FString::Printf(TEXT("[AimTrainer] %s"), *Message));
		}
#endif
	}
}

AAimTrainerCharacter::AAimTrainerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, StandingCameraHeight));
	FirstPersonCamera->bUsePawnControlRotation = true;

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	ApplyCharacterSettings();
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	JumpMaxCount = 1;

	TestingComponent = CreateDefaultSubobject<UMovementTestingComponent>(TEXT("MovementTesting"));
	
	// 【追加】計測用コンポーネントの生成
	StatsComponent = CreateDefaultSubobject<UAimTrainerStatsComponent>(TEXT("StatsComponent"));
}

void AAimTrainerCharacter::ApplyCharacterSettings()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MaxWalkSpeedCrouched = CrouchedSpeed;
	Movement->MaxAcceleration = MoveAcceleration;
	Movement->BrakingDecelerationWalking = MoveBrakingDeceleration;
	Movement->GroundFriction = MoveGroundFriction;
	Movement->bUseSeparateBrakingFriction = true;
	Movement->BrakingFriction = MoveBrakingFriction;
	Movement->BrakingFrictionFactor = 1.f;
	Movement->AirControl = AirControlAmount;
	Movement->JumpZVelocity = JumpZVelocity;
	Movement->GravityScale = GravityScale;
	Movement->SetCrouchedHalfHeight(CrouchedCapsuleHalfHeight);

	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetFieldOfView(CameraFOV);
	}
}

void AAimTrainerCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyCharacterSettings();

	if (DefaultWeaponClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		CurrentWeapon = GetWorld()->SpawnActor<AWeaponBase>(DefaultWeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

		if (CurrentWeapon)
		{
			CurrentWeapon->AttachToComponent(FirstPersonCamera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
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
	if (FirstPersonCamera && CameraBlendElapsed >= 0.f)
	{
		CameraBlendElapsed += DeltaSeconds;
		const float Duration = FMath::Max(CrouchTransitionTime, 0.01f);
		const float Alpha = FMath::Clamp(CameraBlendElapsed / Duration, 0.f, 1.f);
		const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Alpha, CrouchEaseExponent);
		FVector CamLoc = FirstPersonCamera->GetRelativeLocation();
		CamLoc.Z = FMath::Lerp(CameraBlendStartZ, GetTargetCameraHeight(), Eased);
		FirstPersonCamera->SetRelativeLocation(CamLoc);

		if (Alpha >= 1.f) CameraBlendElapsed = -1.f;
	}
}

void AAimTrainerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->RemoveMappingContext(DefaultMappingContext);
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void AAimTrainerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC) return;

	if (MoveAction) EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAimTrainerCharacter::Input_Move);
	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	if (CrouchAction)
	{
		EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &ACharacter::Crouch, false);
		EIC->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ACharacter::UnCrouch, false);
	}
	if (LookAction) EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AAimTrainerCharacter::Input_Look);

	if (FireAction)
	{
		EIC->BindAction(FireAction, ETriggerEvent::Started, this, &AAimTrainerCharacter::OnFireStart);
		EIC->BindAction(FireAction, ETriggerEvent::Completed, this, &AAimTrainerCharacter::OnFireStop);
	}

	if (TestingComponent)
	{
		PlayerInputComponent->BindKey(EKeys::F5, IE_Pressed, TestingComponent.Get(), &UMovementTestingComponent::StartSession);
		PlayerInputComponent->BindKey(EKeys::F6, IE_Pressed, TestingComponent.Get(), &UMovementTestingComponent::EndSession);
		PlayerInputComponent->BindKey(EKeys::F7, IE_Pressed, TestingComponent.Get(), &UMovementTestingComponent::PrintReport);
	}

	if (ReloadAction)
	{
		EIC->BindAction(ReloadAction, ETriggerEvent::Started, this, &AAimTrainerCharacter::OnReload);
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
	if (!Controller) return;

	const FVector2D LookValue = Value.Get<FVector2D>();
	float EffectiveSensitivity = 1.0f;

	// GameInstanceからSettingsSubsystemを取得し、変換後の感度を得る
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UAimTrainerSettingsSubsystem* Settings = GameInstance->GetSubsystem<UAimTrainerSettingsSubsystem>())
		{
			// 将来的なADS状態の判定に備えてメソッドを呼び分ける
			EffectiveSensitivity = Settings->GetConvertedSensitivity();
		}
	}

	AddControllerYawInput(LookValue.X * EffectiveSensitivity);

	const float PitchSign = bInvertLookY ? -1.f : 1.f;
	AddControllerPitchInput(LookValue.Y * PitchSign * EffectiveSensitivity);
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

void AAimTrainerCharacter::OnReload()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->Reload();
	}
}