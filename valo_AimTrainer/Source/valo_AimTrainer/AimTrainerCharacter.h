#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AimTrainerCharacter.generated.h"

class UCameraComponent;
class UMovementTestingComponent;
class UInputAction;
class UInputMappingContext;
class AWeaponBase;
struct FInputActionValue;

UCLASS(config = Game)
class VALO_AIMTRAINER_API AAimTrainerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAimTrainerCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
	bool IsCameraBlending() const { return CameraBlendElapsed >= 0.f; }
	float GetTargetCameraHeight() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AWeaponBase> DefaultWeaponClass;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	AWeaponBase* CurrentWeapon;

protected:
	virtual void BeginPlay() override;
	void ApplyCharacterSettings();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AimTrainer|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AimTrainer|Testing")
	TObjectPtr<UMovementTestingComponent> TestingComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "1.0"))
	float WalkSpeed = 540.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "1.0"))
	float CrouchedSpeed = 270.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "1.0"))
	float MoveAcceleration = 3000.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "0.0"))
	float MoveBrakingDeceleration = 7500.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "0.0"))
	float MoveGroundFriction = 4.5f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "0.0"))
	float MoveBrakingFriction = 0.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirControlAmount = 0.15f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Jump", meta = (ClampMin = "1.0"))
	float JumpZVelocity = 370.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Jump", meta = (ClampMin = "0.1"))
	float GravityScale = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch", meta = (ClampMin = "30.0"))
	float CrouchedCapsuleHalfHeight = 60.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch")
	float StandingCameraHeight = 64.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch")
	float CrouchedCameraHeight = 50.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch", meta = (ClampMin = "0.01"))
	float CrouchTransitionTime = 0.30f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch", meta = (ClampMin = "1.0", ClampMax = "3.0"))
	float CrouchEaseExponent = 1.3f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Look")
	bool bInvertLookY = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Look", meta = (ClampMin = "60.0", ClampMax = "140.0"))
	float CameraFOV = 103.f;

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void OnFireStart();
	void OnFireStop();

private:
	void StartCameraHeightBlend();

	float CameraBlendStartZ = 0.f;
	float CameraBlendElapsed = -1.f;
};