#include "RifleBase.h"
#include "WeaponDataAsset.h"
#include "TimerManager.h"
#include "AimTrainerCharacter.h"
#include "Camera/CameraComponent.h"
#include "DamageableInterface.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

ARifleBase::ARifleBase()
{
}

void ARifleBase::StartFire()
{
	if (!WeaponData) return;

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
	GetWorldTimerManager().ClearTimer(FireTimerHandle);
}

void ARifleBase::Fire()
{
	if (!WeaponData) return;

	// 武器の所有者（プレイヤーキャラクター）を取得
	AAimTrainerCharacter* Character = Cast<AAimTrainerCharacter>(GetOwner());
	if (!Character) return;

	// カメラの情報を取得（画面中央から飛ばすため）
	UCameraComponent* Camera = Character->GetFirstPersonCamera();
	if (!Camera) return;

	// ライントレースの始点と終点を計算
	FVector StartLoc = Camera->GetComponentLocation();
	FVector ForwardVector = Camera->GetForwardVector();
	
	// 射程距離（とりあえず10000uu = 100m先まで）
	FVector EndLoc = StartLoc + (ForwardVector * 10000.f);

	// トレースの設定（自分自身と所有者を無視する）
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(Character);

	// 射撃（ライントレース）の実行
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult, 
		StartLoc, 
		EndLoc, 
		ECC_Visibility, // ターゲットのCollision設定に合わせて後日調整可能
		QueryParams
	);

	if (bHit)
	{
		// 当たったアクターを取得
		AActor* HitActor = HitResult.GetActor();
		
		// インターフェースを実装しているかチェックしてダメージ適用
		if (HitActor && HitActor->Implements<UDamageable>())
		{
			// インターフェース関数の呼び出し
			IDamageable::Execute_ReceiveDamage(HitActor, WeaponData->Damage);
		}

		// デバッグ表示：着弾地点まで赤線、着弾点を緑のボックスで表示（3秒間残る）
		DrawDebugLine(GetWorld(), StartLoc, HitResult.ImpactPoint, FColor::Red, false, 3.0f, 0, 1.0f);
		DrawDebugBox(GetWorld(), HitResult.ImpactPoint, FVector(5.f, 5.f, 5.f), FColor::Green, false, 3.0f, 0, 1.0f);
	}
	else
	{
		// 何も当たらなかった場合は最大距離まで赤線を表示
		DrawDebugLine(GetWorld(), StartLoc, EndLoc, FColor::Red, false, 3.0f, 0, 1.0f);
	}
}