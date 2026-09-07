#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FrontierRaidLootPoolPolicyDataAsset.generated.h"

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierRaidLootPoolPolicy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot Pool")
	FName LootTableId;

	/** Number of complete spawn waves reserved when the dedicated server starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot Pool", meta=(ClampMin="1"))
	int32 InitialSpawnWaves = 1;

	/** A refill is requested after the remaining entry count reaches this value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot Pool", meta=(ClampMin="0"))
	int32 RefillThreshold = 2;

	/** Number of entries requested per refill. Zero disables refill for this table. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot Pool", meta=(ClampMin="0"))
	int32 RefillRequestCount = 10;

	bool TryCalculateInitialRequestCount(int32 ComponentCount, int32& OutRequestedCount, FString& OutError) const;
};

/** Central dedicated-server reserve and refill policy for every backend loot table. */
UCLASS(BlueprintType)
class FRONTIER_API UFrontierRaidLootPoolPolicyDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot Pool")
	TArray<FFrontierRaidLootPoolPolicy> Policies;

	const FFrontierRaidLootPoolPolicy* FindPolicy(FName LootTableId) const;
	bool ValidatePolicies(FString& OutError) const;
};
