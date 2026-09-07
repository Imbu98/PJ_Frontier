#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierSpectatorStatusWidget.generated.h"

class UButton;
class UOverlay;
class UProgressBar;
class UTextBlock;
class UWidget;

UCLASS()
class FRONTIER_API UFrontierSpectatorStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void RefreshSpectatorState();
	UWidget* GetPreferredFocusTarget() const;

private:
	void BuildWidgetTreeIfNeeded();
	void RefreshRaidTimerText();
	void RefreshTargetText();
	void RefreshExtractionProgress();
	void SetExtractionProgress(float InProgress);
	void HandleRefreshTick();

	UFUNCTION()
	void HandlePrevSpectatorClicked();

	UFUNCTION()
	void HandleNextSpectatorClicked();

	UFUNCTION()
	void HandleEndSpectatingClicked();

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> PrevSpectatorButton;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> NextSpectatorButton;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> EndSpectatorButton;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SpectatorTargetText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RaidTimerText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UOverlay> ExtractionOverlay;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> ExtractionProgressBar;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ExtractionProgressText;

	FTimerHandle RefreshTimerHandle;
};
