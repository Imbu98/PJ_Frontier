#include "UI/FrontierQuickSlotWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/FrontierQuickSlotComponent.h"
#include "Components/FrontierRaidInventoryComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Texture2D.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierPlayerState.h"
#include "UI/FrontierInventoryDragDropOperation.h"

void UFrontierQuickSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshSlot();
}

void UFrontierQuickSlotWidget::NativeDestruct()
{
	SetQuickSlotComponent(nullptr);
	Super::NativeDestruct();
}

void UFrontierQuickSlotWidget::SetQuickSlotComponent(UFrontierQuickSlotComponent* InComponent)
{
	if (QuickSlotComponent == InComponent)
	{
		RefreshSlot();
		return;
	}

	if (QuickSlotComponent)
	{
		QuickSlotComponent->OnQuickSlotsChanged.RemoveAll(this);
	}
	UnbindRaidInventory();

	QuickSlotComponent = InComponent;
	if (QuickSlotComponent)
	{
		QuickSlotComponent->OnQuickSlotsChanged.AddUObject(this, &UFrontierQuickSlotWidget::HandleQuickSlotsChanged);

		if (AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(QuickSlotComponent->GetOwner()))
		{
			BoundRaidInventoryComponent = PlayerState->GetRaidInventoryComponent();
			if (BoundRaidInventoryComponent)
			{
				BoundRaidInventoryComponent->OnInventoryChanged.AddDynamic(
					this,
					&UFrontierQuickSlotWidget::HandleRaidInventoryChanged);
			}
		}
	}
	RefreshSlot();
}

void UFrontierQuickSlotWidget::SetQuickSlotIndex(const int32 InSlotIndex)
{
	QuickSlotIndex = InSlotIndex;
	RefreshSlot();
}

void UFrontierQuickSlotWidget::SetWidgetMode(const EFrontierQuickSlotWidgetMode InMode)
{
	WidgetMode = InMode;
}

bool UFrontierQuickSlotWidget::IsOccupied() const
{
	return QuickSlotComponent && QuickSlotComponent->IsQuickSlotOccupied(QuickSlotIndex);
}

void UFrontierQuickSlotWidget::HandleQuickSlotsChanged()
{
	RefreshSlot();
}

void UFrontierQuickSlotWidget::HandleRaidInventoryChanged(const TArray<FFrontierInventorySlot>& Slots)
{
	(void)Slots;
	RefreshSlot();
}

void UFrontierQuickSlotWidget::UnbindRaidInventory()
{
	if (BoundRaidInventoryComponent)
	{
		BoundRaidInventoryComponent->OnInventoryChanged.RemoveDynamic(
			this,
			&UFrontierQuickSlotWidget::HandleRaidInventoryChanged);
		BoundRaidInventoryComponent = nullptr;
	}
}

void UFrontierQuickSlotWidget::RefreshSlot()
{
	FFrontierItemInstance Item;
	const bool bHasItem = QuickSlotComponent
		&& QuickSlotIndex >= 0
		&& QuickSlotComponent->ResolveItemForQuickSlot(QuickSlotIndex, Item);

	if (OccupiedRoot)
	{
		OccupiedRoot->SetVisibility(bHasItem ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (EmptyRoot)
	{
		EmptyRoot->SetVisibility(bHasItem ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (ItemIconImage)
	{
		UTexture2D* Icon = bHasItem ? Item.GetIcon().LoadSynchronous() : nullptr;
		ItemIconImage->SetBrushFromTexture(Icon);
		ItemIconImage->SetVisibility(Icon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (QuantityText)
	{
		QuantityText->SetText(bHasItem ? FText::AsNumber(Item.Quantity) : FText::GetEmpty());
		QuantityText->SetVisibility(bHasItem && Item.Quantity > 1 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (KeyText)
	{
		KeyText->SetText(QuickSlotIndex >= 0 && QuickSlotIndex < 3
			? FText::AsNumber(QuickSlotIndex + 1)
			: FText::GetEmpty());
	}
	if (SelectionBorder)
	{
		SelectionBorder->SetVisibility(WidgetMode == EFrontierQuickSlotWidgetMode::Edit
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

FReply UFrontierQuickSlotWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (WidgetMode == EFrontierQuickSlotWidgetMode::Action)
	{
		if (IsOccupied())
		{
			if (AFrontierPlayerController* Controller = Cast<AFrontierPlayerController>(GetOwningPlayer()))
			{
				Controller->RequestActivateQuickSlot(QuickSlotIndex);
			}
		}
		return FReply::Handled();
	}

	return IsOccupied()
	? UWidgetBlueprintLibrary::DetectDragIfPressed(
		InMouseEvent,
		this,
		EKeys::LeftMouseButton
	).NativeReply
	: FReply::Handled();
}

FReply UFrontierQuickSlotWidget::NativeOnMouseButtonDoubleClick(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (WidgetMode == EFrontierQuickSlotWidgetMode::Edit
		&& InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
		&& IsOccupied())
	{
		if (AFrontierPlayerController* Controller = Cast<AFrontierPlayerController>(GetOwningPlayer()))
		{
			Controller->ServerClearQuickSlot(QuickSlotIndex);
		}
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

void UFrontierQuickSlotWidget::NativeOnDragDetected(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	if (WidgetMode != EFrontierQuickSlotWidgetMode::Edit || !IsOccupied())
	{
		return;
	}

	UFrontierInventoryDragDropOperation* DragOperation = NewObject<UFrontierInventoryDragDropOperation>(this);
	if (!DragOperation)
	{
		return;
	}

	QuickSlotComponent->ResolveItemForQuickSlot(QuickSlotIndex, DragOperation->DraggedItemInstance);
	DragOperation->SourceType = EFrontierDragSourceType::QuickSlot;
	DragOperation->SourceQuickSlotIndex = QuickSlotIndex;
	DragOperation->OwningPlayerController = GetOwningPlayer();
	DragOperation->DefaultDragVisual = this;
	DragOperation->Pivot = EDragPivot::MouseDown;
	OutOperation = DragOperation;
}

bool UFrontierQuickSlotWidget::NativeOnDrop(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	if (WidgetMode != EFrontierQuickSlotWidgetMode::Edit)
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	if (UFrontierInventoryDragDropOperation* DragOperation = Cast<UFrontierInventoryDragDropOperation>(InOperation))
	{
		if (AFrontierPlayerController* Controller = Cast<AFrontierPlayerController>(GetOwningPlayer()))
		{
			if (DragOperation->SourceType == EFrontierDragSourceType::InventorySlot
				&& DragOperation->SourceInventoryCollection == EFrontierInventoryCollectionType::RaidInventory)
			{
				Controller->ServerRegisterQuickSlotFromInventory(QuickSlotIndex, DragOperation->SourceInventorySlotIndex);
				return true;
			}

			if (DragOperation->SourceType == EFrontierDragSourceType::QuickSlot
				&& DragOperation->SourceQuickSlotIndex != QuickSlotIndex)
			{
				Controller->ServerMoveQuickSlot(DragOperation->SourceQuickSlotIndex, QuickSlotIndex);
				return true;
			}
		}
	}

	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}
