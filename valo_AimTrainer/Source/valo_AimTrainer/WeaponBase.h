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

	// ==========================================
	// 今回の追加: リロード要求インターフェース
	// ==========================================

	/** 
	 * リロード要求を受け取る仮想関数。
	 * 理由: キャラクター側は「持っている武器の種類」を意識せず、Baseクラス経由でリロード指示を出せるようにするため。
	 *       ※弾数管理やタイマー等の実処理はここでは行わず、継承先のRifleBase側でオーバーライドして実装します。
	 */
	virtual void Reload() {}
};