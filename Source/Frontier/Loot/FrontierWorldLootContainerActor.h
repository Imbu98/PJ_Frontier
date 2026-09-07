#pragma once

#include "CoreMinimal.h"
#include "Loot/FrontierLootContainerActor.h"
#include "FrontierWorldLootContainerActor.generated.h"

/** Level-placed or otherwise persistent world loot container. */
UCLASS()
class FRONTIER_API AFrontierWorldLootContainerActor : public AFrontierLootContainerActor
{
	GENERATED_BODY()

public:
	AFrontierWorldLootContainerActor();
};
