#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "FrontierAggregatedStatsGameplayEffect.generated.h"

/**
 * Shared infinite effect used by stat-owning systems such as equipment and the
 * skill tree. Each system owns a separate active handle, so rebuilding one
 * source cannot remove another source's contribution.
 */
UCLASS()
class FRONTIER_API UFrontierAggregatedStatsGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UFrontierAggregatedStatsGameplayEffect();
};
