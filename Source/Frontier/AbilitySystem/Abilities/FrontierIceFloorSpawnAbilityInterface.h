#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FrontierIceFloorSpawnAbilityInterface.generated.h"

UINTERFACE(MinimalAPI)
class UFrontierIceFloorSpawnAbilityInterface : public UInterface
{
	GENERATED_BODY()
};

class FRONTIER_API IFrontierIceFloorSpawnAbilityInterface
{
	GENERATED_BODY()

public:
	virtual void SpawnIceFloorFromNotify() = 0;
};
