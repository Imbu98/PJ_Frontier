#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierUpgradeResultWidget.generated.h"

class UBorder;
class UImage;
class UOverlay;
class UTextBlock;
class UTexture2D;

/** Non-blocking presentation for a completed item upgrade attempt. */
UCLASS()
class FRONTIER_API UFrontierUpgradeResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category="Upgrade|Result")
	void PresentResult(
		bool bInUpgradeSucceeded,
		int32 InPreviousEnhancementLevel,
		int32 InCurrentEnhancementLevel,
		const FText& InItemName,
		UTexture2D* InItemIcon);

protected:

	UFUNCTION(BlueprintImplementableEvent, Category="Upgrade|Result", meta=(DisplayName="On Result Presented"))
	void BP_OnResultPresented(bool bInUpgradeSucceeded);
	
	void ApplyResultStyle();
	void UpdatePresentationAnimation(float DeltaTime);

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UOverlay> Overlay_ResultRoot;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UBorder> Border_ResultCard;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UBorder> Border_ResultAccent;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> Image_ItemIcon;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ResultSymbol;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ResultTitle;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ItemName;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_LevelTransition;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ResultMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upgrade|Result|Animation")
	float IntroDuration = 0.32f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upgrade|Result|Animation")
	float HoldDuration = 1.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upgrade|Result|Animation")
	float OutroDuration = 0.28f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upgrade|Result|Animation")
	FLinearColor SuccessAccentColor = FLinearColor(0.f, 1.0f, 1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upgrade|Result|Animation")
	FLinearColor FailureAccentColor = FLinearColor(0.95f, 0.12f, 0.08f, 1.0f);

private:
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PresentedItemIcon;

	FText PresentedItemName;
	int32 PreviousEnhancementLevel = 0;
	int32 CurrentEnhancementLevel = 0;
	float AnimationElapsed = 0.0f;
	bool bUpgradeSucceeded = false;
	bool bPresentationPlaying = false;
};
