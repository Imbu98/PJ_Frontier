#include "UI/FrontierInventoryDragDropOperation.h"

#include "FrontierPlayerController.h"

void UFrontierInventoryDragDropOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	Super::DragCancelled_Implementation(PointerEvent);

	if (OwningPlayerController
		&& (DraggedItemInstance.IsValid() || SourceType == EFrontierDragSourceType::QuickSlot))
	{
		AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(OwningPlayerController);
		if (!FrontierPlayerController)
		{
			return;
		}

		if (FrontierPlayerController->TryHandleInventoryDragDropOnUI(PointerEvent.GetScreenSpacePosition(), this))
		{
			return;
		}

		if (SourceType == EFrontierDragSourceType::InventorySlot
			&& SourceInventoryCollection == EFrontierInventoryCollectionType::RaidInventory)
		{
			FrontierPlayerController->ServerDropRaidInventoryItem(SourceInventorySlotIndex);
		}
		else if (SourceType == EFrontierDragSourceType::LoadoutSlot
			&& SourceLoadoutSlotType != EFrontierEquipmentSlot::None)
		{
			FrontierPlayerController->ServerDropLoadoutItem(SourceLoadoutSlotType);
		}
		else if (SourceType == EFrontierDragSourceType::QuickSlot
			&& SourceQuickSlotIndex != INDEX_NONE)
		{
			FrontierPlayerController->ServerClearQuickSlot(SourceQuickSlotIndex);
		}
	}
}
