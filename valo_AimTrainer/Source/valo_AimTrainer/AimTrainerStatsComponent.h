#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AimTrainerStatsComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VALO_AIMTRAINER_API UAimTrainerStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UAimTrainerStatsComponent();

	// --- 計測用フック ---
	void RecordBotSpawn();
	void RecordShot();
	void RecordHit(bool bIsHeadshot);
	void RecordBotKill();
	
	// リセット
	void ResetStats();

	// --- 算出関数 (UIやログ用) ---
	UFUNCTION(BlueprintPure, Category = "AimTrainer|Stats")
	float GetAccuracy() const;

	UFUNCTION(BlueprintPure, Category = "AimTrainer|Stats")
	float GetHeadshotRate() const;

	UFUNCTION(BlueprintPure, Category = "AimTrainer|Stats")
	float GetReactionTimeMs() const;

	UFUNCTION(BlueprintPure, Category = "AimTrainer|Stats")
	float GetKillTime() const;

protected:
	virtual void BeginPlay() override;

private:
	// --- カウンター ---
	int32 ShotCount = 0;
	int32 HitCount = 0;
	int32 HeadshotCount = 0;

	// --- タイムスタンプ ---
	float BotSpawnTime = -1.0f;
	float FirstHitTime = -1.0f;
	float KillTime = -1.0f;

	// 現在のターゲットに対して初弾が命中したかのフラグ
	bool bHasLandedFirstHit = false;
};