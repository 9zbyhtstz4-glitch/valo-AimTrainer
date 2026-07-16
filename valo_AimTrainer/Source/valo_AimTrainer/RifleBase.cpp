#include "RifleBase.h"
#include "WeaponDataAsset.h"
#include "TimerManager.h"

ARifleBase::ARifleBase()
{
}

void ARifleBase::StartFire()
{
	if (!WeaponData) return;

	// フルオートならタイマーで連続発射、セミオートなら1回のみ
	if (WeaponData->bAutomaticFire)
	{
		float TimeBetweenShots = 1.0f / WeaponData->FireRate;
		GetWorldTimerManager().SetTimer(FireTimerHandle, this, &ARifleBase::Fire, TimeBetweenShots, true, 0.0f);
	}
	else
	{
		Fire();
	}
}

void ARifleBase::StopFire()
{
	// フルオート射撃タイマーの停止
	GetWorldTimerManager().ClearTimer(FireTimerHandle);
}

void ARifleBase::Fire()
{
	// Phase3にてHitscan（ライントレース）とダメージ処理を実装予定
	// 現在は空の関数
}