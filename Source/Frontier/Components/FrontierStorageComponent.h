#pragma once

#include "CoreMinimal.h"
#include "Components/FrontierInventoryComponent.h"
#include "FrontierStorageComponent.generated.h"

UCLASS(ClassGroup=(Frontier), meta=(BlueprintSpawnableComponent))
class FRONTIER_API UFrontierStorageComponent : public UFrontierInventoryComponent
{
	GENERATED_BODY()

public:
	UFrontierStorageComponent();

protected:
	virtual bool ShouldReplicateInventoryToOwnerOnly() const override { return true; }
};
