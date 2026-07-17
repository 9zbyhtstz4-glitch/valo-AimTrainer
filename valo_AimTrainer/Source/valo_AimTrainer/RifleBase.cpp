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
	UE_LOG(LogTemp, Warning, TEXT("[RifleBase] StartFire が呼ばれました"));

	if (!WeaponData)
	{
		UE_LOG(LogTemp, Error, TEXT("[RifleBase] WeaponData (データアセット) が設定されていません！射撃を中止します"));
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
			IDamageable::Execute_ReceiveDamage(HitActor, WeaponData->Damage);
		}

		// 判別のための青色デバッグライン（これで赤色が出たら別の処理が動いている証拠です）
		DrawDebugLine(GetWorld(), StartLoc, HitResult.ImpactPoint, FColor::Blue, false, 3.0f, 0, 1.0f);
		DrawDebugBox(GetWorld(), HitResult.ImpactPoint, FVector(5.f, 5.f, 5.f), FColor::Green, false, 3.0f, 0, 1.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[RifleBase] 何もヒットしませんでした"));
		DrawDebugLine(GetWorld(), StartLoc, EndLoc, FColor::Blue, false, 3.0f, 0, 1.0f);
	}
}