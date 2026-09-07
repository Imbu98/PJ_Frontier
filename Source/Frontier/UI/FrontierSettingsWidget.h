#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierSettingsWidget.generated.h"

class UButton;
class USlider;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;
class UFrontierSettingsSelectButton;
class UFrontierUserSettingsSubsystem;

UCLASS()
class FRONTIER_API UFrontierSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Frontier|Settings")
	void OpenSettings();

	UFUNCTION(BlueprintCallable, Category="Frontier|Settings")
	void CloseSettings();
	void RefreshProfileInfo();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleMouseSensitivityChanged(float NewValue);

	UFUNCTION()
	void HandleMouseSensitivityCaptureEnded();

	UFUNCTION()
	void HandleSettingsSelectButtonSelected(UFrontierSettingsSelectButton* SelectedButton);

	UFUNCTION()
	void HandleChangeNicknameClicked();

	void RefreshMouseSensitivity();
	void UpdateMouseSensitivityText(float Sensitivity) const;
	void BindSettingsSelectButtons();
	void UnbindSettingsSelectButtons();
	UFrontierUserSettingsSubsystem* GetUserSettingsSubsystem() const;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_CloseSettings;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierSettingsSelectButton> Button_ControlSetting;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierSettingsSelectButton> Button_UserSetting;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_SettingConfirm;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> WidgetSwitcher_Settings;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> VerticalBox_Buttons;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<USlider> Slider_MouseSensitivity;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MouseSensitivity;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_ChangeNickName;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SteamID;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_NickName;
};
