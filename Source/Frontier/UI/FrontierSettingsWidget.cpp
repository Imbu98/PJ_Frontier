#include "UI/FrontierSettingsWidget.h"

#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierLobbyPlayerController.h"
#include "Online/FrontierPlayerSessionSubsystem.h"
#include "Settings/FrontierUserSettingsSubsystem.h"
#include "UI/FrontierSettingsSelectButton.h"

void UFrontierSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_ControlSetting)
	{
		Button_ControlSetting->SetSettingsPanelIndex(0);
	}
	if (Button_UserSetting)
	{
		Button_UserSetting->SetSettingsPanelIndex(1);
	}

	if (Button_CloseSettings)
	{
		Button_CloseSettings->OnClicked.AddUniqueDynamic(this, &UFrontierSettingsWidget::CloseSettings);
	}
	if (Button_SettingConfirm)
	{
		Button_SettingConfirm->OnClicked.AddUniqueDynamic(
			this,
			&UFrontierSettingsWidget::CloseSettings);
	}
	if (Button_ChangeNickName)
	{
		Button_ChangeNickName->OnClicked.RemoveAll(this);
		Button_ChangeNickName->OnClicked.AddDynamic(this, &ThisClass::HandleChangeNicknameClicked);
	}
	if (Slider_MouseSensitivity)
	{
		Slider_MouseSensitivity->SetMinValue(UFrontierUserSettingsSubsystem::MinimumMouseSensitivity);
		Slider_MouseSensitivity->SetMaxValue(UFrontierUserSettingsSubsystem::MaximumMouseSensitivity);
		Slider_MouseSensitivity->SetStepSize(1.0f);
		Slider_MouseSensitivity->OnValueChanged.AddUniqueDynamic(
			this,
			&UFrontierSettingsWidget::HandleMouseSensitivityChanged);
		Slider_MouseSensitivity->OnMouseCaptureEnd.AddUniqueDynamic(
			this,
			&UFrontierSettingsWidget::HandleMouseSensitivityCaptureEnded);
		Slider_MouseSensitivity->OnControllerCaptureEnd.AddUniqueDynamic(
			this,
			&UFrontierSettingsWidget::HandleMouseSensitivityCaptureEnded);
	}

	BindSettingsSelectButtons();
	RefreshMouseSensitivity();
	RefreshProfileInfo();
}

void UFrontierSettingsWidget::NativeDestruct()
{
	if (UFrontierUserSettingsSubsystem* Settings = GetUserSettingsSubsystem())
	{
		Settings->SaveSettings();
	}
	UnbindSettingsSelectButtons();

	if (Button_CloseSettings)
	{
		Button_CloseSettings->OnClicked.RemoveAll(this);
	}
	if (Button_SettingConfirm)
	{
		Button_SettingConfirm->OnClicked.RemoveAll(this);
	}
	if (Button_ChangeNickName)
	{
		Button_ChangeNickName->OnClicked.RemoveAll(this);
	}
	if (Slider_MouseSensitivity)
	{
		Slider_MouseSensitivity->OnValueChanged.RemoveAll(this);
		Slider_MouseSensitivity->OnMouseCaptureEnd.RemoveAll(this);
		Slider_MouseSensitivity->OnControllerCaptureEnd.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UFrontierSettingsWidget::OpenSettings()
{
	RefreshMouseSensitivity();
	RefreshProfileInfo();
	SetVisibility(ESlateVisibility::Visible);
}

void UFrontierSettingsWidget::RefreshProfileInfo()
{
	const UFrontierPlayerSessionSubsystem* PlayerSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (Text_SteamID)
	{
		Text_SteamID->SetText(FText::FromString(
			PlayerSession && !PlayerSession->GetSteamId().IsEmpty()
				? PlayerSession->GetSteamId()
				: TEXT("Unknown")));
	}
	if (Text_NickName)
	{
		Text_NickName->SetText(FText::FromString(
			PlayerSession && !PlayerSession->GetNickname().IsEmpty()
				? PlayerSession->GetNickname()
				: TEXT("Unknown Player")));
	}
}

void UFrontierSettingsWidget::CloseSettings()
{
	if (UFrontierUserSettingsSubsystem* Settings = GetUserSettingsSubsystem())
	{
		Settings->SaveSettings();
	}
	SetVisibility(ESlateVisibility::Collapsed);

	if (AFrontierPlayerController* PlayerController = Cast<AFrontierPlayerController>(GetOwningPlayer()))
	{
		PlayerController->HandleSettingsWidgetClosed();
	}
}

void UFrontierSettingsWidget::HandleMouseSensitivityChanged(const float NewValue)
{
	const float RoundedValue = FMath::RoundToFloat(NewValue);
	if (UFrontierUserSettingsSubsystem* Settings = GetUserSettingsSubsystem())
	{
		Settings->SetMouseSensitivity(RoundedValue);
	}
	UpdateMouseSensitivityText(RoundedValue);
}

void UFrontierSettingsWidget::HandleMouseSensitivityCaptureEnded()
{
	if (UFrontierUserSettingsSubsystem* Settings = GetUserSettingsSubsystem())
	{
		Settings->SaveSettings();
	}
}

void UFrontierSettingsWidget::HandleChangeNicknameClicked()
{
	if (AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwningPlayer()))
	{
		Controller->ShowChangeNicknameWidget(true);
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UFrontierSettingsWidget::HandleSettingsSelectButtonSelected(
	UFrontierSettingsSelectButton* SelectedButton)
{
	if (!VerticalBox_Buttons || !SelectedButton)
	{
		return;
	}

	for (int32 ChildIndex = 0; ChildIndex < VerticalBox_Buttons->GetChildrenCount(); ++ChildIndex)
	{
		if (UFrontierSettingsSelectButton* SelectButton = Cast<UFrontierSettingsSelectButton>(
			VerticalBox_Buttons->GetChildAt(ChildIndex)))
		{
			SelectButton->SetSelected(SelectButton == SelectedButton);
		}
	}

	if (WidgetSwitcher_Settings
		&& WidgetSwitcher_Settings->GetNumWidgets() > SelectedButton->GetSettingsPanelIndex())
	{
		WidgetSwitcher_Settings->SetActiveWidgetIndex(SelectedButton->GetSettingsPanelIndex());
	}
}

void UFrontierSettingsWidget::BindSettingsSelectButtons()
{
	if (!VerticalBox_Buttons)
	{
		return;
	}

	UFrontierSettingsSelectButton* InitialSelection = nullptr;

	for (int32 ChildIndex = 0; ChildIndex < VerticalBox_Buttons->GetChildrenCount(); ++ChildIndex)
	{
		UFrontierSettingsSelectButton* SelectButton = Cast<UFrontierSettingsSelectButton>(
			VerticalBox_Buttons->GetChildAt(ChildIndex));
		if (!SelectButton)
		{
			continue;
		}

		SelectButton->OnSelected.AddUniqueDynamic(
			this,
			&UFrontierSettingsWidget::HandleSettingsSelectButtonSelected);
		if (!InitialSelection)
		{
			InitialSelection = SelectButton;
		}
	}

	if (InitialSelection)
	{
		InitialSelection->Select();
	}
}

void UFrontierSettingsWidget::UnbindSettingsSelectButtons()
{
	if (!VerticalBox_Buttons)
	{
		return;
	}

	for (int32 ChildIndex = 0; ChildIndex < VerticalBox_Buttons->GetChildrenCount(); ++ChildIndex)
	{
		if (UFrontierSettingsSelectButton* SelectButton = Cast<UFrontierSettingsSelectButton>(
			VerticalBox_Buttons->GetChildAt(ChildIndex)))
		{
			SelectButton->OnSelected.RemoveDynamic(
				this,
				&UFrontierSettingsWidget::HandleSettingsSelectButtonSelected);
		}
	}
}

void UFrontierSettingsWidget::RefreshMouseSensitivity()
{
	const UFrontierUserSettingsSubsystem* Settings = GetUserSettingsSubsystem();
	const float Sensitivity = Settings
		? Settings->GetMouseSensitivity()
		: UFrontierUserSettingsSubsystem::DefaultMouseSensitivity;

	if (Slider_MouseSensitivity)
	{
		Slider_MouseSensitivity->SetValue(Sensitivity);
	}
	UpdateMouseSensitivityText(Sensitivity);
}

void UFrontierSettingsWidget::UpdateMouseSensitivityText(const float Sensitivity) const
{
	if (!Text_MouseSensitivity)
	{
		return;
	}

	FNumberFormattingOptions FormattingOptions;
	FormattingOptions.MinimumFractionalDigits = 0;
	FormattingOptions.MaximumFractionalDigits = 0;
	Text_MouseSensitivity->SetText(FText::AsNumber(FMath::RoundToInt(Sensitivity), &FormattingOptions));
}

UFrontierUserSettingsSubsystem* UFrontierSettingsWidget::GetUserSettingsSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance
		? GameInstance->GetSubsystem<UFrontierUserSettingsSubsystem>()
		: nullptr;
}
