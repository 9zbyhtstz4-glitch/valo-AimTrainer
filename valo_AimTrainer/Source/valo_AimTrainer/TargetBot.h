// ATargetBot: エイム練習用ターゲットBot(Phase2)
// 役割: HP管理 / 被弾フィードバック / 撃破 / リスポーン。
// 被弾はプロジェクト既存の IDamageable(BlueprintNativeEvent)経由で受け取るため、
// 射撃側(RifleBase)の変更は一切不要。配置した瞬間からダメージが流れる。
//
// リスポーン方式: Destroyせず「非表示化→リセット→再表示」。
//  - スポナー管理クラスなしで完結する
//  - レベルへ手動配置したBotをPIE中ずっと使い回せる
//  - 将来スポナー/ランダム配置を導入する際も本リセット機構をそのまま呼べる
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DamageableInterface.h"
#include "AimTrainerDamageTypes.h" // 【追加】ダメージ情報パケット用
#include "TargetBot.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class USceneComponent;       // 【追加】
class UCapsuleComponent;     // 【追加】
class USphereComponent;      // 【追加】

class ATargetBot;

/** 撃破通知(将来のスコア/命中率/TTK計測がここへ接続する。現状は購読者なし) */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBotKilledSignature, ATargetBot* /*Bot*/);

UCLASS()
class VALO_AIMTRAINER_API ATargetBot : public AActor, public IDamageable
{
	GENERATED_BODY()

public:
	ATargetBot();

	//~ Begin IDamageable interface
	/** 被弾処理(RifleBaseの Execute_ReceiveDamage から呼ばれる) */
	// 【変更】floatから FAimTrainerDamageInfo の参照へ変更
	virtual void ReceiveDamage_Implementation(const FAimTrainerDamageInfo& DamageInfo) override;
	//~ End IDamageable interface

	/** 撃破通知デリゲート(ネイティブ用) */
	FOnBotKilledSignature OnBotKilled;

	/** 現在HP(HUDや計測から参照可能) */
	UFUNCTION(BlueprintCallable, Category = "TargetBot")
	float GetHealth() const { return Health; }

	/** 生存中か(非表示リスポーン待ちはfalse) */
	UFUNCTION(BlueprintCallable, Category = "TargetBot")
	bool IsAlive() const { return !bDead; }

protected:
	virtual void BeginPlay() override;

	// ==============================
	// コンポーネント (Head/Body分離)
	// ==============================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TargetBot|Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TargetBot|Components")
	TObjectPtr<UCapsuleComponent> BodyCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TargetBot|Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TargetBot|Components")
	TObjectPtr<USphereComponent> HeadCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TargetBot|Components")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	// ==============================
	// 調整パラメータ(すべてBPで変更可能)
	// ==============================

	/** 最大HP。既定100=ヴァンダル(40dmg)3発で撃破(VALORANTレンジのボット相当) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TargetBot", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	/** 撃破から再出現までの秒数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TargetBot", meta = (ClampMin = "0.1"))
	float RespawnDelay = 1.0f;

	/** trueで初期位置を中心とした範囲内のランダム位置に再出現する */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TargetBot|Respawn")
	bool bRandomRespawnOffset = false;

	/** ランダム再出現の範囲(アクターローカル軸の±半径。X=前後/Y=左右/Z=上下) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TargetBot|Respawn",
		meta = (EditCondition = "bRandomRespawnOffset"))
	FVector RespawnAreaExtent = FVector(0.f, 300.f, 150.f);

	/** 通常色(VALORANT練習ボット風のシアン) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TargetBot|Visual")
	FLinearColor BaseColor = FLinearColor(0.f, 1.f, 0.9f);

	/** 被弾フラッシュ(白)の表示秒数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TargetBot|Visual", meta = (ClampMin = "0.01"))
	float HitFlashTime = 0.08f;

	/** HP残量に応じて色をシアン→赤へ変化させる(HPの視覚化) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TargetBot|Visual")
	bool bShowHealthByColor = true;

private:
	// ==============================
	// 部位判定および計算用内部定義
	// ==============================

	enum class ETargetHitZone : uint8
	{
		Unknown,
		Body,
		Head
	};

	/** 
	 * HitComponentのTagを解析し、HitZone Enumを返す純粋な関数 
	 * 副作用・ログ出力は持たない
	 */
	ETargetHitZone ResolveHitZone(UPrimitiveComponent* HitComponent) const;

	/** 
	 * HitZoneとDamageInfoから最終ダメージを算出する純粋な計算関数 
	 * 副作用・ログ出力は持たない
	 */
	float CalculateFinalDamage(ETargetHitZone HitZone, const FAimTrainerDamageInfo& DamageInfo) const;

	// ==============================
	// 内部処理(細分化)
	// ==============================

	/** 撃破処理: 非表示化+通知+リスポーン予約 */
	void Die();

	/** 再出現処理: HP全快・色/位置リセット・再表示 */
	void Respawn();

	/** 被弾フラッシュを終了し、HP比率に応じた色へ戻す */
	void RestoreColor();

	/** 現在HPに対応する表示色を返す(bShowHealthByColor=falseなら常にBaseColor) */
	FLinearColor GetHealthColor() const;

	// 【変更】単一のMidを、明示的な2変数管理(BodyMid, HeadMid)へ変更
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BodyMid;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HeadMid;

	/** 現在HP */
	float Health = 100.f;

	/** 撃破済み(リスポーン待ち)フラグ。死亡中の多重被弾を無効化する */
	bool bDead = false;

	/** 初期位置(リスポーン基準点)。BeginPlayで記録 */
	FVector SpawnLocation = FVector::ZeroVector;

	FTimerHandle RespawnTimerHandle;
	FTimerHandle FlashTimerHandle;
};