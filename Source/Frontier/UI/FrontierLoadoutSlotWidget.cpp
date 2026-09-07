#include "UI/FrontierLoadoutSlotWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "Input/Reply.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "UI/FrontierInventoryDragDropOperation.h"
#include "UI/FrontierInventoryWidget.h"
#include "UI/FrontierItemRarityBorderWidget.h"
#include "FrontierPlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Frontier.h"
#include "TimerManager.h"

void UFrontierLoadoutSlotWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	NativeOnSlotDataChanged();
}

void UFrontierLoadoutSlotWidget::NativeDestruct()
{
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

FReply UFrontierLoadoutSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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

	return FReply::Unhandled();
}

FReply UFrontierLoadoutSlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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

		OnSlotDoubleClicked.Broadcast(this, SlotType);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

void UFrontierLoadoutSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
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

	DragOperation->SourceType = EFrontierDragSourceType::LoadoutSlot;
	DragOperation->SourceLoadoutSlotType = SlotType;
	DragOperation->DraggedItemInstance = SlotData.ItemInstance;
	DragOperation->OwningPlayerController = GetOwningPlayer();
	DragOperation->DefaultDragVisual = this;
	DragOperation->Pivot = EDragPivot::MouseDown;
	OutOperation = DragOperation;
}

bool UFrontierLoadoutSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (UFrontierInventoryDragDropOperation* DragOperation = Cast<UFrontierInventoryDragDropOperation>(InOperation))
	{
		if (DragOperation->SourceType == EFrontierDragSourceType::QuickSlot)
		{
			return false;
		}

		OnSlotDropped.Broadcast(this, SlotType, DragOperation);
		return true;
	}

	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UFrontierLoadoutSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (!IsSlotOccupied())
	{
		FRONTIER_LOG(Log, TEXT("LoadoutSlot hover ignored because slot is empty. SlotType=%d"), static_cast<int32>(SlotType));
		return;
	}

	if (bSuppressTooltipUntilMouseLeave)
	{
		return;
	}

	FRONTIER_LOG(Log, TEXT("LoadoutSlot hover begin. SlotType=%d"), static_cast<int32>(SlotType));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TooltipHoverTimerHandle);
		World->GetTimerManager().SetTimer(TooltipHoverTimerHandle, this, &UFrontierLoadoutSlotWidget::RequestShowTooltip, TooltipHoverDelay, false);
	}
}

void UFrontierLoadoutSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
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

void UFrontierLoadoutSlotWidget::SetSlotData(const EFrontierEquipmentSlot InSlotType, const FFrontierLoadoutSlot& InSlotData)
{
	if (SlotType != InSlotType)
	{
		CachedEmptySlotIconTexture = nullptr;
		CachedEmptySlotIconType = EFrontierEquipmentSlot::None;
	}

	SlotType = InSlotType;
	SlotData = InSlotData;
	NativeOnSlotDataChanged();
}

void UFrontierLoadoutSlotWidget::SetSelected(const bool bInSelected)
{
	if (bSelected == bInSelected)
	{
		return;
	}

	bSelected = bInSelected;
	NativeOnSlotDataChanged();
}

void UFrontierLoadoutSlotWidget::BroadcastSlotClicked()
{
	OnSlotClicked.Broadcast(this, SlotType);
}

EFrontierEquipmentSlot UFrontierLoadoutSlotWidget::GetSlotType() const
{
	return SlotType;
}

FFrontierLoadoutSlot UFrontierLoadoutSlotWidget::GetSlotData() const
{
	return SlotData;
}

bool UFrontierLoadoutSlotWidget::IsSlotOccupied() const
{
	return SlotData.bOccupied && SlotData.ItemInstance.IsValid();
}

FFrontierItemTemplateData UFrontierLoadoutSlotWidget::GetItemTemplateData() const
{
	return SlotData.ItemInstance.IsValid() ? SlotData.ItemInstance.ItemTemplateData.Common : FFrontierItemTemplateData();
}

bool UFrontierLoadoutSlotWidget::IsSelected() const
{
	return bSelected;
}

void UFrontierLoadoutSlotWidget::SetOwningInventoryWidget(UFrontierInventoryWidget* InOwningInventoryWidget)
{
	OwningInventoryWidget = InOwningInventoryWidget;
}

void UFrontierLoadoutSlotWidget::NativeOnSlotDataChanged()
{
	RefreshVisualState();
}

void UFrontierLoadoutSlotWidget::RefreshVisualState()
{
	const bool bOccupied = IsSlotOccupied();
	UTexture2D* EmptySlotIconTexture = CachedEmptySlotIconType == SlotType
		? CachedEmptySlotIconTexture.Get()
		: nullptr;

	if (!EmptySlotIconTexture)
	{
		switch (SlotType)
		{
		case EFrontierEquipmentSlot::MainWeapon:
			EmptySlotIconTexture = MainWeaponEmptyIcon.LoadSynchronous();
			break;
		case EFrontierEquipmentSlot::SubWeapon:
			EmptySlotIconTexture = SubWeaponEmptyIcon.LoadSynchronous();
			break;
		case EFrontierEquipmentSlot::Helmet:
			EmptySlotIconTexture = HelmetEmptyIcon.LoadSynchronous();
			break;
		case EFrontierEquipmentSlot::Chest:
			EmptySlotIconTexture = ChestEmptyIcon.LoadSynchronous();
			break;
		case EFrontierEquipmentSlot::Gloves:
			EmptySlotIconTexture = GlovesEmptyIcon.LoadSynchronous();
			break;
		case EFrontierEquipmentSlot::Boots:
			EmptySlotIconTexture = BootsEmptyIcon.LoadSynchronous();
			break;
		case EFrontierEquipmentSlot::Necklace:
			EmptySlotIconTexture = NecklaceEmptyIcon.LoadSynchronous();
			break;
		case EFrontierEquipmentSlot::Ring:
			EmptySlotIconTexture = RingEmptyIcon.LoadSynchronous();
			break;
		default:
			break;
		}

		CachedEmptySlotIconTexture = EmptySlotIconTexture;
		CachedEmptySlotIconType = SlotType;
	}

	if (ItemIconImage)
	{
		const TSoftObjectPtr<UTexture2D> Icon = bOccupied ? SlotData.ItemInstance.GetIcon() : TSoftObjectPtr<UTexture2D>();
		UTexture2D* IconTexture = !Icon.IsNull()
			? Icon.LoadSynchronous()
			: nullptr;
		ItemIconImage->SetBrushFromTexture(IconTexture, true);
		ItemIconImage->SetVisibility(IconTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	if (EmptySlotTypeImage)
	{
		EmptySlotTypeImage->SetBrushFromTexture(EmptySlotIconTexture, true);
		EmptySlotTypeImage->SetVisibility((!bOccupied && EmptySlotIconTexture) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	if (SlotNameText)
	{
		SlotNameText->SetVisibility(ESlateVisibility::Collapsed);
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

FText UFrontierLoadoutSlotWidget::GetSlotTypeDisplayText() const
{
	switch (SlotType)
	{
	case EFrontierEquipmentSlot::MainWeapon:
		return FText::FromString(TEXT("Main Weapon"));
	case EFrontierEquipmentSlot::SubWeapon:
		return FText::FromString(TEXT("Sub Weapon"));
	case EFrontierEquipmentSlot::Helmet:
		return FText::FromString(TEXT("Helmet"));
	case EFrontierEquipmentSlot::Chest:
		return FText::FromString(TEXT("Chest"));
	case EFrontierEquipmentSlot::Gloves:
		return FText::FromString(TEXT("Gloves"));
	case EFrontierEquipmentSlot::Boots:
		return FText::FromString(TEXT("Boots"));
	case EFrontierEquipmentSlot::Necklace:
		return FText::FromString(TEXT("Necklace"));
	case EFrontierEquipmentSlot::Ring:
		return FText::FromString(TEXT("Ring"));
	default:
		return FText::FromString(TEXT("Empty"));
	}
}

void UFrontierLoadoutSlotWidget::RequestShowTooltip()
{
	if (!IsSlotOccupied())
	{
		FRONTIER_LOG(Log, TEXT("LoadoutSlot tooltip request ignored because slot is no longer occupied. SlotType=%d"), static_cast<int32>(SlotType));
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

		FRONTIER_LOG(Log, TEXT("LoadoutSlot tooltip request forwarded. SlotType=%d"), static_cast<int32>(SlotType));
		InventoryWidget->ShowItemTooltipForLoadoutSlot(this, SlotData.ItemInstance);
	}
	else
	{
		FRONTIER_LOG(Warning, TEXT("LoadoutSlot tooltip request failed because owning inventory widget is null. SlotType=%d"), static_cast<int32>(SlotType));
	}
}

UFrontierInventoryWidget* UFrontierLoadoutSlotWidget::ResolveOwningInventoryWidget() const
{
	return OwningInventoryWidget;
}
