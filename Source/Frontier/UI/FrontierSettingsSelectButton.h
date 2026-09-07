#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierSettingsSelectButton.generated.h"

class UButton;
class UImage;
class UFrontierSettingsSelectButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FFrontierSettingsSelectButtonSelectedSignature,
	UFrontierSettingsSelectButton*,
	SelectedButton);

UCLASS()
class FRONTIER_API UFrontierSettingsSelectButton : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Frontier|Settings")
	void Select();

	UFUNCTION(BlueprintCallable, Category="Frontier|Settings")
	void SetSelected(bool bNewSelected);

	UFUNCTION(BlueprintPure, Category="Frontier|Settings")
	bool IsSelected() const { return bSelected; }

	UFUNCTION(BlueprintPure, Category="Frontier|Settings")
	int32 GetSettingsPanelIndex() const { return SettingsPanelIndex; }

	UFUNCTION(BlueprintCallable, Category="Frontier|Settings")
	void SetSettingsPanelIndex(int32 NewPanelIndex)
	{
		SettingsPanelIndex = FMath::Max(0, NewPanelIndex);
	}

	UPROPERTY(BlueprintAssignable, Category="Frontier|Settings")
	FFrontierSettingsSelectButtonSelectedSignature OnSelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleClicked();

	void RefreshSelectionImage() const;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_Select;

	// Temporary fallback for assets that have not renamed Button_ControlSetting yet.
	UPROPERTY(Transient)
	TObjectPtr<UButton> LegacyButton_Select;

	// Current asset name.
	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> Image_Selected;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Settings", meta=(ClampMin="0"))
	int32 SettingsPanelIndex = 0;

private:
	UPROPERTY(Transient)
	bool bSelected = false;
};
