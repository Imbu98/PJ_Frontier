#include "AI/FrontierEnemyAIController.h"

#include "Character/FrontierEnemyCharacter.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Combat/FrontierDamageStatics.h"
#include "Components/StateTreeAIComponent.h"
#include "Frontier.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Sight.h"

AFrontierEnemyAIController::AFrontierEnemyAIController()
{
	

	StateTreeAIComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAIComponent"));
	BrainComponent = StateTreeAIComponent;

	FrontierPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 1500.0f;
	SightConfig->LoseSightRadius = 1800.0f;
	SightConfig->PeripheralVisionAngleDegrees = 70.0f;
	SightConfig->SetMaxAge(2.0f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageConfig"));
	DamageConfig->SetMaxAge(5.0f);

	FrontierPerceptionComponent->ConfigureSense(*SightConfig);
	FrontierPerceptionComponent->ConfigureSense(*DamageConfig);
	FrontierPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	SetPerceptionComponent(*FrontierPerceptionComponent);

	bStartAILogicOnPossess = true;
}

UStateTreeAIComponent* AFrontierEnemyAIController::GetStateTreeAIComponent() const
{
	
	return StateTreeAIComponent;
}

void AFrontierEnemyAIController::BeginPlay()
{
	
	Super::BeginPlay();

	FrontierPerceptionComponent->OnPerceptionUpdated.AddDynamic(this, &AFrontierEnemyAIController::HandlePerceptionUpdated);
}

void AFrontierEnemyAIController::HandlePerceptionUpdated(const TArray<AActor*>& UpdatedActors)
{
	AFrontierEnemyCharacter* EnemyCharacter = Cast<AFrontierEnemyCharacter>(GetPawn());
	if (!EnemyCharacter)
	{
		FRONTIER_LOG(Warning, TEXT("Cannot update combat target because controlled pawn is not a Frontier enemy."));
		return;
	}

	if (EnemyCharacter->IsDead())
	{
		return;
	}

	TArray<AActor*> PerceivedActors;
	FrontierPerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);

	TArray<AActor*> DamagePerceivedActors;
	FrontierPerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Damage::StaticClass(), DamagePerceivedActors);
	for (AActor* DamagePerceivedActor : DamagePerceivedActors)
	{
		PerceivedActors.AddUnique(DamagePerceivedActor);
	}

	AActor* ClosestTarget = nullptr;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();

	for (AActor* PerceivedActor : PerceivedActors)
	{
		if (!IsValid(PerceivedActor)
			|| !PerceivedActor->IsA<AFrontierPlayerCharacter>()
			|| !UFrontierDamageStatics::CanActorsDamageEachOther(EnemyCharacter, PerceivedActor))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(EnemyCharacter->GetActorLocation(), PerceivedActor->GetActorLocation());
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestTarget = PerceivedActor;
			ClosestDistanceSquared = DistanceSquared;
		}
	}

	EnemyCharacter->SetCombatTarget(ClosestTarget);
}
