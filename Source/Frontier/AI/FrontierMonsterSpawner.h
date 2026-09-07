#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrontierMonsterSpawner.generated.h"

class AFrontierEnemyCharacter;
class USceneComponent;
class USplineComponent;

USTRUCT(BlueprintType)
struct FFrontierMonsterSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Spawner")
	TSubclassOf<AFrontierEnemyCharacter> MonsterClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Spawner", meta=(ClampMin="1"))
	int32 SpawnCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Spawner")
	FTransform RelativeSpawnTransform = FTransform::Identity;
};

UCLASS()
class FRONTIER_API AFrontierMonsterSpawner : public AActor
{
	GENERATED_BODY()

public:
	AFrontierMonsterSpawner();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleManagedMonsterDied(AFrontierEnemyCharacter* DeadEnemy);

	void SpawnMonsters();
	void SpawnMonsterEntry(const FFrontierMonsterSpawnEntry& SpawnEntry);
	void InitializeSpawnedMonster(AFrontierEnemyCharacter* SpawnedEnemy);
	void StartRespawnTimerIfNeeded();
	void HandleRespawnTimerElapsed();
	void ClearManagedMonsters();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Spawner")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Spawner|Patrol")
	TObjectPtr<USplineComponent> PatrolSplineComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Spawner")
	TArray<FFrontierMonsterSpawnEntry> SpawnEntries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Spawner", meta=(ClampMin="0.0"))
	float RespawnDelay = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Spawner|Patrol", meta=(ClampMin="0.0"))
	float PatrolRadius = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Spawner|Patrol")
	bool bUsePatrolSpline = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AFrontierEnemyCharacter>> ManagedMonsters;

	FTimerHandle RespawnTimerHandle;
};
