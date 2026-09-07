#include "UI/FrontierLootWidget.h"

#include "Components/FrontierLootInventoryComponent.h"
#include "Components/FrontierRaidInventoryComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierPlayerState.h"
#include "Inventory/Items/FrontierArmorItemDataAsset.h"
#include "Inventory/Items/FrontierConsumableItemDataAsset.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "Inventory/Items/FrontierWeaponItemDataAsset.h"
#include "Loot/FrontierLootContainerActor.h"
#include "UI/FrontierInventorySlotWidget.h"
#include "UI/FrontierInventoryWidget.h"
#include "UI/FrontierItemRarityBorderWidget.h"

namespace
{
	bool FindChangedInventorySlotIndices(
		const TArray<FFrontierInventorySlot>& PreviousSlots,
		const TArray<FFrontierInventorySlot>& NewSlots,
		TArray<int32>& OutChangedSlotIndices)
	{
		OutChangedSlotIndices.Reset();
		if (PreviousSlots.Num() != NewSlots.Num())
		{
			return true;
		}

		for (int32 SlotIndex = 0; SlotIndex < NewSlots.Num(); ++SlotIndex)
		{
			if (!FFrontierInventorySlot::StaticStruct()->CompareScriptStruct(&PreviousSlots[SlotIndex], &NewSlots[SlotIndex], PPF_None))
			{
				OutChangedSlotIndices.Add(SlotIndex);
			}
		}

		return false;
	}
}

void UFrontierLootWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshFromSources();
}

void UFrontierLootWidget::NativeDestruct()
{
	UnbindLootSource();
	Super::NativeDestruct();
}

void UFrontierLootWidget::SetObservedLootContainer(AFrontierLootContainerActor* InLootContainer)
{
	if (ObservedLootContainer == InLootContainer)
	{
		return;
	}

	UnbindLootSource();
	ObservedLootContainer = InLootContainer;
	RefreshFromSources();
}

void UFrontierLootWidget::SetObservedPlayerState(AFrontierPlayerState* InPlayerState)
{
	ObservedPlayerState = InPlayerState;
	RefreshFromSources();
}

void UFrontierLootWidget::SetOwningInventoryWidget(UFrontierInventoryWidget* InInventoryWidget)
{
	OwningInventoryWidget = InInventoryWidget;
}

void UFrontierLootWidget::HandleLootInventoryChanged(const TArray<FFrontierInventorySlot>& InSlots)
{
	ApplyInventorySlotChanges(CachedLootSlots, InSlots, true);
}

void UFrontierLootWidget::HandleRaidInventoryChanged(const TArray<FFrontierInventorySlot>& InSlots)
{
	ApplyInventorySlotChanges(CachedRaidSlots, InSlots, false);
}

void UFrontierLootWidget::RefreshFromSources()
{
	UnbindLootSource();
	RefreshLootActorText();

	BoundLootInventoryComponent = ObservedLootContainer ? ObservedLootContainer->GetLootInventoryComponent() : nullptr;
	if (BoundLootInventoryComponent)
	{
		BoundLootInventoryComponent->OnInventoryChanged.AddDynamic(this, &UFrontierLootWidget::HandleLootInventoryChanged);
		CachedLootSlots = BoundLootInventoryComponent->GetSlots();
	}
	else
	{
		CachedLootSlots.Reset();
	}

	BoundRaidInventoryComponent = ObservedPlayerState ? ObservedPlayerState->GetRaidInventoryComponent() : nullptr;
	if (BoundRaidInventoryComponent)
	{
		BoundRaidInventoryComponent->OnInventoryChanged.AddDynamic(this, &UFrontierLootWidget::HandleRaidInventoryChanged);
		CachedRaidSlots = BoundRaidInventoryComponent->GetSlots();
	}
	else
	{
		CachedRaidSlots.Reset();
	}

	RebuildLootGrid();
	RebuildRaidInventoryGrid();
	RefreshSelection();
	RefreshSelectedItemInfo();
}

void UFrontierLootWidget::RefreshLootActorText()
{
	if (!Text_lootactor)
	{
		return;
	}

	Text_lootactor->SetText(ObservedLootContainer ? ObservedLootContainer->GetLootActorDisplayName() : FText::GetEmpty());
}

void UFrontierLootWidget::RebuildLootGrid()
{
	if (!LootGrid || !InventorySlotWidgetClass)
	{
		return;
	}

	LootGrid->ClearChildren();
	CreatedLootSlotWidgets.SetNum(CachedLootSlots.Num());

	const int32 SafeColumnCount = FMath::Max(1, LootGridColumns);
	for (int32 SlotIndex = 0; SlotIndex < CachedLootSlots.Num(); ++SlotIndex)
	{
		UFrontierInventorySlotWidget* SlotWidget = CreateWidget<UFrontierInventorySlotWidget>(this, InventorySlotWidgetClass);
		if (!SlotWidget)
		{
			continue;
		}

		SlotWidget->SetSlotData(SlotIndex, CachedLootSlots[SlotIndex]);
		SlotWidget->SetInventoryCollectionType(EFrontierInventoryCollectionType::LootInventory);
		SlotWidget->SetOwningInventoryWidget(OwningInventoryWidget);
		SlotWidget->OnSlotClicked.AddDynamic(this, &UFrontierLootWidget::HandleLootSlotClicked);
		SlotWidget->OnSlotDoubleClicked.AddDynamic(this, &UFrontierLootWidget::HandleLootSlotDoubleClicked);
		SlotWidget->OnSlotDropped.AddDynamic(this, &UFrontierLootWidget::HandleLootSlotDropped);
		CreatedLootSlotWidgets[SlotIndex] = SlotWidget;

		if (UUniformGridSlot* GridSlot = LootGrid->AddChildToUniformGrid(SlotWidget))
		{
			GridSlot->SetRow(SlotIndex / SafeColumnCount);
			GridSlot->SetColumn(SlotIndex % SafeColumnCount);
		}
	}
}

void UFrontierLootWidget::RebuildRaidInventoryGrid()
{
	if (!RaidInventoryGrid || !InventorySlotWidgetClass)
	{
		return;
	}

	RaidInventoryGrid->ClearChildren();
	CreatedRaidSlotWidgets.SetNum(CachedRaidSlots.Num());

	const int32 SafeColumnCount = 6;
	for (int32 SlotIndex = 0; SlotIndex < CachedRaidSlots.Num(); ++SlotIndex)
	{
		UFrontierInventorySlotWidget* SlotWidget = CreateWidget<UFrontierInventorySlotWidget>(this, InventorySlotWidgetClass);
		if (!SlotWidget)
		{
			continue;
		}

		SlotWidget->SetSlotData(SlotIndex, CachedRaidSlots[SlotIndex]);
		SlotWidget->SetInventoryCollectionType(EFrontierInventoryCollectionType::RaidInventory);
		SlotWidget->SetOwningInventoryWidget(OwningInventoryWidget);
		SlotWidget->OnSlotClicked.AddDynamic(this, &UFrontierLootWidget::HandleRaidSlotClicked);
		SlotWidget->OnSlotDoubleClicked.AddDynamic(this, &UFrontierLootWidget::HandleRaidSlotDoubleClicked);
		SlotWidget->OnSlotDropped.AddDynamic(this, &UFrontierLootWidget::HandleRaidSlotDropped);
		CreatedRaidSlotWidgets[SlotIndex] = SlotWidget;

		if (UUniformGridSlot* GridSlot = RaidInventoryGrid->AddChildToUniformGrid(SlotWidget))
		{
			GridSlot->SetRow(SlotIndex / SafeColumnCount);
			GridSlot->SetColumn(SlotIndex % SafeColumnCount);
		}
	}
}

void UFrontierLootWidget::ApplyInventorySlotChanges(
	TArray<FFrontierInventorySlot>& CachedSlots,
	const TArray<FFrontierInventorySlot>& NewSlots,
	const bool bLootGrid)
{
	TArray<int32> ChangedSlotIndices;
	const bool bSlotCountChanged = FindChangedInventorySlotIndices(CachedSlots, NewSlots, ChangedSlotIndices);
	CachedSlots = NewSlots;

	if (!bSlotCountChanged && ChangedSlotIndices.IsEmpty())
	{
		return;
	}

	if (OwningInventoryWidget)
	{
		OwningInventoryWidget->HideItemTooltip(nullptr);
	}

	const TArray<TObjectPtr<UFrontierInventorySlotWidget>>& CreatedWidgets = bLootGrid ? CreatedLootSlotWidgets : CreatedRaidSlotWidgets;
	if (bSlotCountChanged || CreatedWidgets.Num() != NewSlots.Num())
	{
		if (bLootGrid)
		{
			RebuildLootGrid();
		}
		else
		{
			RebuildRaidInventoryGrid();
		}
	}
	else
	{
		RefreshChangedSlotWidgets(CachedSlots, ChangedSlotIndices, bLootGrid);
	}

	RefreshSelection();
	RefreshSelectedItemInfo();
}

void UFrontierLootWidget::RefreshChangedSlotWidgets(
	const TArray<FFrontierInventorySlot>& Slots,
	const TArray<int32>& ChangedSlotIndices,
	const bool bLootGrid)
{
	TArray<TObjectPtr<UFrontierInventorySlotWidget>>& CreatedWidgets = bLootGrid ? CreatedLootSlotWidgets : CreatedRaidSlotWidgets;
	for (const int32 SlotIndex : ChangedSlotIndices)
	{
		if (!Slots.IsValidIndex(SlotIndex) || !CreatedWidgets.IsValidIndex(SlotIndex) || !CreatedWidgets[SlotIndex])
		{
			if (bLootGrid)
			{
				RebuildLootGrid();
			}
			else
			{
				RebuildRaidInventoryGrid();
			}
			return;
		}

		CreatedWidgets[SlotIndex]->SetSlotData(SlotIndex, Slots[SlotIndex]);
	}
}

void UFrontierLootWidget::RefreshSelection()
{
	for (UFrontierInventorySlotWidget* SlotWidget : CreatedLootSlotWidgets)
	{
		if (SlotWidget)
		{
			SlotWidget->SetSelected(SlotWidget->GetSlotIndex() == SelectedLootSlotIndex);
		}
	}

	for (UFrontierInventorySlotWidget* SlotWidget : CreatedRaidSlotWidgets)
	{
		if (SlotWidget)
		{
			SlotWidget->SetSelected(SlotWidget->GetSlotIndex() == SelectedRaidSlotIndex);
		}
	}
}

void UFrontierLootWidget::RefreshSelectedItemInfo()
{
	const FFrontierItemInstance* SelectedItemInstance = nullptr;
	if (CachedLootSlots.IsValidIndex(SelectedLootSlotIndex))
	{
		const FFrontierInventorySlot& LootSlot = CachedLootSlots[SelectedLootSlotIndex];
		if (LootSlot.bOccupied && LootSlot.ItemInstance.IsValid())
		{
			SelectedItemInstance = &LootSlot.ItemInstance;
		}
	}
	else if (CachedRaidSlots.IsValidIndex(SelectedRaidSlotIndex))
	{
		const FFrontierInventorySlot& RaidSlot = CachedRaidSlots[SelectedRaidSlotIndex];
		if (RaidSlot.bOccupied && RaidSlot.ItemInstance.IsValid())
		{
			SelectedItemInstance = &RaidSlot.ItemInstance;
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

	if (SelectedItemStatsText)
	{
		if (!SelectedItemInstance || !SelectedItemInstance->IsValid())
		{
			SelectedItemStatsText->SetText(FText::GetEmpty());
		}
		else
		{
			FString StatsText = FString::Printf(TEXT("Qty: %d"), SelectedItemInstance->Quantity);
			if (SelectedItemInstance->GetCategory() == EFrontierItemCategory::Weapon)
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
			else if (SelectedItemInstance->GetCategory() == EFrontierItemCategory::Armor
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
				StatsText += FString::Printf(TEXT("\nHeal Amount: %.0f\nUse Time: %.1f"),
					SelectedItemInstance->GetHealthRestoreAmount(),
					SelectedItemInstance->GetUseTime());
			}

			SelectedItemStatsText->SetText(FText::FromString(StatsText));
		}
	}
}

void UFrontierLootWidget::HandleLootSlotClicked(UFrontierInventorySlotWidget* SlotWidget, const int32 SlotIndex)
{
	if (AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>())
	{
		FrontierPlayerController->ServerLootContainerItem(ObservedLootContainer, SlotIndex);
	}
}

void UFrontierLootWidget::HandleLootSlotDoubleClicked(UFrontierInventorySlotWidget* SlotWidget, const int32 SlotIndex)
{
	if (AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>())
	{
		FrontierPlayerController->ServerLootContainerItem(ObservedLootContainer, SlotIndex);
	}
}

void UFrontierLootWidget::HandleLootSlotDropped(UFrontierInventorySlotWidget* SlotWidget, const int32 SlotIndex, UFrontierInventoryDragDropOperation* DragOperation)
{
	if (!DragOperation)
	{
		return;
	}

	if (AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>())
	{
		if (DragOperation->SourceType == EFrontierDragSourceType::InventorySlot)
		{
			if (DragOperation->SourceInventoryCollection == EFrontierInventoryCollectionType::RaidInventory)
			{
				FrontierPlayerController->ServerStoreRaidItemInLootContainerSlot(ObservedLootContainer, DragOperation->SourceInventorySlotIndex, SlotIndex);
				return;
			}

			if (DragOperation->SourceInventoryCollection == EFrontierInventoryCollectionType::LootInventory)
			{
				FrontierPlayerController->ServerSwapLootContainerSlots(ObservedLootContainer, DragOperation->SourceInventorySlotIndex, SlotIndex);
			}
		}
	}
}

void UFrontierLootWidget::HandleRaidSlotClicked(UFrontierInventorySlotWidget* SlotWidget, const int32 SlotIndex)
{
	SelectedLootSlotIndex = INDEX_NONE;
	SelectedRaidSlotIndex = (SelectedRaidSlotIndex == SlotIndex) ? INDEX_NONE : SlotIndex;
	RefreshSelection();
	RefreshSelectedItemInfo();
}

void UFrontierLootWidget::HandleRaidSlotDoubleClicked(UFrontierInventorySlotWidget* SlotWidget, const int32 SlotIndex)
{
	if (AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>())
	{
		FrontierPlayerController->ServerStoreRaidItemInLootContainer(ObservedLootContainer, SlotIndex);
	}
}

void UFrontierLootWidget::HandleRaidSlotDropped(UFrontierInventorySlotWidget* SlotWidget, const int32 SlotIndex, UFrontierInventoryDragDropOperation* DragOperation)
{
	if (!DragOperation)
	{
		return;
	}

	if (AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>())
	{
		if (DragOperation->SourceType == EFrontierDragSourceType::InventorySlot)
		{
			if (DragOperation->SourceInventoryCollection == EFrontierInventoryCollectionType::LootInventory)
			{
				FrontierPlayerController->ServerLootContainerItemToRaidSlot(ObservedLootContainer, DragOperation->SourceInventorySlotIndex, SlotIndex);
				return;
			}

			if (DragOperation->SourceInventoryCollection == EFrontierInventoryCollectionType::RaidInventory)
			{
				FrontierPlayerController->ServerSwapRaidInventorySlots(DragOperation->SourceInventorySlotIndex, SlotIndex);
			}
		}
	}
}

void UFrontierLootWidget::UnbindLootSource()
{
	if (BoundLootInventoryComponent)
	{
		BoundLootInventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UFrontierLootWidget::HandleLootInventoryChanged);
		BoundLootInventoryComponent = nullptr;
	}

	if (BoundRaidInventoryComponent)
	{
		BoundRaidInventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UFrontierLootWidget::HandleRaidInventoryChanged);
		BoundRaidInventoryComponent = nullptr;
	}
}
