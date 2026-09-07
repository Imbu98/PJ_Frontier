#pragma once

#include "CoreMinimal.h"

struct FFrontierItemInstance;
struct FFrontierOnlineRaidRuntimeItemDTO;
class UFrontierItemCatalogSubsystem;

/** Converts backend-owned raid item snapshots without rerolling identity, stats, or skills. */
struct FRONTIER_API FFrontierRaidRuntimeItemMapper
{
	static bool TryBuildRuntimeItem(
		const FFrontierOnlineRaidRuntimeItemDTO& RaidItem,
		const UFrontierItemCatalogSubsystem& ItemCatalog,
		bool bRequireLootSourceId,
		FFrontierItemInstance& OutItem,
		FString& OutError);
};
