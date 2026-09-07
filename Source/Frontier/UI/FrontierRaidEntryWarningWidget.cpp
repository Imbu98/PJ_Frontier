#include "UI/FrontierRaidEntryWarningWidget.h"

#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Frontier.h"
#include "UI/FrontierInventorySlotWidget.h"
#include "UI/FrontierInventoryWidget.h"

void UFrontierRaidEntryWarningWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UButton* CloseButton = ResolveCloseButton())
	{
		CloseButton->OnClicked.RemoveDynamic(
			this,
			&UFrontierRaidEntryWarningWidget::HandleCloseButtonClicked);
		CloseButton->OnClicked.AddUniqueDynamic(
			this,
			&UFrontierRaidEntryWarningWidget::HandleCloseButtonClicked);
	}
}

void UFrontierRaidEntryWarningWidget::NativeDestruct()
{
	if (UButton* CloseButton = ResolveCloseButton())
	{
		CloseButton->OnClicked.RemoveDynamic(
			this,
			&UFrontierRaidEntryWarningWidget::HandleCloseButtonClicked);
	}

	Super::NativeDestruct();
}

void UFrontierRaidEntryWarningWidget::ShowLimitedItems(
	const TArray<FFrontierInventorySlot>& LimitedItems)
{
	if (UPanelWidget* ItemPanel = ResolveLimitedItemInfoPanel())
	{
		ItemPanel->ClearChildren();

		if (!LimitedItemSlotWidgetClass)
		{
			FRONTIER_LOG(
				Warning,
				TEXT("[RaidEntry] Warning cannot display limited items because LimitedItemSlotWidgetClass is not configured."));
		}
		else
		{
			for (int32 Index = 0; Index < LimitedItems.Num(); ++Index)
			{
				UFrontierInventorySlotWidget* SlotWidget = CreateWidget<UFrontierInventorySlotWidget>(
					this,
					LimitedItemSlotWidgetClass);
				if (!SlotWidget)
				{
					continue;
				}

				SlotWidget->SetSlotData(Index, LimitedItems[Index]);
				SlotWidget->SetOwningInventoryWidget(OwningInventoryWidget);
				SlotWidget->SetInventoryCollectionType(
					EFrontierInventoryCollectionType::RaidInventory);
				ItemPanel->AddChild(SlotWidget);
			}
		}
	}

	SetVisibility(ESlateVisibility::Visible);
}

void UFrontierRaidEntryWarningWidget::HideWarning()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UFrontierRaidEntryWarningWidget::SetOwningInventoryWidget(
	UFrontierInventoryWidget* InOwningInventoryWidget)
{
	OwningInventoryWidget = InOwningInventoryWidget;
}

void UFrontierRaidEntryWarningWidget::HandleCloseButtonClicked()
{
	HideWarning();
}

UButton* UFrontierRaidEntryWarningWidget::ResolveCloseButton()
{
	if (Button_Close)
	{
		return Button_Close;
	}

	Button_Close = Cast<UButton>(GetWidgetFromName(TEXT("Button_Close")));
	if (!Button_Close)
	{
		// Compatibility with the existing warning WBP before it is renamed.
		Button_Close = Cast<UButton>(GetWidgetFromName(TEXT("Button_CloseButton")));
	}
	return Button_Close;
}

UPanelWidget* UFrontierRaidEntryWarningWidget::ResolveLimitedItemInfoPanel()
{
	if (Horizontal_LimitedItemInfo)
	{
		return Horizontal_LimitedItemInfo;
	}

	Horizontal_LimitedItemInfo = Cast<UPanelWidget>(
		GetWidgetFromName(TEXT("Horizontal_LimitedItemInfo")));
	if (!Horizontal_LimitedItemInfo)
	{
		// Compatibility with the existing warning WBP before it is renamed.
		Horizontal_LimitedItemInfo = Cast<UPanelWidget>(
			GetWidgetFromName(TEXT("HorizontalBox_LimtedItem")));
	}
	return Horizontal_LimitedItemInfo;
}
