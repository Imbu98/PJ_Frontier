#include "UI/FrontierInventorySlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Input/Reply.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "UI/FrontierInventoryDragDropOperation.h"
#include "UI/FrontierInventoryWidget.h"
#include "UI/FrontierItemRarityBorderWidget.h"
#include "FrontierPlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Frontier.h"
#include "TimerManager.h"

void UFrontierInventorySlotWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	//NativeOnSlotDataChanged();
}

void UFrontierInventorySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Slot data can be assigned before this widget is attached to the live Slate
	// tree. Reapply the state here so the Blueprint's default visibility cannot
	// leave an initially empty slot hidden. Occupied slots already get another
	// refresh when their presentation assets finish loading; empty slots do not.
	NativeOnSlotDataChanged();
}

void UFrontierInventorySlotWidget::NativeDestruct()
{
	CancelPresentationLoads();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TooltipHoverTimerHandle);
	}

	if (UFrontierInventoryWidget* InventoryWidget = ResolveOwningInventoryWidget())
	{
		InventoryWidget->HideItemTooltip(this);
	}

	Super::NativeDestruct();
}

FReply UFrontierInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FReply SuperReply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	if (SuperReply.IsEventHandled())
	{
		return SuperReply;
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bSuppressTooltipUntilMouseLeave = true;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(TooltipHoverTimerHandle);
		}
		if (UFrontierInventoryWidget* InventoryWidget = ResolveOwningInventoryWidget())
		{
			InventoryWidget->HideItemTooltip(this);
		}

		BroadcastSlotClicked();
		if (IsSlotOccupied())
		{
			return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
		}

		return FReply::Handled();
	}

	const AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>();
	const bool bAltPressed = (InMouseEvent.IsLeftAltDown() || InMouseEvent.IsRightAltDown())
		|| (FrontierPlayerController && (FrontierPlayerController->IsInputKeyDown(EKeys::LeftAlt) || FrontierPlayerController->IsInputKeyDown(EKeys::RightAlt)));

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton
		&& IsSlotOccupied())
	{
		bSuppressTooltipUntilMouseLeave = true;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(TooltipHoverTimerHandle);
		}
		if (UFrontierInventoryWidget* InventoryWidget = ResolveOwningInventoryWidget())
		{
			InventoryWidget->HideItemTooltip(this);
		}

		if (bAltPressed)
		{
			OnSlotAltRightClicked.Broadcast(this, SlotIndex);
		}
		else
		{
			OnSlotRightClicked.Broadcast(this, SlotIndex);
		}
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply UFrontierInventorySlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bSuppressTooltipUntilMouseLeave = true;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(TooltipHoverTimerHandle);
		}
		if (UFrontierInventoryWidget* InventoryWidget = ResolveOwningInventoryWidget())
		{
			InventoryWidget->HideItemTooltip(this);
		}

		OnSlotDoubleClicked.Broadcast(this, SlotIndex);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

void UFrontierInventorySlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (!IsSlotOccupied())
	{
		return;
	}

	UFrontierInventoryDragDropOperation* DragOperation = NewObject<UFrontierInventoryDragDropOperation>(this);
	if (!DragOperation)
	{
		return;
	}

	DragOperation->SourceType = EFrontierDragSourceType::InventorySlot;
	DragOperation->SourceInventoryCollection = InventoryCollectionType;
	DragOperation->SourceInventorySlotIndex = SlotIndex;
	DragOperation->DraggedItemInstance = SlotData.ItemInstance;
	DragOperation->OwningPlayerController = GetOwningPlayer();
	DragOperation->DefaultDragVisual = this;
	DragOperation->Pivot = EDragPivot::MouseDown;
	OutOperation = DragOperation;
}

bool UFrontierInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (UFrontierInventoryDragDropOperation* DragOperation = Cast<UFrontierInventoryDragDropOperation>(InOperation))
	{
		if (DragOperation->SourceType == EFrontierDragSourceType::QuickSlot)
		{
			return false;
		}

		OnSlotDropped.Broadcast(this, SlotIndex, DragOperation);
		return true;
	}

	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UFrontierInventorySlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (!IsSlotOccupied())
	{
		FRONTIER_LOG(Log, TEXT("InventorySlot hover ignored because slot is empty. SlotIndex=%d"), SlotIndex);
		return;
	}

	if (bSuppressTooltipUntilMouseLeave)
	{
		return;
	}

	FRONTIER_LOG(Log, TEXT("InventorySlot hover begin. SlotIndex=%d"), SlotIndex);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TooltipHoverTimerHandle);
		World->GetTimerManager().SetTimer(TooltipHoverTimerHandle, this, &UFrontierInventorySlotWidget::RequestShowTooltip, TooltipHoverDelay, false);
	}
}

void UFrontierInventorySlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TooltipHoverTimerHandle);
	}

	if (UFrontierInventoryWidget* InventoryWidget = ResolveOwningInventoryWidget())
	{
		InventoryWidget->HideItemTooltip(this);
	}

	bSuppressTooltipUntilMouseLeave = false;

	Super::NativeOnMouseLeave(InMouseEvent);
}

void UFrontierInventorySlotWidget::SetSlotData(const int32 InSlotIndex, const FFrontierInventorySlot& InSlotData)
{
	const FName PreviousTemplateId = SlotData.ItemInstance.GetTemplateId();
	const FName NewTemplateId = InSlotData.ItemInstance.GetTemplateId();
	if (PreviousTemplateId != NewTemplateId)
	{
		CancelPresentationLoads();
	}

	SlotIndex = InSlotIndex;
	SlotData = InSlotData;
	NativeOnSlotDataChanged();
}

void UFrontierInventorySlotWidget::SetSelected(const bool bInSelected)
{
	if (bSelected == bInSelected)
	{
		return;
	}

	bSelected = bInSelected;
	NativeOnSlotDataChanged();
}

void UFrontierInventorySlotWidget::BroadcastSlotClicked()
{
	OnSlotClicked.Broadcast(this, SlotIndex);
}

void UFrontierInventorySlotWidget::SetInventoryCollectionType(const EFrontierInventoryCollectionType InCollectionType)
{
	InventoryCollectionType = InCollectionType;
}

int32 UFrontierInventorySlotWidget::GetSlotIndex() const
{
	return SlotIndex;
}

FFrontierInventorySlot UFrontierInventorySlotWidget::GetSlotData() const
{
	return SlotData;
}

bool UFrontierInventorySlotWidget::IsSlotOccupied() const
{
	return SlotData.bOccupied && SlotData.ItemInstance.IsValid();
}

FFrontierItemTemplateData UFrontierInventorySlotWidget::GetItemTemplateData() const
{
	return SlotData.ItemInstance.IsValid() ? SlotData.ItemInstance.ItemTemplateData.Common : FFrontierItemTemplateData();
}

bool UFrontierInventorySlotWidget::IsSelected() const
{
	return bSelected;
}

void UFrontierInventorySlotWidget::SetOwningInventoryWidget(UFrontierInventoryWidget* InOwningInventoryWidget)
{
	OwningInventoryWidget = InOwningInventoryWidget;
}

void UFrontierInventorySlotWidget::NativeOnSlotDataChanged()
{
	RefreshVisualState();
}

void UFrontierInventorySlotWidget::RequestShowTooltip()
{
	if (!IsSlotOccupied())
	{
		FRONTIER_LOG(Log, TEXT("InventorySlot tooltip request ignored because slot is no longer occupied. SlotIndex=%d"), SlotIndex);
		return;
	}

	if (UFrontierInventoryWidget* InventoryWidget = ResolveOwningInventoryWidget())
	{
		const ESlateVisibility WidgetVisibility = InventoryWidget->GetVisibility();
		if (!IsHovered()
			|| WidgetVisibility == ESlateVisibility::Collapsed
			|| WidgetVisibility == ESlateVisibility::Hidden)
		{
			InventoryWidget->HideItemTooltip(this);
			return;
		}

		FRONTIER_LOG(Log, TEXT("InventorySlot tooltip request forwarded. SlotIndex=%d"), SlotIndex);
		InventoryWidget->ShowItemTooltipForInventorySlot(this, SlotData.ItemInstance);
	}
	else
	{
		FRONTIER_LOG(Warning, TEXT("InventorySlot tooltip request failed because owning inventory widget is null. SlotIndex=%d"), SlotIndex);
	}
}

UFrontierInventoryWidget* UFrontierInventorySlotWidget::ResolveOwningInventoryWidget() const
{
	return OwningInventoryWidget;
}

void UFrontierInventorySlotWidget::RefreshVisualState()
{
	const bool bOccupied = IsSlotOccupied();
	RequestPresentationAssets();

	if (ItemIconImage)
	{
		const TSoftObjectPtr<UTexture2D> Icon = bOccupied ? SlotData.ItemInstance.GetIcon() : TSoftObjectPtr<UTexture2D>();
		UTexture2D* IconTexture = !Icon.IsNull()
			? Icon.Get()
			: nullptr;
		ItemIconImage->SetBrushFromTexture(IconTexture, true);
		ItemIconImage->SetVisibility(IconTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	if (QuantityText)
	{
		const int32 Quantity = SlotData.ItemInstance.Quantity;
		const bool bShowQuantity = bOccupied && Quantity > 1;
		QuantityText->SetText(bShowQuantity ? FText::AsNumber(Quantity) : FText::GetEmpty());
		QuantityText->SetVisibility(bShowQuantity ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	if (SelectionBorder)
	{
		SelectionBorder->SetVisibility(bSelected ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}

	if (OccupiedRoot)
	{
		OccupiedRoot->SetVisibility(bOccupied ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (EmptyRoot)
	{
		EmptyRoot->SetVisibility(bOccupied ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (RarityBorderWidget)
	{
		if (bOccupied)
		{
			RarityBorderWidget->SetItemRarity(SlotData.ItemInstance.GetDisplayRarity());
		}
		else
		{
			RarityBorderWidget->ClearRarity();
		}
	}
}

void UFrontierInventorySlotWidget::RequestPresentationAssets()
{
	if (!IsSlotOccupied())
	{
		CancelPresentationLoads();
		return;
	}

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		return;
	}

	const FName TemplateId = SlotData.ItemInstance.GetTemplateId();
	const TSoftObjectPtr<UTexture2D> Icon = SlotData.ItemInstance.GetIcon();
	const FSoftObjectPath IconPath = Icon.ToSoftObjectPath();
	if (Icon.IsNull() || Icon.Get()
		|| (IconLoadHandle && !IconLoadHandle->HasLoadCompleted()))
	{
		return;
	}

	IconLoadHandle = AssetManager->GetStreamableManager().RequestAsyncLoad(
		IconPath,
		FStreamableDelegate::CreateWeakLambda(this, [this, TemplateId, IconPath]()
		{
			IconLoadHandle.Reset();
			const TSoftObjectPtr<UTexture2D> CurrentIcon = SlotData.ItemInstance.GetIcon();
			if (SlotData.ItemInstance.GetTemplateId() == TemplateId
				&& CurrentIcon.ToSoftObjectPath() == IconPath)
			{
				RefreshVisualState();
			}
		}));
}

void UFrontierInventorySlotWidget::CancelPresentationLoads()
{
	if (ItemDataLoadHandle)
	{
		ItemDataLoadHandle->CancelHandle();
		ItemDataLoadHandle.Reset();
	}

	if (IconLoadHandle)
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}
}
