#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierUpgradeCurrencySlot.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

UCLASS()
class FRONTIER_API UFrontierUpgradeCurrencySlot : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Upgrade")
	void SetCurrency(const FString& InCurrencyCode, UTexture2D* InIcon, int64 InRequiredAmount);

protected:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> Image_RequiredCurrencyImage;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RequiredCurrency;

	UPROPERTY(BlueprintReadOnly, Category="Upgrade")
	FString CurrencyCode;
};
