#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponDataAsset.generated.h"

UCLASS()
class VALO_AIMTRAINER_API UWeaponDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// --- 基本ステータス ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float Damage = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float FireRate = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	bool bAutomaticFire = true;

	// --- 弾薬・リロード設定 ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo", meta = (ClampMin = "1"))
	int32 MagazineSize = 25;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo", meta = (ClampMin = "0.1"))
	float ReloadTime = 2.5f;

	// ==========================================
	// 今回の追加: ヘッドショット倍率
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Damage", meta = (ClampMin = "1.0"))
	float HeadshotMultiplier = 4.0f;
};