#include "UI/FrontierQuickSlotBarWidget.h"

#include "Components/FrontierQuickSlotComponent.h"
#include "FrontierPlayerController.h"
#include "UI/FrontierInventoryDragDropOperation.h"
#include "UI/FrontierQuickSlotWidget.h"

void UFrontierQuickSlotBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ConfigureSlotWidgets();
}

void UFrontierQuickSlotBarWidget::NativeDestruct()
{
	SetQuickSlotComponent(nullptr);
	Super::NativeDestruct();
}

void UFrontierQuickSlotBarWidget::SetQuickSlotComponent(UFrontierQuickSlotComponent* InComponent)
{
	QuickSlotComponent = InComponent;
	ConfigureSlotWidgets();
}

void UFrontierQuickSlotBarWidget::SetEditMode(const bool bInEditMode)
{
	bEditMode = bInEditMode;
	ConfigureSlotWidgets();
}

void UFrontierQuickSlotBarWidget::ConfigureSlotWidgets()
{
	UFrontierQuickSlotWidget* SlotWidgets[] = { QuickSlotWidget_1, QuickSlotWidget_2, QuickSlotWidget_3 };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SlotWidgets); ++Index)
	{
		if (SlotWidgets[Index])
		{
			SlotWidgets[Index]->SetQuickSlotIndex(Index);
			SlotWidgets[Index]->SetWidgetMode(bEditMode
				? EFrontierQuickSlotWidgetMode::Edit
				: EFrontierQuickSlotWidgetMode::Action);
			SlotWidgets[Index]->SetQuickSlotComponent(QuickSlotComponent);
		}
	}
}

void UFrontierQuickSlotBarWidget::RefreshSlots()
{
	UFrontierQuickSlotWidget* SlotWidgets[] = { QuickSlotWidget_1, QuickSlotWidget_2, QuickSlotWidget_3 };
	for (UFrontierQuickSlotWidget* SlotWidget : SlotWidgets)
	{
		if (SlotWidget)
		{
			SlotWidget->RefreshSlot();
		}
	}
}

bool UFrontierQuickSlotBarWidget::NativeOnDrop(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	if (bEditMode)
	{
		if (UFrontierInventoryDragDropOperation* DragOperation = Cast<UFrontierInventoryDragDropOperation>(InOperation))
		{
			if (DragOperation->SourceType == EFrontierDragSourceType::QuickSlot
				&& DragOperation->SourceQuickSlotIndex != INDEX_NONE)
			{
				if (AFrontierPlayerController* Controller = Cast<AFrontierPlayerController>(GetOwningPlayer()))
				{
					Controller->ServerClearQuickSlot(DragOperation->SourceQuickSlotIndex);
					return true;
				}
			}
		}
	}

	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}
