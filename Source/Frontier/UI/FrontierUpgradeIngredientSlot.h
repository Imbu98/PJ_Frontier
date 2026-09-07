#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierUpgradeIngredientSlot.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

UCLASS()
class FRONTIER_API UFrontierUpgradeIngredientSlot : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Upgrade")
	void SetIngredient(FName InItemTemplateId, UTexture2D* InIcon, int32 InOwnedAmount, int32 InRequiredAmount);

protected:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> Image_IngredientImage;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_IngrediantName;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_OwnedIngredientAmount;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RequiredIngredientAmount;

	UPROPERTY(BlueprintReadOnly, Category="Upgrade")
	FName ItemTemplateId;
};
