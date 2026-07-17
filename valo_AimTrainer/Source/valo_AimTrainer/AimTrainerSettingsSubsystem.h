#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AimTrainerSettingsSubsystem.generated.h"

UCLASS()
class VALO_AIMTRAINER_API UAimTrainerSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- 調整可能な設定パラメータ ---

	/** マウスのDPI (UI表示や将来的なRawInput計算用) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimTrainer|Sensitivity")
	float MouseDPI = 800.f;

	/** VALORANTのゲーム内感度設定値 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimTrainer|Sensitivity")
	float ValorantSensitivity = 0.3f;

	/** VALORANTのスコープ感度倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimTrainer|Sensitivity")
	float ScopedSensitivity = 1.0f;

	/** VALORANT感度をUE5のYaw/Pitch入力スケールに変換するための係数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimTrainer|Sensitivity")
	float ConversionMultiplier = 2.53f;

	// --- 計算関数 ---

	/** 最終的にUE5のAddControllerYaw/PitchInputに渡す通常時の感度を取得 */
	UFUNCTION(BlueprintCallable, Category = "AimTrainer|Sensitivity")
	float GetConvertedSensitivity() const;

	/** 最終的にUE5のAddControllerYaw/PitchInputに渡すADS時の感度を取得 */
	UFUNCTION(BlueprintCallable, Category = "AimTrainer|Sensitivity")
	float GetConvertedScopedSensitivity() const;
};