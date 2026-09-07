#include "Components/FrontierRaidInventoryComponent.h"

UFrontierRaidInventoryComponent::UFrontierRaidInventoryComponent()
{
	SlotCount = 30;
}

bool UFrontierRaidInventoryComponent::UseItemAtSlot(const int32 SlotIndex)
{
	FFrontierInventorySlot ItemBeforeUse;
	if (!GetSlot(SlotIndex, ItemBeforeUse)
		|| !ItemBeforeUse.bOccupied
		|| !ItemBeforeUse.ItemInstance.IsValid())
	{
		return false;
	}

	const bool bConsumedEntireRuntimeItem = ItemBeforeUse.ItemInstance.Quantity == 1;
	const FGuid OriginItemId = ItemBeforeUse.ItemInstance.OriginItemInstanceId;
	const FGuid RaidItemId = ItemBeforeUse.ItemInstance.RaidItemId;
	if (!Super::UseItemAtSlot(SlotIndex))
	{
		return false;
	}

	if (bConsumedEntireRuntimeItem)
	{
		if (OriginItemId.IsValid())
		{
			ConsumedOriginItemIds.Add(OriginItemId);
		}
		if (RaidItemId.IsValid())
		{
			ConsumedRaidItemIds.Add(RaidItemId);
		}
	}
	return true;
}

void UFrontierRaidInventoryComponent::ResetConsumedItemTracking()
{
	ConsumedOriginItemIds.Reset();
	ConsumedRaidItemIds.Reset();
}
