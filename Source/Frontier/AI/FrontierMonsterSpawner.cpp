#include "AI/FrontierMonsterSpawner.h"

#include "Character/FrontierEnemyCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "Frontier.h"
#include "TimerManager.h"

AFrontierMonsterSpawner::AFrontierMonsterSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PatrolSplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("PatrolSplineComponent"));
	PatrolSplineComponent->SetupAttachment(SceneRoot);
}

void AFrontierMonsterSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		SpawnMonsters();
	}
}

void AFrontierMonsterSpawner::SpawnMonsters()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
	ClearManagedMonsters();

	for (const FFrontierMonsterSpawnEntry& SpawnEntry : SpawnEntries)
	{
		SpawnMonsterEntry(SpawnEntry);
	}

	FRONTIER_LOG(
		Log,
		TEXT("Monster spawner wave initialized. Spawner=%s ManagedCount=%d"),
		*GetNameSafe(this),
		ManagedMonsters.Num());
}

void AFrontierMonsterSpawner::SpawnMonsterEntry(const FFrontierMonsterSpawnEntry& SpawnEntry)
{
	if (!SpawnEntry.MonsterClass)
	{
		FRONTIER_LOG(Warning, TEXT("Monster spawner skipped invalid entry. Spawner=%s"), *GetNameSafe(this));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (int32 SpawnIndex = 0; SpawnIndex < SpawnEntry.SpawnCount; ++SpawnIndex)
	{
		const FTransform SpawnTransform = SpawnEntry.RelativeSpawnTransform * GetActorTransform();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AFrontierEnemyCharacter* SpawnedEnemy = World->SpawnActor<AFrontierEnemyCharacter>(
			SpawnEntry.MonsterClass,
			SpawnTransform,
			SpawnParameters);
		if (!SpawnedEnemy)
		{
			FRONTIER_LOG(Warning, TEXT("Monster spawner failed to spawn enemy. Spawner=%s Class=%s"),
				*GetNameSafe(this),
				*GetNameSafe(SpawnEntry.MonsterClass.Get()));
			continue;
		}

		InitializeSpawnedMonster(SpawnedEnemy);
	}
}

void AFrontierMonsterSpawner::InitializeSpawnedMonster(AFrontierEnemyCharacter* SpawnedEnemy)
{
	if (!SpawnedEnemy)
	{
		return;
	}

	ManagedMonsters.Add(SpawnedEnemy);
	SpawnedEnemy->OnEnemyDied.RemoveDynamic(this, &AFrontierMonsterSpawner::HandleManagedMonsterDied);
	SpawnedEnemy->OnEnemyDied.AddDynamic(this, &AFrontierMonsterSpawner::HandleManagedMonsterDied);

	const FVector SpawnedLocation = SpawnedEnemy->GetActorLocation();
	if (bUsePatrolSpline && PatrolSplineComponent && PatrolSplineComponent->GetNumberOfSplinePoints() > 0)
	{
		SpawnedEnemy->SetSplinePatrolRoute(SpawnedLocation, PatrolSplineComponent);
	}
	else
	{
		SpawnedEnemy->SetRandomPatrolArea(SpawnedLocation, GetActorLocation(), PatrolRadius);
	}

	FRONTIER_LOG(Log, TEXT("Monster spawner registered enemy. Spawner=%s Enemy=%s Home=%s PatrolCenter=%s Radius=%.2f UseSpline=%d"),
		*GetNameSafe(this),
		*GetNameSafe(SpawnedEnemy),
		*SpawnedLocation.ToCompactString(),
		*GetActorLocation().ToCompactString(),
		PatrolRadius,
		bUsePatrolSpline ? 1 : 0);
}

void AFrontierMonsterSpawner::HandleManagedMonsterDied(AFrontierEnemyCharacter* DeadEnemy)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 ManagedMonsterIndex = ManagedMonsters.IndexOfByKey(DeadEnemy);
	if (ManagedMonsterIndex == INDEX_NONE)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Monster spawner ignored death from an unmanaged or already processed enemy. Spawner=%s Enemy=%s"),
			*GetNameSafe(this),
			*GetNameSafe(DeadEnemy));
		return;
	}

	if (DeadEnemy)
	{
		DeadEnemy->OnEnemyDied.RemoveDynamic(this, &AFrontierMonsterSpawner::HandleManagedMonsterDied);
	}
	ManagedMonsters.RemoveAtSwap(ManagedMonsterIndex);

	FRONTIER_LOG(Log, TEXT("Monster spawner processed monster death. Spawner=%s Enemy=%s Remaining=%d"),
		*GetNameSafe(this),
		*GetNameSafe(DeadEnemy),
		ManagedMonsters.Num());

	StartRespawnTimerIfNeeded();
}

void AFrontierMonsterSpawner::StartRespawnTimerIfNeeded()
{
	if (!ManagedMonsters.IsEmpty() || GetWorldTimerManager().IsTimerActive(RespawnTimerHandle))
	{
		return;
	}

	if (RespawnDelay <= 0.0f)
	{
		HandleRespawnTimerElapsed();
		return;
	}

	FRONTIER_LOG(Log, TEXT("Monster spawner starting respawn timer. Spawner=%s Delay=%.2f"),
		*GetNameSafe(this),
		RespawnDelay);

	GetWorldTimerManager().SetTimer(
		RespawnTimerHandle,
		this,
		&AFrontierMonsterSpawner::HandleRespawnTimerElapsed,
		RespawnDelay,
		false);
}

void AFrontierMonsterSpawner::HandleRespawnTimerElapsed()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!ManagedMonsters.IsEmpty())
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Monster spawner cancelled elapsed respawn because managed enemies are still pending. Spawner=%s Remaining=%d"),
			*GetNameSafe(this),
			ManagedMonsters.Num());
		return;
	}

	FRONTIER_LOG(Log, TEXT("Monster spawner respawn timer elapsed. Spawner=%s"), *GetNameSafe(this));
	SpawnMonsters();
}

void AFrontierMonsterSpawner::ClearManagedMonsters()
{
	for (TObjectPtr<AFrontierEnemyCharacter>& ManagedMonster : ManagedMonsters)
	{
		if (AFrontierEnemyCharacter* Enemy = ManagedMonster.Get())
		{
			Enemy->OnEnemyDied.RemoveDynamic(this, &AFrontierMonsterSpawner::HandleManagedMonsterDied);
		}
	}

	ManagedMonsters.Reset();
}
