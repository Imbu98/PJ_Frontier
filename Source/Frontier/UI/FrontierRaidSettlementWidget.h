#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Progression/FrontierRaidSessionSubsystem.h"
#include "FrontierRaidSettlementWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UButton;
class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFrontierRaidSettlementReturnRequested);

UCLASS()
class FRONTIER_API UFrontierRaidSettlementWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void PlaySettlement(const FFrontierRaidSettlementPresentation& Presentation);

	UPROPERTY(BlueprintAssignable, Category="Frontier|Raid Settlement")
	FFrontierRaidSettlementReturnRequested OnReturnToLobbyRequested;

private:
	UFUNCTION()
	void HandleReturnToLobbyClicked();

	void StartSegment(float StartPercent, float TargetPercent);
	void UpdateAnimation();
	void RefreshLevelLabels();
	FText ResolveEscapeResultText(EFrontierRaidOutcome Outcome) const;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_ReturnToLobby;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> ProgressBar_Experience;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_CurrentLevel;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_NextLevel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_AwardedExperience;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_EscapeResult;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Raid Settlement")
	TObjectPtr<UDataTable> EscapeResultTextDataTable;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Raid Settlement", meta=(ClampMin="0.1"))
	float FillDurationPerLevel = 0.8f;

	FFrontierRaidSettlementPresentation ActivePresentation;
	int32 DisplayedLevel = 0;
	float SegmentStartPercent = 0.0f;
	float SegmentTargetPercent = 0.0f;
	double SegmentStartTime = 0.0;
	FTimerHandle AnimationTimerHandle;
};
