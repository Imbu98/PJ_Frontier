#include "UI/FrontierInventoryWidget.h"

#include "Components/FrontierLoadoutComponent.h"
#include "Components/FrontierLootInventoryComponent.h"
#include "Components/FrontierStorageComponent.h"
#include "Components/FrontierRaidInventoryComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierLobbyPlayerController.h"
#include "Game/FrontierPlayerState.h"
#include "InputCoreTypes.h"
#include "Inventory/Items/FrontierArmorItemDataAsset.h"
#include "Inventory/Items/FrontierConsumableItemDataAsset.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "Inventory/Items/FrontierWeaponItemDataAsset.h"
#include "Loot/FrontierDroppedItemActor.h"
#include "Loot/FrontierLootContainerActor.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Frontier.h"
#include "FrontierSteamSubsystem.h"
#include "Components/Button.h"
#include "UI/FrontierInventoryDragDropOperation.h"
#include "UI/FrontierItemTooltipWidget.h"
#include "UI/FrontierItemRarityBorderWidget.h"
#include "UI/FrontierInventorySlotWidget.h"
#include "UI/FrontierLoadoutSlotWidget.h"

class UFrontierSteamSubsystem;

namespace
{
	bool ServerEquipRaidInventoryItemToLoadout(APlayerController* Controller, const int32 SourceSlotIndex, const EFrontierEquipmentSlot SlotType)
	{
		if (AFrontierPlayerController* FrontierController = Cast<AFrontierPlayerController>(Controller))
		{
			FrontierController->ServerEquipRaidInventoryItemToLoadout(SourceSlotIndex, SlotType);
			return true;
		}

		if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(Controller))
		{
			LobbyController->ServerEquipRaidInventoryItemToLoadout(SourceSlotIndex, SlotType);
			return true;
		}

		return false;
	}

	bool ServerUnequipLoadoutItemToRaidInventory(APlayerController* Controller, const EFrontierEquipmentSlot SlotType)
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

	bool ServerUnequipLoadoutItemToRaidInventorySlot(APlayerController* Controller, const EFrontierEquipmentSlot SlotType, const int32 TargetSlotIndex)
	{
		if (AFrontierPlayerController* FrontierController = Cast<AFrontierPlayerController>(Controller))
		{
			FrontierController->ServerUnequipLoadoutItemToRaidInventorySlot(SlotType, TargetSlotIndex);
			return true;
		}

		if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(Controller))
		{
			LobbyController->ServerUnequipLoadoutItemToRaidInventorySlot(SlotType, TargetSlotIndex);
			return true;
		}

		return false;
	}

	bool ServerSwapRaidInventorySlots(APlayerController* Controller, const int32 SourceSlotIndex, const int32 TargetSlotIndex)
	{
		if (AFrontierPlayerController* FrontierController = Cast<AFrontierPlayerController>(Controller))
		{
			FrontierController->ServerSwapRaidInventorySlots(SourceSlotIndex, TargetSlotIndex);
			return true;
		}

		if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(Controller))
		{
			LobbyController->ServerSwapRaidInventorySlots(SourceSlotIndex, TargetSlotIndex);
			return true;
		}

		return false;
	}

	bool ServerSwapStorageSlots(APlayerController* Controller, const int32 SourceSlotIndex, const int32 TargetSlotIndex)
	{
		if (AFrontierPlayerController* FrontierController = Cast<AFrontierPlayerController>(Controller))
		{
			FrontierController->ServerSwapStorageSlots(SourceSlotIndex, TargetSlotIndex);
			return true;
		}

		if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(Controller))
		{
			LobbyController->ServerSwapStorageSlots(SourceSlotIndex, TargetSlotIndex);
			return true;
		}

		return false;
	}

	bool ServerStoreRaidItemInStorage(APlayerController* Controller, const int32 SourceRaidSlotIndex)
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

	bool ServerStoreRaidItemInStorageSlot(APlayerController* Controller, const int32 SourceRaidSlotIndex, const int32 TargetLobbySlotIndex)
	{
		if (AFrontierPlayerController* FrontierController = Cast<AFrontierPlayerController>(Controller))
		{
			FrontierController->ServerStoreRaidItemInStorageSlot(SourceRaidSlotIndex, TargetLobbySlotIndex);
			return true;
		}

		if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(Controller))
		{
			LobbyController->ServerStoreRaidItemInStorageSlot(SourceRaidSlotIndex, TargetLobbySlotIndex);
			return true;
		}

		return false;
	}

	bool ServerWithdrawLobbyItemToRaidInventory(APlayerController* Controller, const int32 SourceLobbySlotIndex)
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

	bool ServerWithdrawLobbyItemToRaidInventorySlot(APlayerController* Controller, const int32 SourceLobbySlotIndex, const int32 TargetRaidSlotIndex)
	{
		if (AFrontierPlayerController* FrontierController = Cast<AFrontierPlayerController>(Controller))
		{
			FrontierController->ServerWithdrawLobbyItemToRaidInventorySlot(SourceLobbySlotIndex, TargetRaidSlotIndex);
			return true;
		}

		if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(Controller))
		{
			LobbyController->ServerWithdrawLobbyItemToRaidInventorySlot(SourceLobbySlotIndex, TargetRaidSlotIndex);
			return true;
		}

		return false;
	}

	template<typename SlotType>
	bool FindChangedSlotIndices(const TArray<SlotType>& PreviousSlots, const TArray<SlotType>& NewSlots, TArray<int32>& OutChangedSlotIndices)
	{
		OutChangedSlotIndices.Reset();
		if (PreviousSlots.Num() != NewSlots.Num())
		{
			return true;
		}

		for (int32 SlotIndex = 0; SlotIndex < NewSlots.Num(); ++SlotIndex)
		{
			if (!SlotType::StaticStruct()->CompareScriptStruct(&PreviousSlots[SlotIndex], &NewSlots[SlotIndex], PPF_None))
			{
				OutChangedSlotIndices.Add(SlotIndex);
			}
		}

		return false;
	}
}

void UFrontierInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (!ObservedPlayerState)
	{
		ObservedPlayerState = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>() : nullptr;
	}
	


	RefreshFromPlayerState();
}

void UFrontierInventoryWidget::NativeDestruct()
{
	HideItemTooltip(TooltipSourceWidget);
	UnbindInventorySources();
	Super::NativeDestruct();
}

FReply UFrontierInventoryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::F)
	{
		if (AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>())
		{
			if (FrontierPlayerController->IsLootUIOpen())
			{
				FrontierPlayerController->RequestInteraction();
				return FReply::Handled();
			}
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool UFrontierInventoryWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (UFrontierInventoryDragDropOperation* DragOperation = Cast<UFrontierInventoryDragDropOperation>(InOperation))
	{
		if (APlayerController* OwningController = GetOwningPlayer())
		{
			if (WidgetMode == EInventoryWidgetMode::PlayerInventory
				&& DragOperation->SourceType == EFrontierDragSourceType::DroppedItem
				&& IsValid(DragOperation->SourceDroppedItem))
			{
				if (AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(OwningController))
				{
					FrontierPlayerController->ServerPickupDroppedItem(DragOperation->SourceDroppedItem);
					ClearSelection();
					return true;
				}
			}

			if (WidgetMode == EInventoryWidgetMode::StorageView
				&& DragOperation->SourceType == EFrontierDragSourceType::InventorySlot
				&& DragOperation->SourceInventoryCollection == EFrontierInventoryCollectionType::RaidInventory)
			{
				ServerStoreRaidItemInStorage(OwningController, DragOperation->SourceInventorySlotIndex);
				ClearSelection();
				return true;
			}

			if (WidgetMode == EInventoryWidgetMode::PlayerInventory)
			{
				if (DragOperation->SourceType == EFrontierDragSourceType::InventorySlot
					&& DragOperation->SourceInventoryCollection == EFrontierInventoryCollectionType::Storage)
				{
					ServerWithdrawLobbyItemToRaidInventory(OwningController, DragOperation->SourceInventorySlotIndex);
					ClearSelection();
					return true;
				}

				if (DragOperation->SourceType == EFrontierDragSourceType::LoadoutSlot)
				{
					ServerUnequipLoadoutItemToRaidInventory(OwningController, DragOperation->SourceLoadoutSlotType);
					ClearSelection();
					return true;
				}
			}
		}
	}

	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UFrontierInventoryWidget::SetObservedPlayerState(AFrontierPlayerState* InObservedPlayerState)
{
	if (ObservedPlayerState == InObservedPlayerState)
	{
		return;
	}

	UnbindInventorySources();
	ObservedPlayerState = InObservedPlayerState;
	RefreshFromPlayerState();
}

void UFrontierInventoryWidget::SetObservedLootContainer(AFrontierLootContainerActor* InObservedLootContainer)
{
	if (ObservedLootContainer == InObservedLootContainer)
	{
		return;
	}

	UnbindInventorySources();
	ObservedLootContainer = InObservedLootContainer;
	RefreshFromPlayerState();
}

void UFrontierInventoryWidget::SetInventoryWidgetMode(const EInventoryWidgetMode InWidgetMode)
{
	if (WidgetMode == InWidgetMode)
	{
		return;
	}

	UnbindInventorySources();
	WidgetMode = InWidgetMode;
	ClearSelection();
	RefreshFromPlayerState();
}

EInventoryWidgetMode UFrontierInventoryWidget::GetInventoryWidgetMode() const
{
	return WidgetMode;
}

AFrontierPlayerState* UFrontierInventoryWidget::GetObservedPlayerState() const
{
	return ObservedPlayerState;
}

UFrontierStorageComponent* UFrontierInventoryWidget::GetStorageComponent() const
{
	return ObservedPlayerState ? ObservedPlayerState->GetStorageComponent() : nullptr;
}

UFrontierRaidInventoryComponent* UFrontierInventoryWidget::GetRaidInventoryComponent() const
{
	return ObservedPlayerState ? ObservedPlayerState->GetRaidInventoryComponent() : nullptr;
}

UFrontierLoadoutComponent* UFrontierInventoryWidget::GetLoadoutComponent() const
{
	const bool bNeedsLoadout = WidgetMode == EInventoryWidgetMode::PlayerInventory
		|| WidgetMode == EInventoryWidgetMode::StorageView
		|| WidgetMode == EInventoryWidgetMode::UpgradeSelect;
	return bNeedsLoadout && ObservedPlayerState ? ObservedPlayerState->GetLoadoutComponent() : nullptr;
}

UFrontierLootInventoryComponent* UFrontierInventoryWidget::GetLootInventoryComponent() const
{
	return ObservedLootContainer ? ObservedLootContainer->GetLootInventoryComponent() : nullptr;
}

void UFrontierInventoryWidget::RefreshFromPlayerState()
{
	BindInventorySources();
	UpdateCachedData();
	NativeOnInventoryDataChanged();
}

bool UFrontierInventoryWidget::IsUpgradeSelectMode() const
{
	return WidgetMode == EInventoryWidgetMode::UpgradeSelect;
}

bool UFrontierInventoryWidget::IsEquipmentItemInstance(const FFrontierItemInstance& ItemInstance) const
{
	if (!ItemInstance.IsValid())
	{
		return false;
	}

	const EFrontierItemCategory Category = ItemInstance.GetCategory();
	return Category == EFrontierItemCategory::Weapon
		|| Category == EFrontierItemCategory::Armor
		|| Category == EFrontierItemCategory::Accessory;
}

const TArray<FFrontierInventorySlot>& UFrontierInventoryWidget::GetCachedLobbySlots() const
{
	return CachedLobbySlots;
}

const TArray<FFrontierInventorySlot>& UFrontierInventoryWidget::GetCachedRaidSlots() const
{
	return CachedRaidSlots;
}

const TArray<FFrontierLoadoutSlot>& UFrontierInventoryWidget::GetCachedLoadoutSlots() const
{
	return CachedLoadoutSlots;
}

void UFrontierInventoryWidget::ShowItemTooltipForInventorySlot(UFrontierInventorySlotWidget* SlotWidget, const FFrontierItemInstance& ItemInstance)
{
	ShowItemTooltipInternal(SlotWidget, ItemInstance);
}

void UFrontierInventoryWidget::ShowItemTooltipForLoadoutSlot(UFrontierLoadoutSlotWidget* SlotWidget, const FFrontierItemInstance& ItemInstance)
{
	ShowItemTooltipInternal(SlotWidget, ItemInstance);
}

void UFrontierInventoryWidget::ShowItemTooltipForWidget(UWidget* SourceWidget, const FFrontierItemInstance& ItemInstance)
{
	ShowItemTooltipInternal(SourceWidget, ItemInstance, true);
}

void UFrontierInventoryWidget::HideItemTooltip(UWidget* RequestingWidget)
{
	if (!ItemTooltipWidget)
	{
		return;
	}

	if (RequestingWidget && TooltipSourceWidget != RequestingWidget)
	{
		return;
	}

	TooltipSourceWidget = nullptr;
	ItemTooltipWidget->SetVisibility(ESlateVisibility::Collapsed);
	ItemTooltipWidget->ClearItemInstance();
}

void UFrontierInventoryWidget::HandleStorageChanged(const TArray<FFrontierInventorySlot>& InSlots)
{
	TArray<FFrontierInventorySlot> NormalizedSlots = InSlots;
	NormalizeInventorySlotsForDisplay(NormalizedSlots, BoundStorageComponent);
	ApplyInventorySlotChanges(CachedLobbySlots, NormalizedSlots, WidgetMode == EInventoryWidgetMode::StorageView);
}

void UFrontierInventoryWidget::HandleRaidInventoryChanged(const TArray<FFrontierInventorySlot>& InSlots)
{
	TArray<FFrontierInventorySlot> NormalizedSlots = InSlots;
	NormalizeInventorySlotsForDisplay(NormalizedSlots, BoundRaidInventoryComponent);
	ApplyInventorySlotChanges(CachedRaidSlots, NormalizedSlots, WidgetMode == EInventoryWidgetMode::PlayerInventory || WidgetMode == EInventoryWidgetMode::UpgradeSelect);
}

void UFrontierInventoryWidget::HandleLoadoutChanged(const TArray<FFrontierLoadoutSlot>& InSlots)
{
	TArray<FFrontierLoadoutSlot> NormalizedSlots = InSlots;
	NormalizeLoadoutSlotsForDisplay(NormalizedSlots);
	ApplyLoadoutSlotChanges(NormalizedSlots,
		WidgetMode == EInventoryWidgetMode::PlayerInventory
		|| WidgetMode == EInventoryWidgetMode::StorageView
		|| WidgetMode == EInventoryWidgetMode::UpgradeSelect);
}

void UFrontierInventoryWidget::HandleLootInventoryChanged(const TArray<FFrontierInventorySlot>& InSlots)
{
	ApplyInventorySlotChanges(CachedLootSlots, InSlots, WidgetMode == EInventoryWidgetMode::LootView);
}

void UFrontierInventoryWidget::HandleDeadPlayerInventoryChanged(const TArray<FFrontierInventorySlot>& InSlots)
{
	ApplyInventorySlotChanges(CachedLootSlots, InSlots, WidgetMode == EInventoryWidgetMode::DeadPlayerLootView);
}

void UFrontierInventoryWidget::HandleDeadPlayerLoadoutChanged(const TArray<FFrontierLoadoutSlot>& InSlots)
{
	ApplyLoadoutSlotChanges(InSlots, WidgetMode == EInventoryWidgetMode::DeadPlayerLootView);
}

void UFrontierInventoryWidget::NativeOnInventoryDataChanged()
{
	HideItemTooltip(nullptr);
	RebuildRaidInventoryGrid();
	RebuildLoadoutGrid();

	if (RaidInventoryGrid)
	{
		RaidInventoryGrid->InvalidateLayoutAndVolatility();
	}

	if (LoadoutGrid)
	{
		LoadoutGrid->InvalidateLayoutAndVolatility();
	}

	InvalidateLayoutAndVolatility();
	ForceLayoutPrepass();
	RefreshSelectionState();
	RefreshSelectedItemInfo();
	ApplyUpgradeSelectionEnabledState();
}

void UFrontierInventoryWidget::RebuildRaidInventoryGrid()
{
	if (!RaidInventoryGrid || !InventorySlotWidgetClass)
	{
		return;
	}

	RaidInventoryGrid->ClearChildren();

	const int32 SafeColumnCount = FMath::Max(1, InventoryGridColumns);

	const TArray<FFrontierInventorySlot>* DisplaySlots = &CachedLootSlots;
	EFrontierInventoryCollectionType CollectionType = EFrontierInventoryCollectionType::LootInventory;
	if (WidgetMode == EInventoryWidgetMode::PlayerInventory || WidgetMode == EInventoryWidgetMode::UpgradeSelect)
	{
		DisplaySlots = &CachedRaidSlots;
		CollectionType = EFrontierInventoryCollectionType::RaidInventory;
	}
	else if (WidgetMode == EInventoryWidgetMode::StorageView)
	{
		DisplaySlots = &CachedLobbySlots;
		CollectionType = EFrontierInventoryCollectionType::Storage;
	}
	CreatedInventorySlotWidgets.SetNum(DisplaySlots->Num());

	for (int32 SlotIndex = 0; SlotIndex < DisplaySlots->Num(); ++SlotIndex)
	{
		UFrontierInventorySlotWidget* SlotWidget = CreateWidget<UFrontierInventorySlotWidget>(this, InventorySlotWidgetClass);
		if (!SlotWidget)
		{
			continue;
		}

		SlotWidget->SetSlotData(SlotIndex, (*DisplaySlots)[SlotIndex]);
		SlotWidget->SetInventoryCollectionType(CollectionType);
		SlotWidget->SetOwningInventoryWidget(this);
		SlotWidget->OnSlotClicked.AddDynamic(this, &UFrontierInventoryWidget::HandleInventorySlotClicked);
		SlotWidget->OnSlotDoubleClicked.AddDynamic(this, &UFrontierInventoryWidget::HandleInventorySlotDoubleClicked);
		SlotWidget->OnSlotRightClicked.AddDynamic(this, &UFrontierInventoryWidget::HandleInventorySlotRightClicked);
		SlotWidget->OnSlotAltRightClicked.AddDynamic(this, &UFrontierInventoryWidget::HandleInventorySlotAltRightClicked);
		SlotWidget->OnSlotDropped.AddDynamic(this, &UFrontierInventoryWidget::HandleInventorySlotDropped);
		SlotWidget->SetIsEnabled(!IsUpgradeSelectMode() || CanUseSlotForUpgradeSelection((*DisplaySlots)[SlotIndex]));
		CreatedInventorySlotWidgets[SlotIndex] = SlotWidget;

		if (UUniformGridSlot* GridSlot = RaidInventoryGrid->AddChildToUniformGrid(SlotWidget))
		{
			GridSlot->SetRow(SlotIndex / SafeColumnCount);
			GridSlot->SetColumn(SlotIndex % SafeColumnCount);
		}
	}
}

void UFrontierInventoryWidget::RebuildLoadoutGrid()
{
	if (!LoadoutGrid || !LoadoutSlotWidgetClass)
	{
		return;
	}

	LoadoutGrid->ClearChildren();
	CreatedLoadoutSlotWidgets.Reset();

	if (WidgetMode != EInventoryWidgetMode::PlayerInventory
		&& WidgetMode != EInventoryWidgetMode::StorageView
		&& WidgetMode != EInventoryWidgetMode::UpgradeSelect
		&& WidgetMode != EInventoryWidgetMode::DeadPlayerLootView)
	{
		return;
	}

	const int32 SafeColumnCount = FMath::Max(1, LoadoutGridColumns);
	CreatedLoadoutSlotWidgets.SetNum(CachedLoadoutSlots.Num());

	for (int32 SlotIndex = 0; SlotIndex < CachedLoadoutSlots.Num(); ++SlotIndex)
	{
		const FFrontierLoadoutSlot& SlotData = CachedLoadoutSlots[SlotIndex];
		UFrontierLoadoutSlotWidget* SlotWidget = CreateWidget<UFrontierLoadoutSlotWidget>(this, LoadoutSlotWidgetClass);
		if (!SlotWidget)
		{
			continue;
		}

		SlotWidget->SetSlotData(SlotData.SlotType, SlotData);
		SlotWidget->SetOwningInventoryWidget(this);
		SlotWidget->OnSlotClicked.AddDynamic(this, &UFrontierInventoryWidget::HandleLoadoutSlotClicked);
		SlotWidget->OnSlotDoubleClicked.AddDynamic(this, &UFrontierInventoryWidget::HandleLoadoutSlotDoubleClicked);
		SlotWidget->OnSlotDropped.AddDynamic(this, &UFrontierInventoryWidget::HandleLoadoutSlotDropped);
		SlotWidget->SetIsEnabled(!IsUpgradeSelectMode() || CanUseLoadoutSlotForUpgradeSelection(SlotData));
		CreatedLoadoutSlotWidgets[SlotIndex] = SlotWidget;

		if (UUniformGridSlot* GridSlot = LoadoutGrid->AddChildToUniformGrid(SlotWidget))
		{
			GridSlot->SetRow(SlotIndex / SafeColumnCount);
			GridSlot->SetColumn(SlotIndex % SafeColumnCount);
		}
	}
}

void UFrontierInventoryWidget::ApplyInventorySlotChanges(
	TArray<FFrontierInventorySlot>& CachedSlots,
	const TArray<FFrontierInventorySlot>& NewSlots,
	const bool bAffectsDisplayedGrid)
{
	TArray<int32> ChangedSlotIndices;
	const bool bSlotCountChanged = FindChangedSlotIndices(CachedSlots, NewSlots, ChangedSlotIndices);
	CachedSlots = NewSlots;

	if (!bAffectsDisplayedGrid || (!bSlotCountChanged && ChangedSlotIndices.IsEmpty()))
	{
		return;
	}

	HideItemTooltip(nullptr);
	if (bSlotCountChanged || CreatedInventorySlotWidgets.Num() != NewSlots.Num())
	{
		RebuildRaidInventoryGrid();
	}
	else
	{
		RefreshChangedInventorySlotWidgets(ChangedSlotIndices);
	}

	RefreshSelectionState();
	RefreshSelectedItemInfo();
	ApplyUpgradeSelectionEnabledState();
}

void UFrontierInventoryWidget::ApplyLoadoutSlotChanges(const TArray<FFrontierLoadoutSlot>& NewSlots, const bool bAffectsDisplayedGrid)
{
	TArray<int32> ChangedSlotIndices;
	const bool bSlotCountChanged = FindChangedSlotIndices(CachedLoadoutSlots, NewSlots, ChangedSlotIndices);
	CachedLoadoutSlots = NewSlots;

	if (!bAffectsDisplayedGrid || (!bSlotCountChanged && ChangedSlotIndices.IsEmpty()))
	{
		return;
	}

	HideItemTooltip(nullptr);
	if (bSlotCountChanged || CreatedLoadoutSlotWidgets.Num() != NewSlots.Num())
	{
		RebuildLoadoutGrid();
	}
	else
	{
		RefreshChangedLoadoutSlotWidgets(ChangedSlotIndices);
	}

	RefreshSelectionState();
	RefreshSelectedItemInfo();
	ApplyUpgradeSelectionEnabledState();
}

void UFrontierInventoryWidget::NormalizeInventorySlotsForDisplay(TArray<FFrontierInventorySlot>& InOutSlots, const UFrontierInventoryComponent* InventoryComponent) const
{
	const int32 DesiredSlotCount = InventoryComponent ? InventoryComponent->GetSlotCount() : InOutSlots.Num();
	InOutSlots.SetNum(FMath::Max(0, DesiredSlotCount));

	for (int32 SlotIndex = 0; SlotIndex < InOutSlots.Num(); ++SlotIndex)
	{
		FFrontierInventorySlot& InventorySlot = InOutSlots[SlotIndex];
		InventorySlot.SlotIndex = SlotIndex;
		if (!InventorySlot.bOccupied || !InventorySlot.ItemInstance.IsValid())
		{
			InventorySlot.bOccupied = false;
			InventorySlot.ItemInstance = FFrontierItemInstance();
		}
	}
}

void UFrontierInventoryWidget::NormalizeLoadoutSlotsForDisplay(TArray<FFrontierLoadoutSlot>& InOutSlots) const
{
	if (InOutSlots.IsEmpty())
	{
		const EFrontierEquipmentSlot DefaultSlots[] = {
			EFrontierEquipmentSlot::MainWeapon,
			EFrontierEquipmentSlot::SubWeapon,
			EFrontierEquipmentSlot::Helmet,
			EFrontierEquipmentSlot::Chest,
			EFrontierEquipmentSlot::Gloves,
			EFrontierEquipmentSlot::Boots,
			EFrontierEquipmentSlot::Necklace,
			EFrontierEquipmentSlot::Ring
		};

		for (const EFrontierEquipmentSlot SlotType : DefaultSlots)
		{
			FFrontierLoadoutSlot LoadoutSlot;
			LoadoutSlot.SlotType = SlotType;
			InOutSlots.Add(LoadoutSlot);
		}
	}

	for (FFrontierLoadoutSlot& LoadoutSlot : InOutSlots)
	{
		if (!LoadoutSlot.bOccupied || !LoadoutSlot.ItemInstance.IsValid())
		{
			LoadoutSlot.bOccupied = false;
			LoadoutSlot.ItemInstance = FFrontierItemInstance();
		}
	}
}

void UFrontierInventoryWidget::RefreshChangedInventorySlotWidgets(const TArray<int32>& ChangedSlotIndices)
{
	const TArray<FFrontierInventorySlot>& DisplaySlots = GetDisplayedInventorySlots();
	for (const int32 SlotIndex : ChangedSlotIndices)
	{
		if (!DisplaySlots.IsValidIndex(SlotIndex) || !CreatedInventorySlotWidgets.IsValidIndex(SlotIndex) || !CreatedInventorySlotWidgets[SlotIndex])
		{
			RebuildRaidInventoryGrid();
			return;
		}

		CreatedInventorySlotWidgets[SlotIndex]->SetSlotData(SlotIndex, DisplaySlots[SlotIndex]);
		CreatedInventorySlotWidgets[SlotIndex]->SetIsEnabled(!IsUpgradeSelectMode() || CanUseSlotForUpgradeSelection(DisplaySlots[SlotIndex]));
	}
}

void UFrontierInventoryWidget::RefreshChangedLoadoutSlotWidgets(const TArray<int32>& ChangedSlotIndices)
{
	for (const int32 SlotIndex : ChangedSlotIndices)
	{
		if (!CachedLoadoutSlots.IsValidIndex(SlotIndex) || !CreatedLoadoutSlotWidgets.IsValidIndex(SlotIndex) || !CreatedLoadoutSlotWidgets[SlotIndex])
		{
			RebuildLoadoutGrid();
			return;
		}

		const FFrontierLoadoutSlot& SlotData = CachedLoadoutSlots[SlotIndex];
		CreatedLoadoutSlotWidgets[SlotIndex]->SetSlotData(SlotData.SlotType, SlotData);
		CreatedLoadoutSlotWidgets[SlotIndex]->SetIsEnabled(!IsUpgradeSelectMode() || CanUseLoadoutSlotForUpgradeSelection(SlotData));
	}
}

const TArray<FFrontierInventorySlot>& UFrontierInventoryWidget::GetDisplayedInventorySlots() const
{
	if (WidgetMode == EInventoryWidgetMode::PlayerInventory || WidgetMode == EInventoryWidgetMode::UpgradeSelect)
	{
		return CachedRaidSlots;
	}

	if (WidgetMode == EInventoryWidgetMode::StorageView)
	{
		return CachedLobbySlots;
	}

	return CachedLootSlots;
}

bool UFrontierInventoryWidget::CanUseSlotForUpgradeSelection(const FFrontierInventorySlot& InventorySlot) const
{
	return InventorySlot.bOccupied && IsEquipmentItemInstance(InventorySlot.ItemInstance);
}

bool UFrontierInventoryWidget::CanUseLoadoutSlotForUpgradeSelection(const FFrontierLoadoutSlot& LoadoutSlot) const
{
	return LoadoutSlot.bOccupied && IsEquipmentItemInstance(LoadoutSlot.ItemInstance);
}

void UFrontierInventoryWidget::ApplyUpgradeSelectionEnabledState()
{
	if (!IsUpgradeSelectMode())
	{
		for (UFrontierInventorySlotWidget* SlotWidget : CreatedInventorySlotWidgets)
		{
			if (SlotWidget)
			{
				SlotWidget->SetIsEnabled(true);
			}
		}
		for (UFrontierLoadoutSlotWidget* SlotWidget : CreatedLoadoutSlotWidgets)
		{
			if (SlotWidget)
			{
				SlotWidget->SetIsEnabled(true);
			}
		}
		return;
	}

	const TArray<FFrontierInventorySlot>& DisplaySlots = GetDisplayedInventorySlots();
	for (UFrontierInventorySlotWidget* SlotWidget : CreatedInventorySlotWidgets)
	{
		if (SlotWidget)
		{
			const int32 SlotIndex = SlotWidget->GetSlotIndex();
			SlotWidget->SetIsEnabled(DisplaySlots.IsValidIndex(SlotIndex) && CanUseSlotForUpgradeSelection(DisplaySlots[SlotIndex]));
		}
	}

	for (UFrontierLoadoutSlotWidget* SlotWidget : CreatedLoadoutSlotWidgets)
	{
		if (SlotWidget)
		{
			const FFrontierLoadoutSlot SlotData = SlotWidget->GetSlotData();
			SlotWidget->SetIsEnabled(CanUseLoadoutSlotForUpgradeSelection(SlotData));
		}
	}
}

void UFrontierInventoryWidget::BroadcastUpgradeItemSelected(const FFrontierItemInstance& ItemInstance)
{
	if (!IsEquipmentItemInstance(ItemInstance))
	{
		return;
	}

	OnUpgradeItemSelected.Broadcast(ItemInstance);
}

bool UFrontierInventoryWidget::TryEquipRaidInventoryItem(const int32 SlotIndex)
{
	if (!CachedRaidSlots.IsValidIndex(SlotIndex)
		|| !CachedRaidSlots[SlotIndex].bOccupied
		|| !CachedRaidSlots[SlotIndex].ItemInstance.IsValid())
	{
		return false;
	}

	const FFrontierItemInstance& ItemInstance = CachedRaidSlots[SlotIndex].ItemInstance;
	EFrontierEquipmentSlot EquipSlot = EFrontierEquipmentSlot::None;
	const EFrontierItemCategory ItemCategory = ItemInstance.GetCategory();
	if (ItemCategory == EFrontierItemCategory::Weapon
		|| ItemCategory == EFrontierItemCategory::Armor
		|| ItemCategory == EFrontierItemCategory::Accessory)
	{
		EquipSlot = ItemInstance.GetEquipSlot();
	}
	else
	{
		const TArray<EFrontierEquipmentSlot>& AllowedEquipSlots = ItemInstance.GetAllowedEquipSlots();
		if (!AllowedEquipSlots.IsEmpty())
		{
			EquipSlot = AllowedEquipSlots[0];
		}
	}

	if (EquipSlot == EFrontierEquipmentSlot::None)
	{
		return false;
	}

	if (!ServerEquipRaidInventoryItemToLoadout(GetOwningPlayer(), SlotIndex, EquipSlot))
	{
		return false;
	}

	ClearSelection();
	return true;
}

void UFrontierInventoryWidget::RefreshSelectionState()
{
	for (UFrontierInventorySlotWidget* SlotWidget : CreatedInventorySlotWidgets)
	{
		if (SlotWidget)
		{
			SlotWidget->SetSelected(SlotWidget->GetSlotIndex() == SelectedInventorySlotIndex);
		}
	}

	for (UFrontierLoadoutSlotWidget* SlotWidget : CreatedLoadoutSlotWidgets)
	{
		if (SlotWidget)
		{
			SlotWidget->SetSelected(SlotWidget->GetSlotType() == SelectedLoadoutSlotType);
		}
	}
}

void UFrontierInventoryWidget::HandleInventorySlotClicked(UFrontierInventorySlotWidget* SlotWidget, const int32 SlotIndex)
{
	HideItemTooltip(nullptr);

	if (IsUpgradeSelectMode())
	{
		return;
	}

	if (WidgetMode != EInventoryWidgetMode::PlayerInventory)
	{
		if (WidgetMode == EInventoryWidgetMode::StorageView)
		{
			ServerWithdrawLobbyItemToRaidInventory(GetOwningPlayer(), SlotIndex);
		}
		if (AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>())
		{
			if (WidgetMode == EInventoryWidgetMode::LootView || WidgetMode == EInventoryWidgetMode::DeadPlayerLootView)
			{
				FrontierPlayerController->ServerLootContainerItem(ObservedLootContainer, SlotIndex);
			}
		}
		return;
	}

	SelectedLoadoutSlotType = EFrontierEquipmentSlot::None;
	SelectedInventorySlotIndex = (SelectedInventorySlotIndex == SlotIndex) ? INDEX_NONE : SlotIndex;
	RefreshSelectionState();
	RefreshSelectedItemInfo();
}

void UFrontierInventoryWidget::HandleLoadoutSlotClicked(UFrontierLoadoutSlotWidget* SlotWidget, const EFrontierEquipmentSlot SlotType)
{
	HideItemTooltip(nullptr);

	if (IsUpgradeSelectMode())
	{
		return;
	}

	if (WidgetMode == EInventoryWidgetMode::DeadPlayerLootView)
	{
		if (AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>())
		{
			FrontierPlayerController->ServerLootContainerLoadoutItem(ObservedLootContainer, SlotType);
		}
		return;
	}

	if (WidgetMode != EInventoryWidgetMode::PlayerInventory)
	{
		return;
	}

	if (SelectedInventorySlotIndex != INDEX_NONE)
	{
		ServerEquipRaidInventoryItemToLoadout(GetOwningPlayer(), SelectedInventorySlotIndex, SlotType);
		ClearSelection();
	}
	else
	{
		SelectedInventorySlotIndex = INDEX_NONE;
		SelectedLoadoutSlotType = (SelectedLoadoutSlotType == SlotType) ? EFrontierEquipmentSlot::None : SlotType;
		RefreshSelectionState();
		RefreshSelectedItemInfo();
	}
}

void UFrontierInventoryWidget::HandleInventorySlotDoubleClicked(UFrontierInventorySlotWidget* SlotWidget, const int32 SlotIndex)
{
	HideItemTooltip(nullptr);

	if (IsUpgradeSelectMode())
	{
		if (CachedRaidSlots.IsValidIndex(SlotIndex))
		{
			BroadcastUpgradeItemSelected(CachedRaidSlots[SlotIndex].ItemInstance);
		}
		return;
	}

	if (WidgetMode != EInventoryWidgetMode::PlayerInventory)
	{
		if (WidgetMode == EInventoryWidgetMode::StorageView)
		{
			ServerWithdrawLobbyItemToRaidInventory(GetOwningPlayer(), SlotIndex);
		}
		if (AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>())
		{
			if (WidgetMode == EInventoryWidgetMode::LootView || WidgetMode == EInventoryWidgetMode::DeadPlayerLootView)
			{
				FrontierPlayerController->ServerLootContainerItem(ObservedLootContainer, SlotIndex);
			}
		}
		return;
	}

	if (!CachedRaidSlots.IsValidIndex(SlotIndex))
	{
		return;
	}

	const FFrontierItemInstance& ItemInstance = CachedRaidSlots[SlotIndex].ItemInstance;

	if (APlayerController* OwningController = GetOwningPlayer())
	{
		const EFrontierItemCategory ItemCategory = ItemInstance.GetCategory();
		if (ItemCategory == EFrontierItemCategory::Weapon
			|| ItemCategory == EFrontierItemCategory::Armor
			|| ItemCategory == EFrontierItemCategory::Accessory)
		{
			const EFrontierEquipmentSlot EquipSlot = ItemInstance.GetEquipSlot();
			if (EquipSlot == EFrontierEquipmentSlot::None)
			{
				return;
			}
			ServerEquipRaidInventoryItemToLoadout(OwningController, SlotIndex, EquipSlot);
			ClearSelection();
			return;
		}

		if (ItemCategory == EFrontierItemCategory::Consumable)
		{
			if (ItemInstance.IsUsableInRaid())
			{
				if (AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(OwningController))
				{
					FrontierPlayerController->ServerUseRaidInventoryItem(SlotIndex);
				}
				ClearSelection();
				return;
			}

			const TArray<EFrontierEquipmentSlot>& AllowedEquipSlots = ItemInstance.GetAllowedEquipSlots();
			if (!AllowedEquipSlots.IsEmpty())
			{
				ServerEquipRaidInventoryItemToLoadout(OwningController, SlotIndex, AllowedEquipSlots[0]);
				ClearSelection();
			}
		}
	}
}

void UFrontierInventoryWidget::HandleInventorySlotRightClicked(UFrontierInventorySlotWidget* SlotWidget, const int32 SlotIndex)
{
	HideItemTooltip(nullptr);

	if (IsUpgradeSelectMode() || WidgetMode != EInventoryWidgetMode::PlayerInventory)
	{
		return;
	}

	TryEquipRaidInventoryItem(SlotIndex);
}

void UFrontierInventoryWidget::HandleInventorySlotAltRightClicked(UFrontierInventorySlotWidget* SlotWidget, const int32 SlotIndex)
{
	HideItemTooltip(nullptr);

	if (IsUpgradeSelectMode())
	{
		return;
	}

	if (WidgetMode == EInventoryWidgetMode::PlayerInventory)
	{
		AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>();
		if (!FrontierPlayerController || FrontierPlayerController->IsLobbyStorageUIOpen())
		{
			ServerStoreRaidItemInStorage(GetOwningPlayer(), SlotIndex);
			return;
		}

		if (AFrontierLootContainerActor* LootContainer = FrontierPlayerController->GetCurrentOpenedLootContainer())
		{
			FrontierPlayerController->ServerStoreRaidItemInLootContainer(LootContainer, SlotIndex);
		}

		return;
	}

	if (WidgetMode == EInventoryWidgetMode::StorageView)
	{
		ServerWithdrawLobbyItemToRaidInventory(GetOwningPlayer(), SlotIndex);
		return;
	}

	if ((WidgetMode == EInventoryWidgetMode::LootView || WidgetMode == EInventoryWidgetMode::DeadPlayerLootView) && ObservedLootContainer)
	{
		if (AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>())
		{
			FrontierPlayerController->ServerLootContainerItem(ObservedLootContainer, SlotIndex);
		}
	}
}

void UFrontierInventoryWidget::HandleLoadoutSlotDoubleClicked(UFrontierLoadoutSlotWidget* SlotWidget, const EFrontierEquipmentSlot SlotType)
{
	HideItemTooltip(nullptr);

	if (IsUpgradeSelectMode())
	{
		if (SlotWidget)
		{
			BroadcastUpgradeItemSelected(SlotWidget->GetSlotData().ItemInstance);
		}
		return;
	}

	if (WidgetMode == EInventoryWidgetMode::DeadPlayerLootView)
	{
		if (AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>())
		{
			FrontierPlayerController->ServerLootContainerLoadoutItem(ObservedLootContainer, SlotType);
		}
		return;
	}

	if (WidgetMode != EInventoryWidgetMode::PlayerInventory)
	{
		if (WidgetMode != EInventoryWidgetMode::StorageView)
		{
			return;
		}
	}

	ServerUnequipLoadoutItemToRaidInventory(GetOwningPlayer(), SlotType);
	ClearSelection();
}

void UFrontierInventoryWidget::HandleInventorySlotDropped(UFrontierInventorySlotWidget* SlotWidget, const int32 SlotIndex, UFrontierInventoryDragDropOperation* DragOperation)
{
	HideItemTooltip(nullptr);

	if (!DragOperation)
	{
		return;
	}

	if (IsUpgradeSelectMode())
	{
		return;
	}

	if (APlayerController* OwningController = GetOwningPlayer())
	{
		if (WidgetMode == EInventoryWidgetMode::PlayerInventory
			&& DragOperation->SourceType == EFrontierDragSourceType::DroppedItem
			&& IsValid(DragOperation->SourceDroppedItem))
		{
			if (AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(OwningController))
			{
				if (SlotWidget && !SlotWidget->IsSlotOccupied())
				{
					FrontierPlayerController->ServerPickupDroppedItemToRaidSlot(DragOperation->SourceDroppedItem, SlotIndex);
				}
				else
				{
					FrontierPlayerController->ServerPickupDroppedItem(DragOperation->SourceDroppedItem);
				}
				ClearSelection();
			}
			return;
		}

		if (WidgetMode == EInventoryWidgetMode::StorageView)
		{
			if (DragOperation->SourceType == EFrontierDragSourceType::InventorySlot)
			{
				if (DragOperation->SourceInventoryCollection == EFrontierInventoryCollectionType::Storage)
				{
					ServerSwapStorageSlots(OwningController, DragOperation->SourceInventorySlotIndex, SlotIndex);
					ClearSelection();
					return;
				}

				if (DragOperation->SourceInventoryCollection == EFrontierInventoryCollectionType::RaidInventory)
				{
					ServerStoreRaidItemInStorageSlot(OwningController, DragOperation->SourceInventorySlotIndex, SlotIndex);
					ClearSelection();
					return;
				}
			}

			return;
		}

		if (WidgetMode != EInventoryWidgetMode::PlayerInventory)
		{
			return;
		}

		if (DragOperation->SourceType == EFrontierDragSourceType::InventorySlot)
		{
			if (DragOperation->SourceInventoryCollection == EFrontierInventoryCollectionType::Storage)
			{
				ServerWithdrawLobbyItemToRaidInventorySlot(OwningController, DragOperation->SourceInventorySlotIndex, SlotIndex);
				ClearSelection();
				return;
			}

			ServerSwapRaidInventorySlots(OwningController, DragOperation->SourceInventorySlotIndex, SlotIndex);
			ClearSelection();
			return;
		}

		if (DragOperation->SourceType == EFrontierDragSourceType::LoadoutSlot)
		{
			ServerUnequipLoadoutItemToRaidInventorySlot(OwningController, DragOperation->SourceLoadoutSlotType, SlotIndex);
			ClearSelection();
		}
	}
}

void UFrontierInventoryWidget::HandleLoadoutSlotDropped(UFrontierLoadoutSlotWidget* SlotWidget, const EFrontierEquipmentSlot SlotType, UFrontierInventoryDragDropOperation* DragOperation)
{
	HideItemTooltip(nullptr);

	if (IsUpgradeSelectMode())
	{
		return;
	}

	if (WidgetMode != EInventoryWidgetMode::PlayerInventory)
	{
		return;
	}

	if (!DragOperation)
	{
		return;
	}

	if (APlayerController* OwningController = GetOwningPlayer())
	{
		if (DragOperation->SourceType == EFrontierDragSourceType::InventorySlot)
		{
			ServerEquipRaidInventoryItemToLoadout(OwningController, DragOperation->SourceInventorySlotIndex, SlotType);
			ClearSelection();
		}
	}
}

void UFrontierInventoryWidget::RefreshSelectedItemInfo()
{
	const FFrontierItemInstance* SelectedItemInstance = nullptr;

	const TArray<FFrontierInventorySlot>* DisplaySlots = &CachedLootSlots;
	if (WidgetMode == EInventoryWidgetMode::PlayerInventory)
	{
		DisplaySlots = &CachedRaidSlots;
	}
	else if (WidgetMode == EInventoryWidgetMode::StorageView)
	{
		DisplaySlots = &CachedLobbySlots;
	}

	if (SelectedInventorySlotIndex != INDEX_NONE && DisplaySlots->IsValidIndex(SelectedInventorySlotIndex))
	{
		const FFrontierInventorySlot& SelectedInventorySlot = (*DisplaySlots)[SelectedInventorySlotIndex];
		if (SelectedInventorySlot.bOccupied && SelectedInventorySlot.ItemInstance.IsValid())
		{
			SelectedItemInstance = &SelectedInventorySlot.ItemInstance;
		}
	}
	else if (SelectedLoadoutSlotType != EFrontierEquipmentSlot::None)
	{
		for (const FFrontierLoadoutSlot& LoadoutSlot : CachedLoadoutSlots)
		{
			if (LoadoutSlot.SlotType == SelectedLoadoutSlotType && LoadoutSlot.bOccupied && LoadoutSlot.ItemInstance.IsValid())
			{
				SelectedItemInstance = &LoadoutSlot.ItemInstance;
				break;
			}
		}
	}

	const TSoftObjectPtr<UTexture2D> ItemIconPtr = SelectedItemInstance ? SelectedItemInstance->GetIcon() : TSoftObjectPtr<UTexture2D>();
	UTexture2D* ItemIcon = !ItemIconPtr.IsNull() ? ItemIconPtr.LoadSynchronous() : nullptr;

	if (SelectedItemIconImage)
	{
		SelectedItemIconImage->SetBrushFromTexture(ItemIcon, true);
		SelectedItemIconImage->SetVisibility(ItemIcon ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}

	if (SelectedItemRarityBorderWidget)
	{
		if (SelectedItemInstance && SelectedItemInstance->IsValid())
		{
			SelectedItemRarityBorderWidget->SetItemRarity(SelectedItemInstance->GetDisplayRarity());
		}
		else
		{
			SelectedItemRarityBorderWidget->ClearRarity();
		}
	}

	if (SelectedItemNameText)
	{
		SelectedItemNameText->SetText(SelectedItemInstance ? SelectedItemInstance->GetDisplayNameText() : FText::GetEmpty());
	}

	if (SelectedItemDescriptionText)
	{
		SelectedItemDescriptionText->SetText(SelectedItemInstance ? SelectedItemInstance->GetDescriptionText() : FText::GetEmpty());
	}

	if (SelectedItemTypeText)
	{
		SelectedItemTypeText->SetText(SelectedItemInstance ? FText::FromString(UEnum::GetValueAsString(SelectedItemInstance->GetCategory())) : FText::GetEmpty());
	}

	if (SelectedItemStatsText)
	{
		if (!SelectedItemInstance || !SelectedItemInstance->IsValid())
		{
			SelectedItemStatsText->SetText(FText::GetEmpty());
		}
		else
		{
			FString StatsText = FString::Printf(
				TEXT("ItemID: %s\nQty: %d\nEnhance: %d\nDurability: %.0f"),
				*SelectedItemInstance->GetTemplateId().ToString(),
				SelectedItemInstance->Quantity,
				SelectedItemInstance->EnhancementLevel,
				SelectedItemInstance->Durability);

			if (SelectedItemInstance->ItemInstanceId.IsValid())
			{
				StatsText += FString::Printf(
					TEXT("\nInstanceID: %s"),
					*SelectedItemInstance->ItemInstanceId.ToString(EGuidFormats::DigitsWithHyphens));
			}

			if (SelectedItemInstance->GetCategory() == EFrontierItemCategory::Weapon
				|| SelectedItemInstance->GetCategory() == EFrontierItemCategory::Armor
				|| SelectedItemInstance->GetCategory() == EFrontierItemCategory::Accessory)
			{
				for (const FFrontierRuntimeStatData& InstanceStat : SelectedItemInstance->RuntimeGeneratedStats)
				{
					const FString OptionName = InstanceStat.StatTag.IsValid() ? InstanceStat.StatTag.GetTagName().ToString() : InstanceStat.OptionId;
					if (!OptionName.IsEmpty())
					{
						StatsText += FString::Printf(TEXT("\n%s: %.0f"), *OptionName, InstanceStat.FinalValue);
					}
				}
			}
			else if (SelectedItemInstance->GetCategory() == EFrontierItemCategory::Consumable)
			{
				StatsText += FString::Printf(TEXT("\nHeal Amount: %.0f\nUse Time: %.1f\nUsable In Raid: %s"),
					SelectedItemInstance->GetHealthRestoreAmount(),
					SelectedItemInstance->GetUseTime(),
					SelectedItemInstance->IsUsableInRaid() ? TEXT("Yes") : TEXT("No"));
			}

			SelectedItemStatsText->SetText(FText::FromString(StatsText));
		}
	}
}

void UFrontierInventoryWidget::ClearSelection()
{
	SelectedInventorySlotIndex = INDEX_NONE;
	SelectedLoadoutSlotType = EFrontierEquipmentSlot::None;
	RefreshSelectionState();
	RefreshSelectedItemInfo();
}

void UFrontierInventoryWidget::BindInventorySources()
{
	UnbindInventorySources();

	BoundStorageComponent = GetStorageComponent();
	BoundRaidInventoryComponent = (WidgetMode == EInventoryWidgetMode::PlayerInventory || WidgetMode == EInventoryWidgetMode::UpgradeSelect)
		? GetRaidInventoryComponent()
		: nullptr;
	BoundLoadoutComponent = GetLoadoutComponent();
	BoundLootInventoryComponent = WidgetMode == EInventoryWidgetMode::LootView ? GetLootInventoryComponent() : nullptr;

	if (BoundStorageComponent)
	{
		BoundStorageComponent->OnInventoryChanged.AddDynamic(this, &UFrontierInventoryWidget::HandleStorageChanged);
	}

	if (BoundRaidInventoryComponent)
	{
		BoundRaidInventoryComponent->OnInventoryChanged.AddDynamic(this, &UFrontierInventoryWidget::HandleRaidInventoryChanged);
	}

	if (BoundLoadoutComponent)
	{
		BoundLoadoutComponent->OnLoadoutChanged.AddDynamic(this, &UFrontierInventoryWidget::HandleLoadoutChanged);
	}

	if (BoundLootInventoryComponent)
	{
		BoundLootInventoryComponent->OnInventoryChanged.AddDynamic(this, &UFrontierInventoryWidget::HandleLootInventoryChanged);
	}

	if (WidgetMode == EInventoryWidgetMode::DeadPlayerLootView && ObservedLootContainer)
	{
		ObservedLootContainer->OnDeadPlayerInventoryChanged.AddDynamic(this, &UFrontierInventoryWidget::HandleDeadPlayerInventoryChanged);
		ObservedLootContainer->OnDeadPlayerLoadoutChanged.AddDynamic(this, &UFrontierInventoryWidget::HandleDeadPlayerLoadoutChanged);
	}
}

void UFrontierInventoryWidget::EnsureItemTooltipWidget()
{
	if (ItemTooltipWidget || !ItemTooltipWidgetClass)
	{
		if (!ItemTooltipWidgetClass)
		{
			FRONTIER_LOG(Warning, TEXT("EnsureItemTooltipWidget failed because ItemTooltipWidgetClass is null."));
		}
		return;
	}

	ItemTooltipWidget = CreateWidget<UFrontierItemTooltipWidget>(GetOwningPlayer(), ItemTooltipWidgetClass);
	if (!ItemTooltipWidget)
	{
		FRONTIER_LOG(Warning, TEXT("EnsureItemTooltipWidget failed because CreateWidget returned null."));
		return;
	}

	
	ItemTooltipWidget->AddToViewport(50);
	ItemTooltipWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void UFrontierInventoryWidget::ShowItemTooltipInternal(
	UWidget* SourceWidget,
	const FFrontierItemInstance& ItemInstance,
	const bool bPlaceToLeft)
{
	if (!SourceWidget || !ItemInstance.IsValid())
	{
		return;
	}

	EnsureItemTooltipWidget();
	if (!ItemTooltipWidget)
	{
		FRONTIER_LOG(Warning, TEXT("ShowItemTooltipInternal aborted because ItemTooltipWidget is null."));
		return;
	}

	TooltipSourceWidget = SourceWidget;
	ItemTooltipWidget->SetItemInstance(ItemInstance);
	ItemTooltipWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	// ForceLayoutPrepass();
	// ItemTooltipWidget->ForceLayoutPrepass();
	FRONTIER_LOG(Log, TEXT("Showing item tooltip. SourceWidget=%s"), *GetNameSafe(SourceWidget));
	PositionTooltipWidget(SourceWidget, bPlaceToLeft);
}

void UFrontierInventoryWidget::PositionTooltipWidget(UWidget* SourceWidget, const bool bPlaceToLeft)
{
	if (!ItemTooltipWidget || !SourceWidget)
	{
		return;
	}

	const FGeometry SourceGeometry = SourceWidget->GetCachedGeometry();
	const FVector2D SourceAbsolutePosition = SourceGeometry.GetAbsolutePosition();
	const FVector2D SourceSize = SourceGeometry.GetLocalSize();

	FVector2D PixelPosition;
	FVector2D ViewportPosition;
	USlateBlueprintLibrary::AbsoluteToViewport(GetWorld(), SourceAbsolutePosition, PixelPosition, ViewportPosition);

	ItemTooltipWidget->ForceLayoutPrepass();
	const FVector2D TooltipDesiredSize = ItemTooltipWidget->GetTooltipContentDesiredSize();
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this).GetLocalSize();
	const FVector2D TooltipAnchorPosition = bPlaceToLeft
		? UWidgetLayoutLibrary::GetMousePositionOnViewport(this)
		: ViewportPosition;
	FVector2D FinalPosition(
		bPlaceToLeft
			? FMath::Max(0.0f, TooltipAnchorPosition.X - TooltipDesiredSize.X - TooltipOffsetX)
			: ViewportPosition.X + SourceSize.X + TooltipOffsetX,
		bPlaceToLeft
			? TooltipAnchorPosition.Y - (TooltipDesiredSize.Y * 0.5f)
			: ViewportPosition.Y + ((SourceSize.Y - TooltipDesiredSize.Y) * 0.5f));

	if (!bPlaceToLeft && ViewportSize.X > 0.0f && FinalPosition.X + TooltipDesiredSize.X > ViewportSize.X)
	{
		FinalPosition.X = ViewportPosition.X - TooltipDesiredSize.X - TooltipOffsetX;
	}
	if (!ViewportSize.IsNearlyZero())
	{
		FinalPosition.X = FMath::Clamp(FinalPosition.X, 0.0f, FMath::Max(0.0f, ViewportSize.X - TooltipDesiredSize.X));
		FinalPosition.Y = FMath::Clamp(FinalPosition.Y, 0.0f, FMath::Max(0.0f, ViewportSize.Y - TooltipDesiredSize.Y));
	}

	ItemTooltipWidget->SetTooltipViewportPositionExact(FinalPosition);
}

void UFrontierInventoryWidget::UnbindInventorySources()
{
	if (BoundStorageComponent)
	{
		BoundStorageComponent->OnInventoryChanged.RemoveDynamic(this, &UFrontierInventoryWidget::HandleStorageChanged);
		BoundStorageComponent = nullptr;
	}

	if (BoundRaidInventoryComponent)
	{
		BoundRaidInventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UFrontierInventoryWidget::HandleRaidInventoryChanged);
		BoundRaidInventoryComponent = nullptr;
	}

	if (BoundLoadoutComponent)
	{
		BoundLoadoutComponent->OnLoadoutChanged.RemoveDynamic(this, &UFrontierInventoryWidget::HandleLoadoutChanged);
		BoundLoadoutComponent = nullptr;
	}

	if (BoundLootInventoryComponent)
	{
		BoundLootInventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UFrontierInventoryWidget::HandleLootInventoryChanged);
		BoundLootInventoryComponent = nullptr;
	}

	if (ObservedLootContainer)
	{
		ObservedLootContainer->OnDeadPlayerInventoryChanged.RemoveDynamic(this, &UFrontierInventoryWidget::HandleDeadPlayerInventoryChanged);
		ObservedLootContainer->OnDeadPlayerLoadoutChanged.RemoveDynamic(this, &UFrontierInventoryWidget::HandleDeadPlayerLoadoutChanged);
	}
}

void UFrontierInventoryWidget::UpdateCachedData()
{
	CachedLobbySlots = BoundStorageComponent ? BoundStorageComponent->GetSlots() : TArray<FFrontierInventorySlot>();
	CachedRaidSlots = BoundRaidInventoryComponent ? BoundRaidInventoryComponent->GetSlots() : TArray<FFrontierInventorySlot>();
	NormalizeInventorySlotsForDisplay(CachedLobbySlots, BoundStorageComponent);
	NormalizeInventorySlotsForDisplay(CachedRaidSlots, BoundRaidInventoryComponent);

	if (WidgetMode == EInventoryWidgetMode::DeadPlayerLootView && ObservedLootContainer)
	{
		CachedLoadoutSlots = ObservedLootContainer->GetDeadPlayerLoadoutItems();
		CachedLootSlots = ObservedLootContainer->GetDeadPlayerInventory();
		return;
	}

	CachedLoadoutSlots = BoundLoadoutComponent ? BoundLoadoutComponent->GetLoadoutSlots() : TArray<FFrontierLoadoutSlot>();
	NormalizeLoadoutSlotsForDisplay(CachedLoadoutSlots);
	CachedLootSlots = BoundLootInventoryComponent ? BoundLootInventoryComponent->GetSlots() : TArray<FFrontierInventorySlot>();
}

