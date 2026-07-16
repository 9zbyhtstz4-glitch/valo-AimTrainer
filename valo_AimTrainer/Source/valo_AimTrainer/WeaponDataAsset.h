#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponDataAsset.generated.h"

UCLASS(BlueprintType)
class VALO_AIMTRAINER_API UWeaponDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/* --- 基本射撃パラメータ --- */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Basic")
	float Damage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Basic")
	float FireRate; // 1秒間あたりの発射弾数 (Rounds per second)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Basic")
	bool bAutomaticFire; // フルオートかセミオートか

	/* --- 弾薬・リロード --- */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ammo")
	int32 MagazineSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ammo")
	float ReloadTime;

	/* --- 精度・リコイル (VALORANT系パラメータ) --- */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy")
	float FirstShotAccuracy; // 初弾の精度（誤差角）

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy")
	float Spread; // 連射時の拡散値増加量

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy")
	float RecoveryTime; // 射撃後、精度がリセットされるまでの時間

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy")
	float MovementPenalty; // 移動時の拡散ペナルティ倍率

	// 将来的にCurveVectorなどを追加し、リコイルパターンを制御予定
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil")
	// class UCurveVector* RecoilPatternCurve; 
};