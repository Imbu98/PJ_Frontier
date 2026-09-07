#include "Extraction/FrontierExtractionZoneActor.h"

#include "Character/FrontierPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Frontier.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierGameMode.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "NiagaraComponent.h"
#include "Net/UnrealNetwork.h"

AFrontierExtractionZoneActor::AFrontierExtractionZoneActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);

	if (MeshComponent)
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetVisibility(false, true);
		MeshComponent->SetHiddenInGame(true, true);
	}

	ExtractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ExtractionBox"));
	ExtractionBox->SetupAttachment(SceneRoot);
	ExtractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ExtractionBox->SetCollisionProfileName(TEXT("Trigger"));
	ExtractionBox->SetGenerateOverlapEvents(true);
}

bool AFrontierExtractionZoneActor::CanInteract(const AFrontierPlayerController* InteractingController) const
{
	(void)InteractingController;
	// Extraction is driven exclusively by overlap duration. The zone must never be
	// selected as an F-interaction target.
	return false;
}

void AFrontierExtractionZoneActor::Interacted(AFrontierPlayerController* InteractingController)
{
}

FText AFrontierExtractionZoneActor::GetInteractionPromptText(const AFrontierPlayerController* InteractingController) const
{
	return FText::FromString(TEXT("Stay inside to extract"));
}

FVector AFrontierExtractionZoneActor::GetInteractionWorldLocation() const
{
	return ExtractionBox ? ExtractionBox->GetComponentLocation() : GetActorLocation();
}

void AFrontierExtractionZoneActor::GetInteractionHighlightComponents(TArray<UPrimitiveComponent*>& OutComponents) const
{
	OutComponents.Reset();
}

bool AFrontierExtractionZoneActor::IsActorInsideExtractionZone(const AActor* Actor) const
{
	return Actor && ExtractionBox && ExtractionBox->IsOverlappingActor(Actor);
}

float AFrontierExtractionZoneActor::GetLocalProgressForCharacter(const AFrontierPlayerCharacter* PlayerCharacter) const
{
	if (!PlayerCharacter || PlayerCharacter->IsDead() || !IsActorInsideExtractionZone(PlayerCharacter)
		|| ExtractionStartTime < 0.0f)
	{
		return 0.0f;
	}

	const float CurrentTimeSeconds = GetCurrentWorldTimeSeconds();
	return RequiredSafeDuration > 0.0f
		? FMath::Clamp((CurrentTimeSeconds - ExtractionStartTime) / RequiredSafeDuration, 0.0f, 1.0f)
		: 1.0f;
}

void AFrontierExtractionZoneActor::BeginPlay()
{
	Super::BeginPlay();

	if (!ExtractionBox)
	{
		SetActorTickEnabled(false);
		return;
	}

	ExtractionBox->OnComponentBeginOverlap.AddDynamic(this, &AFrontierExtractionZoneActor::HandleExtractionBoxBeginOverlap);
	ExtractionBox->OnComponentEndOverlap.AddDynamic(this, &AFrontierExtractionZoneActor::HandleExtractionBoxEndOverlap);

	UpdateNiagaraEffect();
}

void AFrontierExtractionZoneActor::SetExtractionNiagaraComponent(UNiagaraComponent* InNiagaraComponent)
{
	if (NiagaraComponent && NiagaraComponent != InNiagaraComponent)
	{
		NiagaraComponent->DeactivateImmediate();
		NiagaraComponent->SetVisibility(false, true);
	}

	NiagaraComponent = InNiagaraComponent;
	bNiagaraEffectRunning = false;
	if (NiagaraComponent)
	{
		// The extraction zone owns activation so the emitter age cannot accumulate
		// while the effect is hidden and no players occupy the zone.
		NiagaraComponent->SetAutoActivate(false);
	}
	UpdateNiagaraEffect();
}

void AFrontierExtractionZoneActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if ((CurrentTimeSeconds - LastEvaluationTime) < EvaluationInterval)
	{
		return;
	}

	LastEvaluationTime = CurrentTimeSeconds;
	EvaluateCandidates();
	UpdateNiagaraEffect();
}

void AFrontierExtractionZoneActor::HandleExtractionBoxBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	AddOrRefreshCandidate(Cast<AFrontierPlayerCharacter>(OtherActor));
}

void AFrontierExtractionZoneActor::HandleExtractionBoxEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;

	RemoveCandidate(Cast<AFrontierPlayerCharacter>(OtherActor));
}

void AFrontierExtractionZoneActor::AddOrRefreshCandidate(AFrontierPlayerCharacter* PlayerCharacter)
{
	if (!HasAuthority() || !PlayerCharacter || PlayerCharacter->IsDead())
	{
		return;
	}

	AController* PlayerController = PlayerCharacter->GetController();
	if (!PlayerController)
	{
		return;
	}

	for (FFrontierExtractionCandidate& Candidate : ExtractionCandidates)
	{
		if (Candidate.PlayerCharacter == PlayerCharacter)
		{
			return;
		}
	}

	FFrontierExtractionCandidate& NewCandidate = ExtractionCandidates.AddDefaulted_GetRef();
	NewCandidate.PlayerCharacter = PlayerCharacter;
}

void AFrontierExtractionZoneActor::RemoveCandidate(AFrontierPlayerCharacter* PlayerCharacter)
{
	if (!HasAuthority() || !PlayerCharacter)
	{
		return;
	}

	// A character can generate multiple overlap events. Only remove it once it is
	// no longer overlapping the zone with any component.
	if (ExtractionBox && ExtractionBox->IsOverlappingActor(PlayerCharacter))
	{
		return;
	}

	ExtractionCandidates.RemoveAll(
		[PlayerCharacter](const FFrontierExtractionCandidate& Candidate)
		{
			return Candidate.PlayerCharacter == PlayerCharacter;
		});

	if (ExtractionCandidates.IsEmpty())
	{
		SetExtractionStartTime(-1.0f);
		SetHasOccupants(false);
	}
}

void AFrontierExtractionZoneActor::EvaluateCandidates()
{
	if (!HasAuthority() || !ExtractionBox)
	{
		return;
	}

	const float CurrentTimeSeconds = GetCurrentWorldTimeSeconds();
	TArray<FFrontierExtractionCandidate> ValidCandidates;
	ValidCandidates.Reserve(ExtractionCandidates.Num());

	for (const FFrontierExtractionCandidate& Candidate : ExtractionCandidates)
	{
		AFrontierPlayerCharacter* PlayerCharacter = Candidate.PlayerCharacter.Get();
		if (PlayerCharacter && !PlayerCharacter->IsDead()
			&& PlayerCharacter->GetController() && ExtractionBox->IsOverlappingActor(PlayerCharacter))
		{
			ValidCandidates.Add(Candidate);
		}
	}

	ExtractionCandidates = MoveTemp(ValidCandidates);
	if (ExtractionCandidates.IsEmpty())
	{
		SetExtractionStartTime(-1.0f);
		SetHasOccupants(false);
		return;
	}

	SetHasOccupants(true);

	if (ExtractionStartTime < 0.0f)
	{
		SetExtractionStartTime(CurrentTimeSeconds);
		return;
	}

	if ((CurrentTimeSeconds - ExtractionStartTime) < RequiredSafeDuration)
	{
		return;
	}

	AFrontierGameMode* FrontierGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<AFrontierGameMode>()
		: nullptr;
	if (!FrontierGameMode)
	{
		return;
	}

	// Snapshot the current occupants before extraction changes their pawns and
	// causes overlap-end callbacks. Every valid occupant shares this timer.
	const TArray<FFrontierExtractionCandidate> CandidatesToExtract = ExtractionCandidates;
	for (const FFrontierExtractionCandidate& Candidate : CandidatesToExtract)
	{
		AFrontierPlayerCharacter* PlayerCharacter = Candidate.PlayerCharacter.Get();
		if (!PlayerCharacter || PlayerCharacter->IsDead() || !ExtractionBox->IsOverlappingActor(PlayerCharacter))
		{
			continue;
		}

		if (AController* PlayerController = PlayerCharacter->GetController())
		{
			FRONTIER_LOG(Log, TEXT("Extraction condition satisfied. Player=%s Controller=%s"), *GetNameSafe(PlayerCharacter), *GetNameSafe(PlayerController));
			FrontierGameMode->HandlePlayerExtraction(PlayerController);
		}
	}

	// The zone is reusable. A later group starts a fresh shared timer.
	ExtractionCandidates.Reset();
	SetExtractionStartTime(-1.0f);
	SetHasOccupants(false);
}

float AFrontierExtractionZoneActor::GetCurrentWorldTimeSeconds() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState<AGameStateBase>())
		{
			return GameState->GetServerWorldTimeSeconds();
		}

		return World->GetTimeSeconds();
	}

	return 0.0f;
}

void AFrontierExtractionZoneActor::SetExtractionStartTime(const float NewStartTime)
{
	if (FMath::IsNearlyEqual(ExtractionStartTime, NewStartTime))
	{
		return;
	}

	ExtractionStartTime = NewStartTime;
	ForceNetUpdate();
}

void AFrontierExtractionZoneActor::SetHasOccupants(const bool bNewHasOccupants)
{
	if (bHasOccupants == bNewHasOccupants)
	{
		return;
	}

	bHasOccupants = bNewHasOccupants;
	UpdateNiagaraEffect();
	ForceNetUpdate();
}

void AFrontierExtractionZoneActor::OnRep_HasOccupants()
{
	UpdateNiagaraEffect();
}

void AFrontierExtractionZoneActor::UpdateNiagaraEffect()
{
	if (!NiagaraComponent)
	{
		return;
	}

	const float Progress = (ExtractionStartTime >= 0.0f && RequiredSafeDuration > 0.0f)
		? FMath::Clamp((GetCurrentWorldTimeSeconds() - ExtractionStartTime) / RequiredSafeDuration, 0.0f, 1.0f)
		: 0.0f;
	const float NiagaraParameterValue = FMath::Lerp(
		NiagaraParameterStartValue,
		NiagaraParameterEndValue,
		Progress);

	if (!NiagaraSpeedParameterName.IsNone())
	{
		NiagaraComponent->SetVariableFloat(NiagaraSpeedParameterName, NiagaraParameterValue);
	}

	if (bHasOccupants)
	{
		NiagaraComponent->SetVisibility(true, true);
		if (!bNiagaraEffectRunning)
		{
			// Reset=true guarantees Emitter.Age starts at zero after the B value has
			// already been applied to User.Speed.
			NiagaraComponent->Activate(true);
			bNiagaraEffectRunning = true;
		}
		return;
	}

	if (bNiagaraEffectRunning || NiagaraComponent->IsActive())
	{
		NiagaraComponent->DeactivateImmediate();
	}
	NiagaraComponent->SetVisibility(false, true);
	bNiagaraEffectRunning = false;
}

void AFrontierExtractionZoneActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFrontierExtractionZoneActor, ExtractionStartTime);
	DOREPLIFETIME(AFrontierExtractionZoneActor, bHasOccupants);
}
