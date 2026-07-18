#include "RifleBase.h"
#include "WeaponDataAsset.h"
#include "TimerManager.h"
#include "AimTrainerCharacter.h"
#include "Camera/CameraComponent.h"
#include "DamageableInterface.h"
#include "AimTrainerDamageTypes.h"
#include "AimTrainerStatsComponent.h" // 追加
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

ARifleBase::ARifleBase()
{
}

void ARifleBase::BeginPlay()
{
	Super::BeginPlay();

	if (WeaponData)
	{
		CurrentAmmo = WeaponData->MagazineSize;
		UE_LOG(LogTemp, Warning, TEXT("[RifleBase] Spawned. Initial Ammo: %d"), CurrentAmmo);
	}
}

void ARifleBase::StartFire()
{
	UE_LOG(LogTemp, Warning, TEXT("[RifleBase] StartFire が呼ばれました"));

	if (!WeaponData)
	{
		UE_LOG(LogTemp, Error, TEXT("[RifleBase] WeaponData (データアセット) が設定されていません！射撃を中止します"));
		return;
	}

	if (bIsReloading || CurrentAmmo <= 0)
	{
		return;
	}

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
	UE_LOG(LogTemp, Warning, TEXT("[RifleBase] Fire 処理(LineTrace)を開始します"));

	if (!WeaponData) return;
	if (bIsReloading) return;

	if (CurrentAmmo <= 0)
	{
		Reload();
		return;
	}

	CurrentAmmo--;
	UE_LOG(LogTemp, Log, TEXT("[RifleBase] Fired. Ammo: %d / %d"), CurrentAmmo, WeaponData->MagazineSize);

	AAimTrainerCharacter* Character = Cast<AAimTrainerCharacter>(GetOwner());
	if (!Character)
	{
		UE_LOG(LogTemp, Error, TEXT("[RifleBase] 所有者(Character)の取得に失敗しました"));
		return;
	}

	// 【追加】計測：射撃記録
	if (UAimTrainerStatsComponent* Stats = Character->GetStatsComponent())
	{
		Stats->RecordShot();
	}

	UCameraComponent* Camera = Character->GetFirstPersonCamera();
	if (!Camera)
	{
		UE_LOG(LogTemp, Error, TEXT("[RifleBase] カメラコンポーネントの取得に失敗しました"));
		return;
	}

	FVector StartLoc = Camera->GetComponentLocation();
	FVector ForwardVector = Camera->GetForwardVector();
	FVector EndLoc = StartLoc + (ForwardVector * 10000.f);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(Character);

	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, StartLoc, EndLoc, ECC_Visibility, QueryParams);

	if (bHit)
	{
		AActor* HitActor = HitResult.GetActor();
		FString ActorName = HitActor ? HitActor->GetName() : TEXT("Unknown");
		UE_LOG(LogTemp, Warning, TEXT("[RifleBase] 着弾しました！ ヒット対象: %s"), *ActorName);
		
		if (HitActor && HitActor->Implements<UDamageable>())
		{
			UE_LOG(LogTemp, Warning, TEXT("[RifleBase] 対象はUDamageableを持っています。ダメージ処理( %f )を送信します"), WeaponData->Damage);
			
			FAimTrainerDamageInfo DamageInfo;
			DamageInfo.BaseDamage = WeaponData->Damage;
			DamageInfo.HeadshotMultiplier = WeaponData->HeadshotMultiplier;
			DamageInfo.HitComponent = HitResult.GetComponent(); 
			DamageInfo.HitLocation = HitResult.ImpactPoint;
			DamageInfo.ShotDirection = (HitResult.ImpactPoint - StartLoc).GetSafeNormal();
			DamageInfo.Distance = HitResult.Distance;

			IDamageable::Execute_ReceiveDamage(HitActor, DamageInfo);
		}

		DrawDebugLine(GetWorld(), StartLoc, HitResult.ImpactPoint, FColor::Blue, false, 3.0f, 0, 1.0f);
		DrawDebugBox(GetWorld(), HitResult.ImpactPoint, FVector(5.f, 5.f, 5.f), FColor::Green, false, 3.0f, 0, 1.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[RifleBase] 何もヒットしませんでした"));
		DrawDebugLine(GetWorld(), StartLoc, EndLoc, FColor::Blue, false, 3.0f, 0, 1.0f);
	}

	if (CurrentAmmo <= 0)
	{
		StopFire();
		Reload();
	}
}

void ARifleBase::Reload()
{
	if (bIsReloading || !WeaponData || CurrentAmmo >= WeaponData->MagazineSize)
	{
		return;
	}

	bIsReloading = true;
	UE_LOG(LogTemp, Warning, TEXT("[RifleBase] Reloading... (%.1f seconds)"), WeaponData->ReloadTime);

	StopFire();
	GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &ARifleBase::FinishReload, WeaponData->ReloadTime, false);
}

void ARifleBase::FinishReload()
{
	if (WeaponData)
	{
		CurrentAmmo = WeaponData->MagazineSize;
	}
	
	bIsReloading = false;
	UE_LOG(LogTemp, Warning, TEXT("[RifleBase] Reload Complete. Ammo: %d"), CurrentAmmo);
}