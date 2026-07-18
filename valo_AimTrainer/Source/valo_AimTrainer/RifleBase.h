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

	// ==========================================
	// 今回の追加: リロード処理のオーバーライド
	// ==========================================
	/** WeaponBaseからのリロード要求を受け取る実処理 */
	virtual void Reload() override;

protected:
	// 実際の射撃処理（ライントレース等）
	virtual void Fire();

	FTimerHandle FireTimerHandle;

	// ==========================================
	// 今回の追加: 初期化処理
	// ==========================================
	/** スポーン時に弾数を最大値(フルマガジン)で初期化するため */
	virtual void BeginPlay() override;

private:
	// ==========================================
	// 今回の追加: 弾数管理とリロード用変数
	// ==========================================
	/** 現在の残弾数 */
	int32 CurrentAmmo = 0;

	/** リロード中かどうかのフラグ (射撃をブロックするため) */
	bool bIsReloading = false;

	/** リロード待機用のタイマーハンドル */
	FTimerHandle ReloadTimerHandle;

	/** タイマー完了時に呼ばれる実回復処理 */
	void FinishReload();
};