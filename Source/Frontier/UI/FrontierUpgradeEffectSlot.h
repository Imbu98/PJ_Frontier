#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierUpgradeEffectSlot.generated.h"

class UTextBlock;

UCLASS()
class FRONTIER_API UFrontierUpgradeEffectSlot : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Upgrade")
	void SetEffect(const FText& InEffectName, const FText& InEffectAmount);

protected:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_EffectName;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_EffectAmountText;
};
