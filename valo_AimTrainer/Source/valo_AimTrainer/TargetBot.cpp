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
#include "Kismet/GameplayStatics.h"
#include "AimTrainerCharacter.h"      // 追加
#include "AimTrainerStatsComponent.h" // 追加

ATargetBot::ATargetBot()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	BodyCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyCollision"));
	BodyCollision->SetupAttachment(Root);
	BodyCollision->SetCapsuleSize(30.f, 50.f);
	BodyCollision->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	BodyCollision->ComponentTags.Add(FName("Body"));

	BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BodyCollision->SetCollisionObjectType(ECC_WorldDynamic);
	BodyCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(BodyCollision);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CylinderMesh.Succeeded()) BodyMesh->SetStaticMesh(CylinderMesh.Object);
	if (BaseMat.Succeeded()) BodyMesh->SetMaterial(0, BaseMat.Object);
	BodyMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.0f)); 

	HeadCollision = CreateDefaultSubobject<USphereComponent>(TEXT("HeadCollision"));
	HeadCollision->SetupAttachment(Root);
	HeadCollision->SetSphereRadius(20.f);
	HeadCollision->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	HeadCollision->ComponentTags.Add(FName("Head"));

	HeadCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HeadCollision->SetCollisionObjectType(ECC_WorldDynamic);
	HeadCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	HeadCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(HeadCollision);
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (SphereMesh.Succeeded()) HeadMesh->SetStaticMesh(SphereMesh.Object);
	if (BaseMat.Succeeded()) HeadMesh->SetMaterial(0, BaseMat.Object);
	HeadMesh->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.4f));
}

void ATargetBot::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	SpawnLocation = GetActorLocation();

	if (BodyMesh) BodyMid = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
	if (HeadMesh) HeadMid = HeadMesh->CreateAndSetMaterialInstanceDynamic(0);

	FLinearColor InitialColor = GetHealthColor();
	if (BodyMid) BodyMid->SetVectorParameterValue(FName(TEXT("Color")), InitialColor);
	if (HeadMid) HeadMid->SetVectorParameterValue(FName(TEXT("Color")), InitialColor);

	// 【追加】Bot出現を記録
	if (AAimTrainerCharacter* PlayerChar = Cast<AAimTrainerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		if (UAimTrainerStatsComponent* Stats = PlayerChar->GetStatsComponent()) Stats->RecordBotSpawn();
	}
}

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
		case ETargetHitZone::Head: return DamageInfo.BaseDamage * DamageInfo.HeadshotMultiplier;
		case ETargetHitZone::Body: return DamageInfo.BaseDamage;
		default: return 0.0f;
	}
}

void ATargetBot::ReceiveDamage_Implementation(const FAimTrainerDamageInfo& DamageInfo)
{
	if (bDead) return;

	UPrimitiveComponent* HitComp = DamageInfo.HitComponent.Get();
	ETargetHitZone HitZone = ResolveHitZone(HitComp);

	if (HitZone == ETargetHitZone::Unknown)
	{
		FString ActorName = GetName();
		FString CompName = HitComp ? HitComp->GetName() : TEXT("NullComponent");
		UE_LOG(LogTemp, Warning, TEXT("[TargetBot] Unknown Hit! Actor: %s, Component: %s"), *ActorName, *CompName);
		return;
	}

	float FinalDamage = CalculateFinalDamage(HitZone, DamageInfo);
	Health -= FinalDamage;

	// 【追加】被弾記録
	if (AAimTrainerCharacter* PlayerChar = Cast<AAimTrainerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		if (UAimTrainerStatsComponent* Stats = PlayerChar->GetStatsComponent())
		{
			Stats->RecordHit(HitZone == ETargetHitZone::Head);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[TargetBot] Hit Damage=%.0f (Headshot:%d) HP=%.0f/%.0f (%s)"),
		FinalDamage, (HitZone == ETargetHitZone::Head), FMath::Max(Health, 0.f), MaxHealth, *GetName());

	if (Health <= 0.f)
	{
		Die();
		return;
	}

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

void ATargetBot::Die()
{
	bDead = true;

	// 【追加】撃破記録
	if (AAimTrainerCharacter* PlayerChar = Cast<AAimTrainerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		if (UAimTrainerStatsComponent* Stats = PlayerChar->GetStatsComponent()) Stats->RecordBotKill();
	}

	UE_LOG(LogTemp, Log, TEXT("[TargetBot] Killed (respawn in %.1fs) (%s)"), RespawnDelay, *GetName());

#if ENABLE_DRAW_DEBUG
	DrawDebugSphere(GetWorld(), GetActorLocation(), 30.f, 10, FColor::Cyan, false, 0.3f, 0, 1.5f);
#endif

	OnBotKilled.Broadcast(this);
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

	// 【追加】再出現記録
	if (AAimTrainerCharacter* PlayerChar = Cast<AAimTrainerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		if (UAimTrainerStatsComponent* Stats = PlayerChar->GetStatsComponent()) Stats->RecordBotSpawn();
	}

	UE_LOG(LogTemp, Log, TEXT("[TargetBot] Respawned (%s)"), *GetName());
}