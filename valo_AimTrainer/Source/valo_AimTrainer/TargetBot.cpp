// ATargetBot の実装
#include "TargetBot.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ATargetBot::ATargetBot()
{
	// HP・被弾・リスポーンはすべてイベント駆動のためTick不要(負荷ゼロ)
	PrimaryActorTick.bCanEverTick = false;

	// --- 球体メッシュ(エンジン標準アセット使用。プロジェクトへのアセット追加なし) ---
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetRelativeScale3D(FVector(0.35f)); // 標準Sphereは直径100uu → 約35uu(頭部サイズ相当)

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(SphereMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMat(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BaseMat.Succeeded())
	{
		MeshComponent->SetMaterial(0, BaseMat.Object);
	}

	// --- 当たり判定 ---
	// 射撃トレース(ECC_Visibility)にのみ反応させる。
	// プレイヤーの移動や他のオブジェクトには干渉しない。
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionObjectType(ECC_WorldDynamic);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ATargetBot::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	SpawnLocation = GetActorLocation();

	// 色制御用のマテリアルインスタンスを作成
	// (パラメータ名 "Color" はエンジン標準 BasicShapeMaterial のもの)
	Mid = MeshComponent->CreateAndSetMaterialInstanceDynamic(0);
	if (Mid)
	{
		Mid->SetVectorParameterValue(FName(TEXT("Color")), GetHealthColor());
	}
}

// ==============================
// 被弾(IDamageable)
// ==============================

void ATargetBot::ReceiveDamage_Implementation(float DamageAmount)
{
	// 撃破済み(非表示リスポーン待ち)の間は無効。
	// ※コリジョンも無効化しているため通常は到達しないが、二重の保険とする。
	if (bDead)
	{
		return;
	}

	Health -= DamageAmount;

	UE_LOG(LogTemp, Log, TEXT("[TargetBot] Hit Damage=%.0f HP=%.0f/%.0f (%s)"),
		DamageAmount, FMath::Max(Health, 0.f), MaxHealth, *GetName());

	if (Health <= 0.f)
	{
		Die();
		return;
	}

	// --- 被弾フィードバック: 白フラッシュ → HP比率色へ戻す ---
	if (Mid)
	{
		Mid->SetVectorParameterValue(FName(TEXT("Color")), FLinearColor::White);
		GetWorldTimerManager().SetTimer(FlashTimerHandle, this,
			&ATargetBot::RestoreColor, HitFlashTime, /*bLoop=*/false);
	}
}

void ATargetBot::RestoreColor()
{
	if (Mid)
	{
		Mid->SetVectorParameterValue(FName(TEXT("Color")), GetHealthColor());
	}
}

FLinearColor ATargetBot::GetHealthColor() const
{
	if (!bShowHealthByColor)
	{
		return BaseColor;
	}
	// HP満タン=BaseColor(シアン) → 残りわずか=赤 へ線形変化
	const float Ratio = FMath::Clamp(Health / FMath::Max(MaxHealth, 1.f), 0.f, 1.f);
	return FMath::Lerp(FLinearColor::Red, BaseColor, Ratio);
}

// ==============================
// 撃破とリスポーン
// ==============================

void ATargetBot::Die()
{
	bDead = true;

	UE_LOG(LogTemp, Log, TEXT("[TargetBot] Killed (respawn in %.1fs) (%s)"),
		RespawnDelay, *GetName());

	// 撃破エフェクト(アセット不要の簡易表現: シアンの球ワイヤが一瞬広がる)
#if ENABLE_DRAW_DEBUG
	DrawDebugSphere(GetWorld(), GetActorLocation(), 30.f, 10, FColor::Cyan,
		false, 0.3f, 0, 1.5f);
#endif

	// 将来のスコア/命中率/TTK計測用フック(現状は購読者なし)
	OnBotKilled.Broadcast(this);

	// Destroyせず非表示+コリジョン無効(リスポーンで使い回す)
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	GetWorldTimerManager().SetTimer(RespawnTimerHandle, this,
		&ATargetBot::Respawn, RespawnDelay, /*bLoop=*/false);
}

void ATargetBot::Respawn()
{
	// --- 状態リセット ---
	Health = MaxHealth;
	bDead = false;

	// --- 位置決定: 同位置 or 初期位置を中心としたローカル軸ランダム範囲 ---
	FVector NewLocation = SpawnLocation;
	if (bRandomRespawnOffset)
	{
		NewLocation +=
			GetActorForwardVector() * FMath::FRandRange(-RespawnAreaExtent.X, RespawnAreaExtent.X) +
			GetActorRightVector()   * FMath::FRandRange(-RespawnAreaExtent.Y, RespawnAreaExtent.Y) +
			FVector::UpVector       * FMath::FRandRange(-RespawnAreaExtent.Z, RespawnAreaExtent.Z);
	}
	SetActorLocation(NewLocation, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

	// --- 見た目リセットと再有効化 ---
	if (Mid)
	{
		Mid->SetVectorParameterValue(FName(TEXT("Color")), GetHealthColor());
	}
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	UE_LOG(LogTemp, Log, TEXT("[TargetBot] Respawned (%s)"), *GetName());
}
