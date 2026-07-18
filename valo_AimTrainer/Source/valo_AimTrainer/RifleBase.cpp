#include "RifleBase.h"
#include "WeaponDataAsset.h"
#include "TimerManager.h"
#include "AimTrainerCharacter.h"
#include "Camera/CameraComponent.h"
#include "DamageableInterface.h"
#include "AimTrainerDamageTypes.h" // 【追加】ダメージパケットを直接生成・使用するため明示的にインクルード
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

ARifleBase::ARifleBase()
{
}

void ARifleBase::BeginPlay()
{
	Super::BeginPlay();

	// スポーン時に弾数をフルマガジンで初期化
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

	// 追加: リロード中、または弾が無い場合は射撃を開始しない
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

	// 追加: リロード中なら射撃処理をブロック
	if (bIsReloading) return;

	// 追加: 弾が無い場合は射撃失敗(即座にリロードへ移行)
	if (CurrentAmmo <= 0)
	{
		Reload();
		return;
	}

	// 追加: 発砲成功なので弾を1消費
	CurrentAmmo--;
	UE_LOG(LogTemp, Log, TEXT("[RifleBase] Fired. Ammo: %d / %d"), CurrentAmmo, WeaponData->MagazineSize);

	AAimTrainerCharacter* Character = Cast<AAimTrainerCharacter>(GetOwner());
	if (!Character)
	{
		UE_LOG(LogTemp, Error, TEXT("[RifleBase] 所有者(Character)の取得に失敗しました"));
		return;
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
			
			// 新規追加: ダメージ情報パケットの生成
			FAimTrainerDamageInfo DamageInfo;
			DamageInfo.BaseDamage = WeaponData->Damage;
			DamageInfo.HeadshotMultiplier = WeaponData->HeadshotMultiplier;
			
			// 【修正】TWeakObjectPtr<UPrimitiveComponent>への代入。TargetBot側では Get() で展開する前提
			DamageInfo.HitComponent = HitResult.GetComponent(); 
			
			DamageInfo.HitLocation = HitResult.ImpactPoint;
			DamageInfo.ShotDirection = (HitResult.ImpactPoint - StartLoc).GetSafeNormal();
			DamageInfo.Distance = HitResult.Distance;

			// 変更: 旧float引数から構造体引数へ変更
			IDamageable::Execute_ReceiveDamage(HitActor, DamageInfo);
		}

		// 判別のための青色デバッグライン
		DrawDebugLine(GetWorld(), StartLoc, HitResult.ImpactPoint, FColor::Blue, false, 3.0f, 0, 1.0f);
		DrawDebugBox(GetWorld(), HitResult.ImpactPoint, FVector(5.f, 5.f, 5.f), FColor::Green, false, 3.0f, 0, 1.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[RifleBase] 何もヒットしませんでした"));
		DrawDebugLine(GetWorld(), StartLoc, EndLoc, FColor::Blue, false, 3.0f, 0, 1.0f);
	}

	// 追加: 今回の射撃で弾が0になったら、連射タイマーを止めてオートリロード
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