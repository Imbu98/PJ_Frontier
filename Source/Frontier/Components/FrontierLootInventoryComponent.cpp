#include "Components/FrontierLootInventoryComponent.h"

UFrontierLootInventoryComponent::UFrontierLootInventoryComponent()
{
	SlotCount = 20;
}

void UFrontierLootInventoryComponent::InitializeLootSlots(const TArray<FFrontierInventorySlot>& InitialLootSlots)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	Slots.Reset();
	const int32 TargetSlotCount = FMath::Max(SlotCount, InitialLootSlots.Num());
	Slots.Reserve(TargetSlotCount);

	for (int32 SlotIndex = 0; SlotIndex < TargetSlotCount; ++SlotIndex)
	{
		FFrontierInventorySlot Slot;
		Slot.SlotIndex = SlotIndex;

		if (InitialLootSlots.IsValidIndex(SlotIndex) && InitialLootSlots[SlotIndex].bOccupied && InitialLootSlots[SlotIndex].ItemInstance.IsValid())
		{
			Slot.bOccupied = true;
			Slot.ItemInstance = InitialLootSlots[SlotIndex].ItemInstance;
			Slot.ItemInstance.EnsureRuntimeIdentity();
		}

		Slots.Add(Slot);
	}

	BroadcastInventoryChanged();
}
