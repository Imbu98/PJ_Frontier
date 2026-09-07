#include "AbilitySystem/Abilities/FrontierIceFloorActor.h"

#include "Character/FrontierBaseCharacter.h"
#include "Components/SphereComponent.h"
#include "Engine/OverlapResult.h"
#include "NiagaraComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AFrontierIceFloorActor::AFrontierIceFloorActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	EffectSphere = CreateDefaultSubobject<USphereComponent>(TEXT("EffectSphere"));
	SetRootComponent(EffectSphere);
	EffectSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	FloorEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FloorEffect"));
	FloorEffect->SetupAttachment(EffectSphere);
}

void AFrontierIceFloorActor::InitializeIceFloor(
	AFrontierBaseCharacter* InSourceCharacter,
	const float InRadius,
	const float InDuration,
	const float InDamageInterval,
	const float InDamageMultiplier,
	const float InSlowMultiplier,
	const FFrontierDamageRequest& InDamageRequest)
{
	SourceCharacter = InSourceCharacter;
	Radius = FMath::Max(0.0f, InRadius);
	Duration = FMath::Max(0.1f, InDuration);
	DamageInterval = FMath::Max(0.05f, InDamageInterval);
	SlowMultiplier = FMath::Clamp(InSlowMultiplier, 0.0f, 1.0f);
	DamageRequest = InDamageRequest;
	DamageRequest.DamageMultiplier = FMath::Max(0.0f, InDamageMultiplier);
	SlowSourceId = FName(*FString::Printf(TEXT("IceFloor_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
}

void AFrontierIceFloorActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshRadiusPresentation();
	if (!HasAuthority())
	{
		return;
	}

	SetLifeSpan(Duration);
	RefreshSlowedCharacters();
	ApplyPeriodicDamage();
	GetWorldTimerManager().SetTimer(SlowRefreshTimerHandle, this, &AFrontierIceFloorActor::RefreshSlowedCharacters, SlowRefreshInterval, true);
	GetWorldTimerManager().SetTimer(DamageTimerHandle, this, &AFrontierIceFloorActor::ApplyPeriodicDamage, DamageInterval, true);
}

void AFrontierIceFloorActor::OnRep_Radius()
{
	RefreshRadiusPresentation();
}

void AFrontierIceFloorActor::OnRep_Duration()
{
	RefreshRadiusPresentation();
}

void AFrontierIceFloorActor::RefreshRadiusPresentation()
{
	if (EffectSphere)
	{
		EffectSphere->SetSphereRadius(Radius);
	}
	if (FloorEffect && !NiagaraScaleParameterName.IsNone())
	{
		const float RadiusRatio = Radius / FMath::Max(NiagaraReferenceRadius, 1.0f);
		const float VisualScale = NiagaraScaleAtReferenceRadius * RadiusRatio;
		FloorEffect->SetVariableFloat(NiagaraScaleParameterName, VisualScale);
	}
	if (FloorEffect && !NiagaraDurationParameterName.IsNone())
	{
		FloorEffect->SetVariableFloat(NiagaraDurationParameterName, Duration);
	}
}

void AFrontierIceFloorActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SlowRefreshTimerHandle);
	GetWorldTimerManager().ClearTimer(DamageTimerHandle);
	RemoveAllSlowModifiers();
	Super::EndPlay(EndPlayReason);
}

void AFrontierIceFloorActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFrontierIceFloorActor, Radius);
	DOREPLIFETIME(AFrontierIceFloorActor, Duration);
}

void AFrontierIceFloorActor::GatherValidTargets(TSet<AFrontierBaseCharacter*>& OutTargets) const
{
	OutTargets.Reset();
	if (!GetWorld() || !SourceCharacter.IsValid())
	{
		return;
	}

	TArray<FOverlapResult> Results;
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_GameTraceChannel2);
	ObjectQuery.AddObjectTypesToQuery(ECC_GameTraceChannel3);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(IceFloorOverlap), false, this);
	GetWorld()->OverlapMultiByObjectType(Results, GetActorLocation(), FQuat::Identity, ObjectQuery, FCollisionShape::MakeSphere(Radius), QueryParams);

	for (const FOverlapResult& Result : Results)
	{
		AFrontierBaseCharacter* Target = Cast<AFrontierBaseCharacter>(Result.GetActor());
		if (Target && !Target->IsDead() && UFrontierDamageStatics::CanActorsDamageEachOther(SourceCharacter.Get(), Target))
		{
			OutTargets.Add(Target);
		}
	}
}

void AFrontierIceFloorActor::RefreshSlowedCharacters()
{
	TSet<AFrontierBaseCharacter*> CurrentTargets;
	GatherValidTargets(CurrentTargets);

	for (const TWeakObjectPtr<AFrontierBaseCharacter>& PreviousTarget : SlowedCharacters)
	{
		if (PreviousTarget.IsValid() && !CurrentTargets.Contains(PreviousTarget.Get()))
		{
			PreviousTarget->RemoveMovementSpeedModifier(SlowSourceId);
		}
	}

	SlowedCharacters.Reset();
	for (AFrontierBaseCharacter* Target : CurrentTargets)
	{
		Target->SetMovementSpeedModifier(SlowSourceId, SlowMultiplier);
		SlowedCharacters.Add(Target);
	}
}

void AFrontierIceFloorActor::ApplyPeriodicDamage()
{
	if (!SourceCharacter.IsValid())
	{
		return;
	}

	TSet<AFrontierBaseCharacter*> CurrentTargets;
	GatherValidTargets(CurrentTargets);
	for (AFrontierBaseCharacter* Target : CurrentTargets)
	{
		DamageRequest.DamageEventId = FGuid::NewGuid();
		UFrontierDamageStatics::ApplyDamage(SourceCharacter.Get(), Target, DamageRequest);
	}
}

void AFrontierIceFloorActor::RemoveAllSlowModifiers()
{
	if (!HasAuthority())
	{
		return;
	}
	for (const TWeakObjectPtr<AFrontierBaseCharacter>& Target : SlowedCharacters)
	{
		if (Target.IsValid())
		{
			Target->RemoveMovementSpeedModifier(SlowSourceId);
		}
	}
	SlowedCharacters.Reset();
}
