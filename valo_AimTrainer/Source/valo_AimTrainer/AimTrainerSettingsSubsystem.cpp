#include "AimTrainerSettingsSubsystem.h"

void UAimTrainerSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UAimTrainerSettingsSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

float UAimTrainerSettingsSubsystem::GetConvertedSensitivity() const
{
	// VALORANT感度 × 変換係数
	return ValorantSensitivity * ConversionMultiplier;
}

float UAimTrainerSettingsSubsystem::GetConvertedScopedSensitivity() const
{
	// スコープ時はさらにScopedSensitivityを乗算
	return GetConvertedSensitivity() * ScopedSensitivity;
}