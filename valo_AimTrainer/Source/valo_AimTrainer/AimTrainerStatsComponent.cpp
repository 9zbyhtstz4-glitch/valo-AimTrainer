#include "AimTrainerStatsComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UAimTrainerStatsComponent::UAimTrainerStatsComponent()
{
	PrimaryActorTick.bCanEverTick = false;
}

void UAimTrainerStatsComponent::BeginPlay()
{
	Super::BeginPlay();
	ResetStats();
}

void UAimTrainerStatsComponent::ResetStats()
{
	ShotCount = 0;
	HitCount = 0;
	HeadshotCount = 0;
	BotSpawnTime = -1.0f;
	FirstHitTime = -1.0f;
	KillTime = -1.0f;
	bHasLandedFirstHit = false;
}

void UAimTrainerStatsComponent::RecordBotSpawn()
{
	if (UWorld* World = GetWorld())
	{
		BotSpawnTime = UGameplayStatics::GetTimeSeconds(World);
	}
	// 新しいボットが出現したため、初弾ヒットフラグとタイムスタンプをリセット
	bHasLandedFirstHit = false;
	FirstHitTime = -1.0f;
	KillTime = -1.0f;
}

void UAimTrainerStatsComponent::RecordShot()
{
	ShotCount++;
}

void UAimTrainerStatsComponent::RecordHit(bool bIsHeadshot)
{
	HitCount++;
	if (bIsHeadshot)
	{
		HeadshotCount++;
	}

	// 現在のボットに対してこれが初弾命中だった場合、時刻を記録
	if (!bHasLandedFirstHit && GetWorld())
	{
		bHasLandedFirstHit = true;
		FirstHitTime = UGameplayStatics::GetTimeSeconds(GetWorld());
	}
}

void UAimTrainerStatsComponent::RecordBotKill()
{
	if (UWorld* World = GetWorld())
	{
		KillTime = UGameplayStatics::GetTimeSeconds(World);
	}
}

float UAimTrainerStatsComponent::GetAccuracy() const
{
	if (ShotCount <= 0) return 0.0f;
	return static_cast<float>(HitCount) / static_cast<float>(ShotCount);
}

float UAimTrainerStatsComponent::GetHeadshotRate() const
{
	if (HitCount <= 0) return 0.0f;
	return static_cast<float>(HeadshotCount) / static_cast<float>(HitCount);
}

float UAimTrainerStatsComponent::GetReactionTimeMs() const
{
	if (BotSpawnTime < 0.0f || FirstHitTime < 0.0f) return 0.0f;
	
	// 秒単位の差分をミリ秒(ms)に変換
	return (FirstHitTime - BotSpawnTime) * 1000.0f;
}

float UAimTrainerStatsComponent::GetKillTime() const
{
	if (BotSpawnTime < 0.0f || KillTime < 0.0f) return 0.0f;
	
	// 出現から撃破までの時間(秒)
	return KillTime - BotSpawnTime;
}