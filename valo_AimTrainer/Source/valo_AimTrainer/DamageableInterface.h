#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AimTrainerDamageTypes.h"
#include "DamageableInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UDamageable : public UInterface
{
	GENERATED_BODY()
};

/**
 * ダメージを受け取るアクターが実装する共通インターフェース
 */
class VALO_AIMTRAINER_API IDamageable
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	/**
	 * ダメージ情報を受信する
	 * @param DamageInfo Weaponから送信されるダメージ情報パケット
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Damage")
	void ReceiveDamage(const FAimTrainerDamageInfo& DamageInfo);
};