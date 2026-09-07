#include "Loot/FrontierRaidLootPoolPolicyDataAsset.h"

bool FFrontierRaidLootPoolPolicy::TryCalculateInitialRequestCount(
	const int32 ComponentCount,
	int32& OutRequestedCount,
	FString& OutError) const
{
	OutRequestedCount = 0;
	OutError.Reset();
	if (ComponentCount <= 0 || InitialSpawnWaves <= 0)
	{
		OutError = FString::Printf(
			TEXT("Invalid initial loot request inputs. LootTableId=%s ComponentCount=%d InitialSpawnWaves=%d"),
			*LootTableId.ToString(),
			ComponentCount,
			InitialSpawnWaves);
		return false;
	}

	const int64 RequestedCount = static_cast<int64>(ComponentCount) * InitialSpawnWaves;
	if (RequestedCount > MAX_int32)
	{
		OutError = FString::Printf(
			TEXT("Initial loot request exceeds int32 capacity. LootTableId=%s ComponentCount=%d InitialSpawnWaves=%d"),
			*LootTableId.ToString(),
			ComponentCount,
			InitialSpawnWaves);
		return false;
	}

	OutRequestedCount = static_cast<int32>(RequestedCount);
	return true;
}

const FFrontierRaidLootPoolPolicy* UFrontierRaidLootPoolPolicyDataAsset::FindPolicy(const FName LootTableId) const
{
	return Policies.FindByPredicate([LootTableId](const FFrontierRaidLootPoolPolicy& Policy)
	{
		return Policy.LootTableId == LootTableId;
	});
}

bool UFrontierRaidLootPoolPolicyDataAsset::ValidatePolicies(FString& OutError) const
{
	OutError.Reset();
	TSet<FName> SeenTableIds;
	for (const FFrontierRaidLootPoolPolicy& Policy : Policies)
	{
		if (Policy.LootTableId.IsNone())
		{
			OutError = TEXT("Raid loot pool policy contains an empty LootTableId.");
			return false;
		}
		if (SeenTableIds.Contains(Policy.LootTableId))
		{
			OutError = FString::Printf(
				TEXT("Raid loot pool policy contains duplicate LootTableId=%s."),
				*Policy.LootTableId.ToString());
			return false;
		}
		if (Policy.InitialSpawnWaves <= 0 || Policy.RefillThreshold < 0 || Policy.RefillRequestCount < 0)
		{
			OutError = FString::Printf(
				TEXT("Raid loot pool policy has invalid values. LootTableId=%s InitialSpawnWaves=%d RefillThreshold=%d RefillRequestCount=%d"),
				*Policy.LootTableId.ToString(),
				Policy.InitialSpawnWaves,
				Policy.RefillThreshold,
				Policy.RefillRequestCount);
			return false;
		}
		SeenTableIds.Add(Policy.LootTableId);
	}
	return true;
}
