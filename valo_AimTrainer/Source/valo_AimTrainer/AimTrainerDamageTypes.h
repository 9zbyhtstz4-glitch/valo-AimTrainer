#pragma once

#include "CoreMinimal.h"
#include "AimTrainerDamageTypes.generated.h"

class UPrimitiveComponent;

/**
 * WeaponからTargetへ渡すダメージ情報の共通データパケット
 * 疎結合を維持するため、武器と的の間でこの構造体のみをやり取りする
 */
USTRUCT(BlueprintType)
struct FAimTrainerDamageInfo
{
	GENERATED_BODY()

	// 基礎ダメージ
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AimTrainer|Damage")
	float BaseDamage = 0.0f;

	// ヘッドショット時のダメージ倍率
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AimTrainer|Damage")
	float HeadshotMultiplier = 1.0f;

	// ヒットしたコンポーネント (TargetBot側がTag判定に使用)
	// ※TWeakObjectPtrはUPROPERTYでブループリントに公開できないためマクロを削除
	TWeakObjectPtr<UPrimitiveComponent> HitComponent = nullptr;

	// 着弾座標 (被弾エフェクト等に使用可能)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AimTrainer|Damage")
	FVector HitLocation = FVector::ZeroVector;

	// 射撃方向 (ノックバック等の計算用)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AimTrainer|Damage")
	FVector ShotDirection = FVector::ZeroVector;

	// 射撃距離 (将来の距離減衰計算用)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AimTrainer|Damage")
	float Distance = 0.0f;
};