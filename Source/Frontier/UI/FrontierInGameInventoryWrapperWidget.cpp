#include "UI/FrontierInGameInventoryWrapperWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "FrontierPlayerController.h"
#include "FrontierSteamSubsystem.h"
#include "Game/FrontierPlayerState.h"
#include "GameFramework/Character.h"
#include "InputCoreTypes.h"
#include "Components/Button.h"
#include "UI/FrontierCharacterPreviewWidget.h"
#include "UI/FrontierCharacterStatPanelWidget.h"
#include "UI/FrontierEquippedSkillPanelWidget.h"
#include "UI/FrontierInventoryDragDropOperation.h"
#include "UI/FrontierInventoryWidget.h"

void UFrontierInGameInventoryWrapperWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UFrontierInGameInventoryWrapperWidget::NativeDestruct()
{
	ClearPanelWidgets();
	ClearCharacterDetailsPanel();

	Super::NativeDestruct();
}

FReply UFrontierInGameInventoryWrapperWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
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

bool UFrontierInGameInventoryWrapperWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
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

bool UFrontierInGameInventoryWrapperWidget::TryHandlePanelDrop(const FVector2D& ScreenSpacePosition, UFrontierInventoryDragDropOperation* DragOperation)
{
	AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>();
	if (!DragOperation || !FrontierPlayerController)
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
			FrontierPlayerController->ServerStoreRaidItemInStorage(DragOperation->SourceInventorySlotIndex);
			return true;
		}
		break;

	case EInventoryWidgetMode::PlayerInventory:
		if (DragOperation->SourceType == EFrontierDragSourceType::InventorySlot
			&& DragOperation->SourceInventoryCollection == EFrontierInventoryCollectionType::Storage)
		{
			FrontierPlayerController->ServerWithdrawLobbyItemToRaidInventory(DragOperation->SourceInventorySlotIndex);
			return true;
		}

		if (DragOperation->SourceType == EFrontierDragSourceType::LoadoutSlot)
		{
			FrontierPlayerController->ServerUnequipLoadoutItemToRaidInventory(DragOperation->SourceLoadoutSlotType);
			return true;
		}
		break;

	default:
		break;
	}

	return false;
}

void UFrontierInGameInventoryWrapperWidget::SetObservedPlayerState(AFrontierPlayerState* InObservedPlayerState)
{
	if (ObservedPlayerState == InObservedPlayerState)
	{
		RefreshCharacterDetailsPanel();
		return;
	}

	ObservedPlayerState = InObservedPlayerState;
	RefreshCharacterDetailsPanel();
}

bool UFrontierInGameInventoryWrapperWidget::AddPanelWidget(UWidget* PanelWidget)
{
	if (!PanelWidget)
	{
		return false;
	}

	if (HasPanelWidget(PanelWidget))
	{
		return true;
	}

	if (PanelWidget->GetParent())
	{
		PanelWidget->RemoveFromParent();
	}

	if (AddPanel)
	{
		UCanvasPanelSlot* CanvasSlot = AddPanel->AddChildToCanvas(PanelWidget);
		if (!CanvasSlot)
		{
			return false;
		}

		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		CanvasSlot->SetOffsets(FMargin(0.0f));
		AttachedPanelWidgets.Add(PanelWidget);
		return true;
	}

	if (!HorizontalBox_Wrapper)
	{
		return false;
	}

	UHorizontalBoxSlot* BoxSlot = HorizontalBox_Wrapper->AddChildToHorizontalBox(PanelWidget);
	if (!BoxSlot)
	{
		return false;
	}

	if (AttachedPanelWidgets.Num() > 0)
	{
		BoxSlot->SetPadding(FMargin(16.0f, 0.0f, 0.0f, 0.0f));
	}

	AttachedPanelWidgets.Add(PanelWidget);
	return true;
}

void UFrontierInGameInventoryWrapperWidget::ClearPanelWidgets()
{
	for (UWidget* AttachedPanelWidget : AttachedPanelWidgets)
	{
		if (AttachedPanelWidget && AttachedPanelWidget->GetParent())
		{
			AttachedPanelWidget->RemoveFromParent();
		}
	}

	AttachedPanelWidgets.Reset();
}

bool UFrontierInGameInventoryWrapperWidget::HasPanelHost() const
{
	return AddPanel != nullptr || HorizontalBox_Wrapper != nullptr;
}

bool UFrontierInGameInventoryWrapperWidget::HasPanelWidget(const UWidget* PanelWidget) const
{
	return PanelWidget && AttachedPanelWidgets.Contains(PanelWidget);
}

UFrontierInventoryWidget* UFrontierInGameInventoryWrapperWidget::GetFixedInventoryWidget() const
{
	return FixedInventoryWidget;
}

void UFrontierInGameInventoryWrapperWidget::RefreshCharacterDetailsPanel()
{
	ACharacter* PreviewCharacter = GetOwningPlayer() ? Cast<ACharacter>(GetOwningPlayer()->GetPawn()) : nullptr;
	
	if (CharacterPreviewWidget)
	{
		CharacterPreviewWidget->SetPreviewCharacter(PreviewCharacter);
	}
	
}

void UFrontierInGameInventoryWrapperWidget::ClearCharacterDetailsPanel()
{
	if (CharacterPreviewWidget)
	{
		CharacterPreviewWidget->ClearPreview();
	}
}

UFrontierInventoryWidget* UFrontierInGameInventoryWrapperWidget::ResolveDropTargetInventoryWidget(const FVector2D& ScreenSpacePosition) const
{
	if (FixedInventoryWidget && FixedInventoryWidget->GetCachedGeometry().IsUnderLocation(ScreenSpacePosition))
	{
		return FixedInventoryWidget;
	}

	for (UWidget* AttachedPanelWidget : AttachedPanelWidgets)
	{
		UFrontierInventoryWidget* InventoryWidget = Cast<UFrontierInventoryWidget>(AttachedPanelWidget);
		if (InventoryWidget && InventoryWidget->GetCachedGeometry().IsUnderLocation(ScreenSpacePosition))
		{
			return InventoryWidget;
		}
	}

	if (!GetCachedGeometry().IsUnderLocation(ScreenSpacePosition))
	{
		return nullptr;
	}

	UFrontierInventoryWidget* AttachedInventoryWidget = nullptr;
	for (UWidget* AttachedPanelWidget : AttachedPanelWidgets)
	{
		if (UFrontierInventoryWidget* InventoryWidget = Cast<UFrontierInventoryWidget>(AttachedPanelWidget))
		{
			AttachedInventoryWidget = InventoryWidget;
			break;
		}
	}

	if (FixedInventoryWidget && AttachedInventoryWidget)
	{
		const FVector2D WrapperCenter = GetCachedGeometry().GetAbsolutePosition() + (GetCachedGeometry().GetAbsoluteSize() * 0.5f);
		return ScreenSpacePosition.X < WrapperCenter.X ? FixedInventoryWidget.Get() : AttachedInventoryWidget;
	}

	return AttachedInventoryWidget ? AttachedInventoryWidget : FixedInventoryWidget.Get();
}
