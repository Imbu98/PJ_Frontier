#include "UI/FrontierSettingsSelectButton.h"

#include "Components/Button.h"
#include "Components/Image.h"

void UFrontierSettingsSelectButton::NativeConstruct()
{
	Super::NativeConstruct();

	LegacyButton_Select = Button_Select
		? nullptr
		: Cast<UButton>(GetWidgetFromName(TEXT("Button_ControlSetting")));
	UButton* SelectButton = Button_Select ? Button_Select.Get() : LegacyButton_Select.Get();
	if (SelectButton)
	{
		SelectButton->OnClicked.AddUniqueDynamic(
			this,
			&UFrontierSettingsSelectButton::HandleClicked);
	}
	RefreshSelectionImage();
}

void UFrontierSettingsSelectButton::NativeDestruct()
{
	UButton* SelectButton = Button_Select ? Button_Select.Get() : LegacyButton_Select.Get();
	if (SelectButton)
	{
		SelectButton->OnClicked.RemoveAll(this);
	}
	LegacyButton_Select = nullptr;
	Super::NativeDestruct();
}

void UFrontierSettingsSelectButton::SetSelected(const bool bNewSelected)
{
	if (bSelected == bNewSelected)
	{
		RefreshSelectionImage();
		return;
	}

	bSelected = bNewSelected;
	RefreshSelectionImage();
}

void UFrontierSettingsSelectButton::Select()
{
	OnSelected.Broadcast(this);
}

void UFrontierSettingsSelectButton::HandleClicked()
{
	Select();
}

void UFrontierSettingsSelectButton::RefreshSelectionImage() const
{
	const ESlateVisibility SelectionVisibility = bSelected
		? ESlateVisibility::Visible
		: ESlateVisibility::Collapsed;

	if (Image_Selected)
	{
		Image_Selected->SetVisibility(SelectionVisibility);
	}
}
