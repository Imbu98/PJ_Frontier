#pragma once

#include "CoreMinimal.h"
#include "Components/FrontierInventoryComponent.h"
#include "FrontierRaidInventoryComponent.generated.h"

UCLASS(ClassGroup=(Frontier), meta=(BlueprintSpawnableComponent))
class FRONTIER_API UFrontierRaidInventoryComponent : public UFrontierInventoryComponent
{
	GENERATED_BODY()

public:
	UFrontierRaidInventoryComponent();

	virtual bool UseItemAtSlot(int32 SlotIndex) override;

	const TSet<FGuid>& GetConsumedOriginItemIds() const { return ConsumedOriginItemIds; }
	const TSet<FGuid>& GetConsumedRaidItemIds() const { return ConsumedRaidItemIds; }
	void ResetConsumedItemTracking();

protected:
	virtual bool ShouldReplicateInventoryToOwnerOnly() const override { return true; }

private:
	TSet<FGuid> ConsumedOriginItemIds;
	TSet<FGuid> ConsumedRaidItemIds;
};
