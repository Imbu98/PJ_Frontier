#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierUserCurrencySlot.generated.h"


UENUM(BlueprintType)
enum class ECurrencyType : uint8
{
	GOLD
};

UCLASS()
class FRONTIER_API UFrontierUserCurrencySlot : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Currency")
	void SetCurrency(ECurrencyType CurrencyType, int64 Balance);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Currency")
	TMap<ECurrencyType, TObjectPtr<UTexture2D>> CurrencyImageMap;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UImage> currencyImage;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UTextBlock> currencyTextBlock;
	
	
};
