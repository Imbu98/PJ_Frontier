#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/DataTable.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "UI/FrontierItemStatDisplayDefinition.h"
#include "FrontierItemTooltipWidget.generated.h"

class UImage;
class UTextBlock;
class UFrontierArmorItemDataAsset;
class UFrontierConsumableItemDataAsset;
class UFrontierItemDataAsset;
class UFrontierSkillDataAsset;
class UFrontierWeaponItemDataAsset;
class UFrontierItemRarityBorderWidget;
class UWidget;
class UWidgetSwitcher;
struct FFrontierSkillInfo;

UCLASS()
class FRONTIER_API UFrontierItemTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Tooltip")
	void SetItemInstance(const FFrontierItemInstance& InItemInstance);

	UFUNCTION(BlueprintCallable, Category="Tooltip")
	void ClearItemInstance();

	UFUNCTION(BlueprintCallable, Category="Tooltip")
	void SetTooltipScreenPosition(const FVector2D& ScreenPosition);

	void SetTooltipViewportPositionExact(const FVector2D& ViewportPosition);

	UFUNCTION(BlueprintPure, Category="Tooltip")
	FVector2D GetTooltipContentDesiredSize() const;

protected:
	void RefreshFromItemInstance();
	FText BuildItemTypeText(EFrontierItemCategory Category) const;
	void RefreshStatsSwitcher(const FFrontierItemInstance& ItemInstance);
	void RefreshWeaponStats(const FFrontierItemInstance& ItemInstance);
	void RefreshArmorStats(const FFrontierItemInstance& ItemInstance);
	void RefreshConsumableStats(const FFrontierItemInstance& ItemInstance);
	void RefreshMiscStats();
	void RefreshCommonStats(const FFrontierItemInstance& ItemInstance);
	void RefreshEquipmentScore(FFrontierItemInstance& ItemInstance);
	FString BuildInstanceStatsText(const FFrontierItemInstance& ItemInstance) const;
	FText ResolveStatDisplayName(FGameplayTag StatTag) const;
	FString BuildGeneratedSkillsText(const FFrontierItemInstance& ItemInstance) const;
	FString BuildSkillRarityDisplayName(EFrontierItemRarity SkillRarity) const;
	FString BuildItemRarityDisplayName(EFrontierItemRarity ItemRarity) const;
	const FFrontierSkillInfo* FindSkillInfo(const FFrontierItemInstance& ItemInstance, FGameplayTag SkillTag, const FString& SkillId = FString()) const;
	void SetStatsSwitcherIndex(int32 WidgetIndex);
	static void SetTextBlockValue(UTextBlock* TextBlock, const FString& Text, bool bVisible);
	FVector2D ResolveTooltipDesiredSize() const;

	mutable FFrontierSkillInfo ResolvedSkillInfoScratch;

	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UImage> ItemIconImage;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierItemRarityBorderWidget> ItemRarityBorderWidget;

	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemEnhancementText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_EquipmentScore;

	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UTextBlock> ItemDescriptionText;

	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UTextBlock> ItemTypeText;

	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UWidgetSwitcher> ItemStatsSwitcher;

	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UWidget> TooltipContentRoot;

	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UTextBlock> WeaponAttackPowerText;

	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UTextBlock> WeaponSkillsText;

	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UTextBlock> ArmorDefenseText;

	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UTextBlock> ArmorSkillsText;

	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UTextBlock> ConsumableHealthRestoreText;
	
	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UTextBlock> WeightText;
	
	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UTextBlock> QuantityText;
	
	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UTextBlock> SellPriceText;
	
	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UTextBlock> RarityText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tooltip|Stats", meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 WeaponStatsWidgetIndex = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tooltip|Stats", meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 ArmorStatsWidgetIndex = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tooltip|Stats", meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 ConsumableStatsWidgetIndex = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tooltip|Stats", meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 MiscStatsWidgetIndex = 3;

	UPROPERTY(Transient)
	FFrontierItemInstance CurrentItemInstance;
};
