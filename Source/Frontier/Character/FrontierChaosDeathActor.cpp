#include "Character/FrontierChaosDeathActor.h"

#include "Field/FieldSystemObjects.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Kismet/GameplayStatics.h"

AFrontierChaosDeathActor::AFrontierChaosDeathActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	GeometryCollectionComponent = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GeometryCollection"));
	SetRootComponent(GeometryCollectionComponent);
	GeometryCollectionComponent->SetGenerateOverlapEvents(false);
	GeometryCollectionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GeometryCollectionComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Ignore); // Player
	GeometryCollectionComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void AFrontierChaosDeathActor::SetDestructionOrigin(
	const FVector ImpactPoint,
	const bool bIsValidImpactPoint)
{
	DestructionOrigin = bIsValidImpactPoint ? ImpactPoint : FVector::ZeroVector;
	bHasDestructionOrigin = bIsValidImpactPoint;
}

void AFrontierChaosDeathActor::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		Destroy();
		return;
	}

	if (DebrisLifetime > 0.0f)
	{
		SetLifeSpan(DebrisLifetime);
	}

	// Reapply these responses at runtime so Blueprint component overrides cannot make debris block the player.
	GeometryCollectionComponent->SetGenerateOverlapEvents(false);
	GeometryCollectionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GeometryCollectionComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Ignore); // Player
	GeometryCollectionComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GeometryCollectionComponent->SetSimulatePhysics(true);
	GetWorldTimerManager().SetTimerForNextTick(this, &AFrontierChaosDeathActor::TriggerDestruction);
}

void AFrontierChaosDeathActor::TriggerDestruction()
{
	if (!GeometryCollectionComponent)
	{
		return;
	}

	const FVector FieldCenter = bHasDestructionOrigin
		? DestructionOrigin
		: GeometryCollectionComponent->Bounds.Origin;

	if (BreakSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BreakSound, FieldCenter);
	}

	URadialFalloff* StrainField = NewObject<URadialFalloff>(this);
	StrainField->SetRadialFalloff(
		BreakStrain,
		0.0f,
		BreakStrain,
		0.0f,
		BreakRadius,
		FieldCenter,
		EFieldFalloffType::Field_FallOff_None);
	GeometryCollectionComponent->ApplyPhysicsField(
		true,
		EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain,
		nullptr,
		StrainField);

	URadialVector* VelocityField = NewObject<URadialVector>(this);
	VelocityField->SetRadialVector(OutwardVelocity, FieldCenter);
	GeometryCollectionComponent->ApplyPhysicsField(
		true,
		EGeometryCollectionPhysicsTypeEnum::Chaos_LinearVelocity,
		nullptr,
		VelocityField);
}
