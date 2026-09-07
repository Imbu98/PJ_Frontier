#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierFocusedItemWidget.generated.h"

class AFrontierDroppedItemActor;
class UImage;
class UFrontierItemRarityBorderWidget;
class UTextBlock;
class UWidget;

UCLASS()
class FRONTIER_API UFrontierFocusedItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Focused Item")
	void SetFocusedItemActor(AFrontierDroppedItemActor* InFocusedItemActor);

protected:
	void RefreshItemInfo();

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> FocusedItemRoot;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> FocusedItemIconImage;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierItemRarityBorderWidget> FocusedItemRarityBorderWidget;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> FocusedItemNameText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> FocusedItemDescriptionText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> FocusedItemStatsText;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierDroppedItemActor> FocusedItemActor;
};
