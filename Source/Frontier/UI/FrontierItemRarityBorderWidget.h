#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/FrontierItemSharedTypes.h"
#include "FrontierItemRarityBorderWidget.generated.h"

class UBorder;

UCLASS()
class FRONTIER_API UFrontierItemRarityBorderWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Rarity Border")
	void SetItemRarity(EFrontierItemRarity Rarity);

	UFUNCTION(BlueprintCallable, Category="Rarity Border", meta=(DeprecatedFunction, DeprecationMessage="Use SetItemRarity with EFrontierItemRarity."))
	void SetItemRarityTag(FGameplayTag RarityTag);

	UFUNCTION(BlueprintCallable, Category="Rarity Border")
	void ClearRarity();

protected:
	virtual void NativeConstruct() override;

	FLinearColor GetColorForRarity(EFrontierItemRarity Rarity) const;

	UPROPERTY(BlueprintReadOnly, Category="Rarity Border", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UBorder> RarityBorder;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rarity Border", meta=(AllowPrivateAccess="true"))
	FLinearColor NoneColor = FLinearColor(0.15f, 0.15f, 0.15f, 0.25f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rarity Border", meta=(AllowPrivateAccess="true"))
	FLinearColor CommonColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rarity Border", meta=(AllowPrivateAccess="true"))
	FLinearColor RareColor = FLinearColor(0.1f, 0.35f, 1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rarity Border", meta=(AllowPrivateAccess="true"))
	FLinearColor EpicColor = FLinearColor(0.55f, 0.15f, 1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rarity Border", meta=(AllowPrivateAccess="true"))
	FLinearColor LegendaryColor = FLinearColor(1.0f, 0.55f, 0.05f, 1.0f);

	UPROPERTY(Transient)
	EFrontierItemRarity CurrentRarity = EFrontierItemRarity::Common;

	UPROPERTY(Transient)
	bool bHasCurrentRarity = false;
};
