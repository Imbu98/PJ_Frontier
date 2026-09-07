#include "UI/FrontierDropLootEntryWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "FrontierPlayerController.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "InputCoreTypes.h"
#include "Loot/FrontierDroppedItemActor.h"
#include "UI/FrontierInventoryDragDropOperation.h"
#include "UI/FrontierInventoryWidget.h"
#include "TimerManager.h"

namespace
{
	FLinearColor ResolveDroppedLootRarityColor(const EFrontierItemRarity Rarity)
	{
		switch (Rarity)
		{
		case EFrontierItemRarity::Rare:
			return FLinearColor(0.08f, 0.24f, 0.7f, 0.94f);
		case EFrontierItemRarity::Epic:
			return FLinearColor(0.38f, 0.08f, 0.62f, 0.94f);
		case EFrontierItemRarity::Legendary:
			return FLinearColor(0.75f, 0.34f, 0.04f, 0.94f);
		case EFrontierItemRarity::Common:
		default:
			return FLinearColor(0.08f, 0.08f, 0.08f, 0.94f);
		}
	}
}

void UFrontierDropLootEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshFromDroppedItem();
}

void UFrontierDropLootEntryWidget::NativeDestruct()
{
	CancelIconLoad();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TooltipHoverTimerHandle);
	}
	if (OwningInventoryWidget)
	{
		OwningInventoryWidget->HideItemTooltip(this);
	}
	Super::NativeDestruct();
}

FReply UFrontierDropLootEntryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FReply SuperReply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	if (SuperReply.IsEventHandled())
	{
		return SuperReply;
	}

	AFrontierDroppedItemActor* DroppedItem = ObservedDroppedItem.Get();
	if (!IsValid(DroppedItem)
		|| !DroppedItem->GetItemInstance().IsValid())
	{
		return FReply::Unhandled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		RequestPickup();
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TooltipHoverTimerHandle);
	}
	if (OwningInventoryWidget)
	{
		OwningInventoryWidget->HideItemTooltip(this);
	}
	return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
}

FReply UFrontierDropLootEntryWidget::NativeOnMouseButtonDoubleClick(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
		&& IsValid(ObservedDroppedItem.Get()))
	{
		RequestPickup();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

void UFrontierDropLootEntryWidget::NativeOnDragDetected(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	AFrontierDroppedItemActor* DroppedItem = ObservedDroppedItem.Get();
	if (!IsValid(DroppedItem) || !DroppedItem->GetItemInstance().IsValid())
	{
		return;
	}

	UFrontierInventoryDragDropOperation* DragOperation = NewObject<UFrontierInventoryDragDropOperation>(this);
	if (!DragOperation)
	{
		return;
	}

	DragOperation->SourceType = EFrontierDragSourceType::DroppedItem;
	DragOperation->SourceDroppedItem = DroppedItem;
	DragOperation->DraggedItemInstance = DroppedItem->GetItemInstance();
	DragOperation->OwningPlayerController = GetOwningPlayer();
	DragOperation->DefaultDragVisual = this;
	DragOperation->Pivot = EDragPivot::MouseDown;
	OutOperation = DragOperation;
}

void UFrontierDropLootEntryWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (!OwningInventoryWidget || !IsValid(ObservedDroppedItem.Get()))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TooltipHoverTimerHandle);
		World->GetTimerManager().SetTimer(
			TooltipHoverTimerHandle,
			this,
			&UFrontierDropLootEntryWidget::RequestShowTooltip,
			TooltipHoverDelay,
			false);
	}
}

void UFrontierDropLootEntryWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TooltipHoverTimerHandle);
	}
	if (OwningInventoryWidget)
	{
		OwningInventoryWidget->HideItemTooltip(this);
	}

	Super::NativeOnMouseLeave(InMouseEvent);
}

void UFrontierDropLootEntryWidget::SetObservedDroppedItem(AFrontierDroppedItemActor* InDroppedItem)
{
	if (ObservedDroppedItem.Get() != InDroppedItem)
	{
		CancelIconLoad();
		ObservedDroppedItem = InDroppedItem;
	}

	RefreshFromDroppedItem();
}

AFrontierDroppedItemActor* UFrontierDropLootEntryWidget::GetObservedDroppedItem() const
{
	return ObservedDroppedItem.Get();
}

void UFrontierDropLootEntryWidget::SetOwningInventoryWidget(UFrontierInventoryWidget* InOwningInventoryWidget)
{
	if (OwningInventoryWidget && OwningInventoryWidget != InOwningInventoryWidget)
	{
		OwningInventoryWidget->HideItemTooltip(this);
	}
	OwningInventoryWidget = InOwningInventoryWidget;
}

void UFrontierDropLootEntryWidget::RefreshFromDroppedItem()
{
	AFrontierDroppedItemActor* DroppedItem = ObservedDroppedItem.Get();
	const FFrontierItemInstance* ItemInstance = DroppedItem ? &DroppedItem->GetItemInstance() : nullptr;
	const bool bHasValidItem = ItemInstance && ItemInstance->IsValid();

	SetVisibility(bHasValidItem ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (EntryBorder)
	{
		const FLinearColor BaseColor = bHasValidItem
			? ResolveDroppedLootRarityColor(ItemInstance->GetDisplayRarity())
			: FLinearColor(0.05f, 0.05f, 0.05f, 0.9f);
		EntryBorder->SetBrushColor(BaseColor);
	}

	if (ItemNameText)
	{
		ItemNameText->SetText(bHasValidItem ? ItemInstance->GetDisplayNameText() : FText::GetEmpty());
	}

	if (ItemSummaryText)
	{
		const FString Summary = bHasValidItem
			? FString::Printf(TEXT("%s  x%d"), *ItemInstance->GetDescriptionText().ToString(), FMath::Max(1, ItemInstance->Quantity))
			: FString();
		ItemSummaryText->SetText(FText::FromString(Summary));
	}

	if (ItemIconImage)
	{
		const TSoftObjectPtr<UTexture2D> Icon = bHasValidItem ? ItemInstance->GetIcon() : TSoftObjectPtr<UTexture2D>();
		UTexture2D* IconTexture = Icon.Get();
		ItemIconImage->SetBrushFromTexture(IconTexture, true);
		ItemIconImage->SetVisibility(IconTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	RequestIconAsset();
}

void UFrontierDropLootEntryWidget::RequestIconAsset()
{
	AFrontierDroppedItemActor* DroppedItem = ObservedDroppedItem.Get();
	const FFrontierItemInstance* ItemInstance = DroppedItem ? &DroppedItem->GetItemInstance() : nullptr;
	if (!ItemInstance || !ItemInstance->IsValid())
	{
		CancelIconLoad();
		return;
	}

	const TSoftObjectPtr<UTexture2D> Icon = ItemInstance->GetIcon();
	const FSoftObjectPath IconPath = Icon.ToSoftObjectPath();
	if (Icon.IsNull() || Icon.Get() || (IconLoadHandle && !IconLoadHandle->HasLoadCompleted()))
	{
		return;
	}

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		return;
	}

	const TWeakObjectPtr<AFrontierDroppedItemActor> RequestedActor = ObservedDroppedItem;
	IconLoadHandle = AssetManager->GetStreamableManager().RequestAsyncLoad(
		IconPath,
		FStreamableDelegate::CreateWeakLambda(this, [this, RequestedActor, IconPath]()
		{
			IconLoadHandle.Reset();
			AFrontierDroppedItemActor* CurrentActor = ObservedDroppedItem.Get();
			if (CurrentActor != RequestedActor.Get() || !CurrentActor)
			{
				return;
			}

			const TSoftObjectPtr<UTexture2D> CurrentIcon = CurrentActor->GetItemInstance().GetIcon();
			if (CurrentIcon.ToSoftObjectPath() == IconPath)
			{
				RefreshFromDroppedItem();
			}
		}));
}

void UFrontierDropLootEntryWidget::CancelIconLoad()
{
	if (IconLoadHandle)
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}
}

void UFrontierDropLootEntryWidget::RequestShowTooltip()
{
	AFrontierDroppedItemActor* DroppedItem = ObservedDroppedItem.Get();
	if (OwningInventoryWidget && IsValid(DroppedItem) && DroppedItem->GetItemInstance().IsValid())
	{
		OwningInventoryWidget->ShowItemTooltipForWidget(this, DroppedItem->GetItemInstance());
	}
}

void UFrontierDropLootEntryWidget::RequestPickup()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TooltipHoverTimerHandle);
	}
	if (OwningInventoryWidget)
	{
		OwningInventoryWidget->HideItemTooltip(this);
	}

	AFrontierDroppedItemActor* DroppedItem = ObservedDroppedItem.Get();
	if (IsValid(DroppedItem))
	{
		if (AFrontierPlayerController* FrontierController = GetOwningPlayer<AFrontierPlayerController>())
		{
			FrontierController->ServerPickupDroppedItem(DroppedItem);
		}
	}
}
