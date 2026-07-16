#include "WeaponBase.h"
#include "WeaponDataAsset.h"
#include "Components/SkeletalMeshComponent.h"

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false; // 負荷軽減のため基本Tickオフ

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	RootComponent = WeaponMesh;
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();
}

void AWeaponBase::StartFire()
{
	// 派生クラスで実装
}

void AWeaponBase::StopFire()
{
	// 派生クラスで実装
}