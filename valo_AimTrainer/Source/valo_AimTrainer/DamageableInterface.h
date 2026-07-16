#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DamageableInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UDamageable : public UInterface
{
	GENERATED_BODY()
};

class VALO_AIMTRAINER_API IDamageable
{
	GENERATED_BODY()

public:
	// BlueprintとC++の両方で実装可能なダメージ処理関数
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Damage")
	void ReceiveDamage(float DamageAmount);
};