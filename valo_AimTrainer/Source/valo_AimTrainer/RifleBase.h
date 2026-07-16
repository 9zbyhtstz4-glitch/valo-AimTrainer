#pragma once

#include "CoreMinimal.h"
#include "WeaponBase.h"
#include "RifleBase.generated.h"

UCLASS()
class VALO_AIMTRAINER_API ARifleBase : public AWeaponBase
{
	GENERATED_BODY()
	
public:
	ARifleBase();

	virtual void StartFire() override;
	virtual void StopFire() override;

protected:
	// 実際の射撃処理（ライントレース等）
	virtual void Fire();

	FTimerHandle FireTimerHandle;
};