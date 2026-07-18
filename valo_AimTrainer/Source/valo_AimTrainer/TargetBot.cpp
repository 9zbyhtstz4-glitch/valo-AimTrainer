// ATargetBot の実装
#include "TargetBot.h"

#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ATargetBot::ATargetBot()
{
	PrimaryActorTick.bCanEverTick = false;

	// --- Root ---
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// --- マテリアル・メッシュの取得 ---
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// ==========================================
	// Body 構成 (カプセル状の体)
	// ==========================================
	BodyCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyCollision"));
	BodyCollision->SetupAttachment(Root);
	BodyCollision->SetCapsuleSize(30.f, 50.f);
	BodyCollision->SetRelativeLocation(FVector(0.f, 0.f, 50.f)); // 地面から浮かせる
	BodyCollision->ComponentTags.Add(FName("Body")); // 【重要】部位Tag

	BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BodyCollision->SetCollisionObjectType(ECC_WorldDynamic);
	BodyCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(BodyCollision);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CylinderMesh.Succeeded()) BodyMesh->SetStaticMesh(CylinderMesh.Object);
	if (BaseMat.Succeeded()) BodyMesh->SetMaterial(0, BaseMat.Object);
	// Cylinder(100x100)をカプセル(半径30,高さ100)に合わせるスケール調整
	BodyMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.0f)); 

	// ==========================================
	// Head 構成 (球体の頭)
	// ==========================================
	HeadCollision = CreateDefaultSubobject<USphereComponent>(TEXT("HeadCollision"));
	HeadCollision->SetupAttachment(Root);
	HeadCollision->SetSphereRadius(20.f);
	HeadCollision->SetRelativeLocation(FVector(0.f, 0.f, 120.f)); // 体の上部に配置
	HeadCollision->ComponentTags.Add(FName("Head")); // 【重要】部位Tag

	HeadCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HeadCollision->SetCollisionObjectType(ECC_WorldDynamic);
	HeadCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	HeadCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(HeadCollision);
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (SphereMesh.Succeeded()) HeadMesh->SetStaticMesh(SphereMesh.Object);
	if (BaseMat.Succeeded()) HeadMesh->SetMaterial(0, BaseMat.Object);
	// Sphere(100x100)を半径20に合わせるスケール調整
	HeadMesh->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.4f));
}

void ATargetBot::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	SpawnLocation = GetActorLocation();

	// 色制御用のマテリアルインスタンスを部位ごとに作成
	if (BodyMesh) BodyMid = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
	if (HeadMesh) HeadMid = HeadMesh->CreateAndSetMaterialInstanceDynamic(0);

	FLinearColor InitialColor = GetHealthColor();
	if (BodyMid) BodyMid->SetVectorParameterValue(FName(TEXT("Color")), InitialColor);
	if (HeadMid) HeadMid->SetVectorParameterValue(FName(TEXT("Color")), InitialColor);
}

// ==============================
// 判定・計算ロジック
// ==============================

ATargetBot::ETargetHitZone ATargetBot::ResolveHitZone(UPrimitiveComponent* HitComponent) const
{
	if (!HitComponent) return ETargetHitZone::Unknown;

	if (HitComponent->ComponentHasTag(FName("Head"))) return ETargetHitZone::Head;
	if (HitComponent->ComponentHasTag(FName("Body"))) return ETargetHitZone::Body;

	return ETargetHitZone::Unknown;
}

float ATargetBot::CalculateFinalDamage(ETargetHitZone HitZone, const FAimTrainerDamageInfo& DamageInfo) const
{
	switch (HitZone)
	{
		case ETargetHitZone::Head:
			return DamageInfo.BaseDamage * DamageInfo.HeadshotMultiplier;
		case ETargetHitZone::Body:
			return DamageInfo.BaseDamage;
		default:
			return 0.0f;
	}
}

// ==============================
// 被弾(IDamageable)
// ==============================

void ATargetBot::ReceiveDamage_Implementation(const FAimTrainerDamageInfo& DamageInfo)
{
	if (bDead) return;

	UPrimitiveComponent* HitComp = DamageInfo.HitComponent.Get();
	ETargetHitZone HitZone = ResolveHitZone(HitComp);

	// 未知の部位(銃など)に当たった場合はログを出して即終了
	if (HitZone == ETargetHitZone::Unknown)
	{
		FString ActorName = GetName();
		FString CompName = HitComp ? HitComp->GetName() : TEXT("NullComponent");
		FString TagsStr = TEXT("None");
		if (HitComp && HitComp->ComponentTags.Num() > 0)
		{
			TagsStr = TEXT("");
			for (const FName& Tag : HitComp->ComponentTags)
			{
				TagsStr += Tag.ToString() + TEXT(", ");
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[TargetBot] Unknown Hit! Actor: %s, Component: %s, Tags: %s"), *ActorName, *CompName, *TagsStr);
		return;
	}

	// ダメージ計算
	float FinalDamage = CalculateFinalDamage(HitZone, DamageInfo);
	Health -= FinalDamage;

	UE_LOG(LogTemp, Log, TEXT("[TargetBot] Hit Damage=%.0f (Headshot:%d) HP=%.0f/%.0f (%s)"),
		FinalDamage, (HitZone == ETargetHitZone::Head), FMath::Max(Health, 0.f), MaxHealth, *GetName());

	// 死亡判定
	if (Health <= 0.f)
	{
		Die();
		return;
	}

	// --- 被弾フィードバック: 白フラッシュ ---
	if (BodyMid) BodyMid->SetVectorParameterValue(FName(TEXT("Color")), FLinearColor::White);
	if (HeadMid) HeadMid->SetVectorParameterValue(FName(TEXT("Color")), FLinearColor::White);

	GetWorldTimerManager().SetTimer(FlashTimerHandle, this, &ATargetBot::RestoreColor, HitFlashTime, false);
}

void ATargetBot::RestoreColor()
{
	FLinearColor CurrentColor = GetHealthColor();
	if (BodyMid) BodyMid->SetVectorParameterValue(FName(TEXT("Color")), CurrentColor);
	if (HeadMid) HeadMid->SetVectorParameterValue(FName(TEXT("Color")), CurrentColor);
}

FLinearColor ATargetBot::GetHealthColor() const
{
	if (!bShowHealthByColor) return BaseColor;
	const float Ratio = FMath::Clamp(Health / FMath::Max(MaxHealth, 1.f), 0.f, 1.f);
	return FMath::Lerp(FLinearColor::Red, BaseColor, Ratio);
}

// ==============================
// 撃破とリスポーン
// ==============================

void ATargetBot::Die()
{
	bDead = true;

	UE_LOG(LogTemp, Log, TEXT("[TargetBot] Killed (respawn in %.1fs) (%s)"), RespawnDelay, *GetName());

#if ENABLE_DRAW_DEBUG
	DrawDebugSphere(GetWorld(), GetActorLocation(), 30.f, 10, FColor::Cyan, false, 0.3f, 0, 1.5f);
#endif

	OnBotKilled.Broadcast(this);

	// Destroyせず非表示+コリジョン無効(Actor全体で制御)
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &ATargetBot::Respawn, RespawnDelay, false);
}

void ATargetBot::Respawn()
{
	Health = MaxHealth;
	bDead = false;

	FVector NewLocation = SpawnLocation;
	if (bRandomRespawnOffset)
	{
		NewLocation +=
			GetActorForwardVector() * FMath::FRandRange(-RespawnAreaExtent.X, RespawnAreaExtent.X) +
			GetActorRightVector()   * FMath::FRandRange(-RespawnAreaExtent.Y, RespawnAreaExtent.Y) +
			FVector::UpVector       * FMath::FRandRange(-RespawnAreaExtent.Z, RespawnAreaExtent.Z);
	}
	SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);

	FLinearColor CurrentColor = GetHealthColor();
	if (BodyMid) BodyMid->SetVectorParameterValue(FName(TEXT("Color")), CurrentColor);
	if (HeadMid) HeadMid->SetVectorParameterValue(FName(TEXT("Color")), CurrentColor);

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	UE_LOG(LogTemp, Log, TEXT("[TargetBot] Respawned (%s)"), *GetName());
}