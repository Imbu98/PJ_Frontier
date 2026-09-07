#include "Warning/AttackWarningActor.h"

#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Frontier.h"
#include "NiagaraComponent.h"

AAttackWarningActor::AAttackWarningActor()
{
	

	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DecalComponent = CreateDefaultSubobject<UDecalComponent>(TEXT("DecalComponent"));
	DecalComponent->SetupAttachment(SceneRoot);
	DecalComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	DecalComponent->SetVisibility(false);

	NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComponent"));
	NiagaraComponent->SetupAttachment(SceneRoot);
	NiagaraComponent->SetAutoActivate(false);
	NiagaraComponent->SetVisibility(false);

	SetActorHiddenInGame(true);
}

void AAttackWarningActor::ActivateWarning(const FAttackWarningData& WarningData)
{
	FRONTIER_LOG(Log, TEXT("Activating attack warning. Actor=%s Location=%s Radius=%.2f Duration=%.2f Shape=%d"),
		*GetNameSafe(this),
		*WarningData.WarningLocation.ToCompactString(),
		WarningData.WarningRadius,
		WarningData.WarningDuration,
		static_cast<uint8>(WarningData.WarningShapeType));

	SetActorLocationAndRotation(WarningData.WarningLocation, WarningData.WarningRotation);
	SetActorHiddenInGame(false);

	if (DecalComponent)
	{
		DecalComponent->SetDecalMaterial(WarningData.WarningDecalMaterial);
		DecalComponent->DecalSize = FVector(DecalProjectionDepth, WarningData.WarningRadius, WarningData.WarningRadius);
		DecalComponent->SetVisibility(WarningData.WarningShapeType == EAttackWarningShapeType::Circle && WarningData.WarningDecalMaterial != nullptr);
	}

	if (NiagaraComponent)
	{
		NiagaraComponent->SetAsset(WarningData.WarningNiagaraSystem);
		const float NiagaraReferenceRadius = FMath::Max(WarningData.WarningNiagaraReferenceRadius, 1.0f);
		NiagaraComponent->SetWorldScale3D(
			FVector(FMath::Max(WarningData.WarningRadius / NiagaraReferenceRadius, 0.01f)));
		NiagaraComponent->SetVisibility(WarningData.WarningNiagaraSystem != nullptr);
		if (WarningData.WarningNiagaraSystem)
		{
			NiagaraComponent->Activate(true);
		}
	}
}

void AAttackWarningActor::DeactivateWarning()
{
	FRONTIER_LOG(Log, TEXT("Deactivating attack warning. Actor=%s"), *GetNameSafe(this));

	if (DecalComponent)
	{
		DecalComponent->SetVisibility(false);
	}

	if (NiagaraComponent)
	{
		NiagaraComponent->Deactivate();
		NiagaraComponent->SetVisibility(false);
	}

	SetActorHiddenInGame(true);
	SetActorLocation(FVector::ZeroVector);
}
