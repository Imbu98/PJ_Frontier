#include "OutGameInventoryWrapperWidget.h"

#include "FrontierPlayerController.h"
#include "Game/FrontierLobbyPlayerController.h"
#include "Game/FrontierPlayerState.h"
#include "GameFramework/Character.h"
#include "UI/FrontierInventoryDragDropOperation.h"
#include "UI/FrontierCharacterPreviewWidget.h"
#include "UI/FrontierInventoryWidget.h"

namespace
{
	bool StoreRaidItemInStorage(APlayerController* Controller, const int32 SourceRaidSlotIndex)
	{
		if (AFrontierPlayerController* FrontierController = Cast<AFrontierPlayerController>(Controller))
		{
			FrontierController->ServerStoreRaidItemInStorage(SourceRaidSlotIndex);
			return true;
		}

		if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(Controller))
		{
			LobbyController->ServerStoreRaidItemInStorage(SourceRaidSlotIndex);
			return true;
		}

		return false;
	}

	bool WithdrawLobbyItemToRaidInventory(APlayerController* Controller, const int32 SourceLobbySlotIndex)
	{
		if (AFrontierPlayerController* FrontierController = Cast<AFrontierPlayerController>(Controller))
		{
			FrontierController->ServerWithdrawLobbyItemToRaidInventory(SourceLobbySlotIndex);
			return true;
		}

		if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(Controller))
		{
			LobbyController->ServerWithdrawLobbyItemToRaidInventory(SourceLobbySlotIndex);
			return true;
		}

		return false;
	}

	bool UnequipLoadoutItemToRaidInventory(APlayerController* Controller, const EFrontierEquipmentSlot SlotType)
	{
		if (AFrontierPlayerController* FrontierController = Cast<AFrontierPlayerController>(Controller))
		{
			FrontierController->ServerUnequipLoadoutItemToRaidInventory(SlotType);
			return true;
		}

		if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(Controller))
		{
			LobbyController->ServerUnequipLoadoutItemToRaidInventory(SlotType);
			return true;
		}

		return false;
	}
}

bool UOutGameInventoryWrapperWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UFrontierInventoryDragDropOperation* DragOperation = Cast<UFrontierInventoryDragDropOperation>(InOperation);
	if (!DragOperation)
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	return TryHandlePanelDrop(InDragDropEvent.GetScreenSpacePosition(), DragOperation)
		? true
		: Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UOutGameInventoryWrapperWidget::RefreshFromPlayerState(AFrontierPlayerState* PlayerState)
{
	if (WBP_CharacterPreviewWidget)
	{
		WBP_CharacterPreviewWidget->SetPreviewPlayerState(PlayerState);
	}

	if (WBP_PlayerInventory)
	{
		WBP_PlayerInventory->SetObservedPlayerState(PlayerState);
		WBP_PlayerInventory->SetInventoryWidgetMode(EInventoryWidgetMode::PlayerInventory);
		WBP_PlayerInventory->RefreshFromPlayerState();
	}

	if (WBP_PlayerStorage)
	{
		WBP_PlayerStorage->SetObservedPlayerState(PlayerState);
		WBP_PlayerStorage->SetInventoryWidgetMode(EInventoryWidgetMode::StorageView);
		WBP_PlayerStorage->RefreshFromPlayerState();
	}
}

UFrontierInventoryWidget* UOutGameInventoryWrapperWidget::GetPlayerInventoryWidget() const
{
	return WBP_PlayerInventory;
}

bool UOutGameInventoryWrapperWidget::TryHandlePanelDrop(const FVector2D& ScreenSpacePosition, UFrontierInventoryDragDropOperation* DragOperation)
{
	APlayerController* OwningController = GetOwningPlayer();
	if (!DragOperation || !OwningController)
	{
		return false;
	}

	UFrontierInventoryWidget* TargetInventoryWidget = ResolveDropTargetInventoryWidget(ScreenSpacePosition);
	if (!TargetInventoryWidget)
	{
		return false;
	}

	switch (TargetInventoryWidget->GetInventoryWidgetMode())
	{
	case EInventoryWidgetMode::StorageView:
		if (DragOperation->SourceType == EFrontierDragSourceType::InventorySlot
			&& DragOperation->SourceInventoryCollection == EFrontierInventoryCollectionType::RaidInventory)
		{
			return StoreRaidItemInStorage(OwningController, DragOperation->SourceInventorySlotIndex);
		}
		break;

	case EInventoryWidgetMode::PlayerInventory:
		if (DragOperation->SourceType == EFrontierDragSourceType::InventorySlot
			&& DragOperation->SourceInventoryCollection == EFrontierInventoryCollectionType::Storage)
		{
			return WithdrawLobbyItemToRaidInventory(OwningController, DragOperation->SourceInventorySlotIndex);
		}

		if (DragOperation->SourceType == EFrontierDragSourceType::LoadoutSlot)
		{
			return UnequipLoadoutItemToRaidInventory(OwningController, DragOperation->SourceLoadoutSlotType);
		}
		break;

	default:
		break;
	}

	return false;
}

UFrontierInventoryWidget* UOutGameInventoryWrapperWidget::ResolveDropTargetInventoryWidget(const FVector2D& ScreenSpacePosition) const
{
	if (WBP_PlayerInventory && WBP_PlayerInventory->GetCachedGeometry().IsUnderLocation(ScreenSpacePosition))
	{
		return WBP_PlayerInventory;
	}

	if (WBP_PlayerStorage && WBP_PlayerStorage->GetCachedGeometry().IsUnderLocation(ScreenSpacePosition))
	{
		return WBP_PlayerStorage;
	}

	if (!GetCachedGeometry().IsUnderLocation(ScreenSpacePosition))
	{
		return nullptr;
	}

	if (WBP_PlayerInventory && WBP_PlayerStorage)
	{
		const FVector2D WrapperCenter = GetCachedGeometry().GetAbsolutePosition() + (GetCachedGeometry().GetAbsoluteSize() * 0.5f);
		return ScreenSpacePosition.X < WrapperCenter.X ? WBP_PlayerInventory.Get() : WBP_PlayerStorage.Get();
	}

	return WBP_PlayerStorage ? WBP_PlayerStorage.Get() : WBP_PlayerInventory.Get();
}
