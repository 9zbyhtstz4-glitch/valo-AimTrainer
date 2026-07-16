#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class UWeaponDataAsset;

UCLASS()
class VALO_AIMTRAINER_API AWeaponBase : public AActor
{
	GENERATED_BODY()
	
public:	
	AWeaponBase();

protected:
	virtual void BeginPlay() override;

public:	
	// 武器のパラメータデータ
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Data")
	UWeaponDataAsset* WeaponData;

	// 武器の見た目（スケルタルメッシュ）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USkeletalMeshComponent* WeaponMesh;

	// キャラクター側から呼ばれる射撃インターフェース
	virtual void StartFire();
	virtual void StopFire();
};