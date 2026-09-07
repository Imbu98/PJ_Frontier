#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/FrontierBackendProtocolComponent.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Progression/FrontierUpgradeBalanceSubsystem.h"
#include "FrontierUpgradeWidget.generated.h"

class UFrontierInventoryDragDropOperation;
class UFrontierInventorySlotWidget;
class UFrontierUpgradeCurrencySlot;
class UFrontierUpgradeEffectSlot;
class UFrontierUpgradeIngredientSlot;
class UFrontierUpgradeResultWidget;
class UButton;
class UImage;
class UTextBlock;
class UVerticalBox;
class UTexture2D;

UCLASS()
class FRONTIER_API UFrontierUpgradeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category="Upgrade")
	void SetSelectedUpgradeItem(const FFrontierItemInstance& ItemInstance);

	UFUNCTION(BlueprintCallable, Category="Upgrade")
	void ClearSelectedUpgradeItem();

	UFUNCTION(BlueprintPure, Category="Upgrade")
	bool HasSelectedUpgradeItem() const;

	UFUNCTION(BlueprintCallable, Category="Upgrade")
	void RefreshUpgradePreview();

protected:
	UFUNCTION()
	void HandleInfoSlotDoubleClicked(UFrontierInventorySlotWidget* SlotWidget, int32 SlotIndex);

	UFUNCTION()
	void HandleInfoSlotDropped(UFrontierInventorySlotWidget* SlotWidget, int32 SlotIndex, UFrontierInventoryDragDropOperation* DragOperation);

	UFUNCTION()
	void HandleUpgradeClicked();

	UFUNCTION()
	void HandleItemUpgradeCompleted(const FFrontierItemUpgradeResult& Result);

	UFUNCTION()
	void HandleInventoryChanged(const TArray<FFrontierInventorySlot>& Slots);

	void RefreshInfoSlot();
	void RefreshUpgradeInfoVisibility();
	void RefreshEquipmentHeader();
	void ClearPreviewSlots();
	void RefreshIngredientSlots(const FFrontierUpgradeLevelData* LevelData);
	void RefreshCurrencySlot(const FFrontierUpgradeLevelData* LevelData);
	void RefreshEffectSlots(const FFrontierUpgradeLevelData* LevelData);
	void RefreshUpgradeButtonState();
	void ShowUpgradeResult(const FFrontierItemUpgradeResult& Result);
	void ShowUpgradeResultPresentation(const FFrontierItemUpgradeResult& Result);
	FText ResolveStatDisplayName(FGameplayTag StatTag, const FString& FallbackName) const;
	int32 CountOwnedItem(FName ItemTemplateId) const;
	UTexture2D* ResolveItemIcon(FName ItemTemplateId) const;
	const FFrontierUpgradeLevelData* ResolveNextUpgradeLevel() const;
	bool IsEquipmentItemInstance(const FFrontierItemInstance& ItemInstance) const;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UFrontierInventorySlotWidget> WBP_InfoSlot; 
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UVerticalBox> Box_UpgradeInfo;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_UpgradeProbability;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_EquipmentName;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CurStep;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_NextStep;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> VerticalBox_IngredientSlotWrapper;
	
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> VerticalBox_EffectSlotWrapper;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UFrontierUpgradeCurrencySlot> WBP_UpgradeCurrencySlot;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Upgrade;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upgrade|Widgets", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UFrontierUpgradeIngredientSlot> UpgradeIngredientSlotClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upgrade|Widgets", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UFrontierUpgradeEffectSlot> UpgradeEffectSlotClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upgrade|Widgets", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UFrontierUpgradeResultWidget> UpgradeResultWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Currency", meta=(AllowPrivateAccess="true"))
	TMap<FString, TObjectPtr<UTexture2D>> CurrencyIconMap;

	UPROPERTY(Transient)
	FFrontierItemInstance SelectedUpgradeItem;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierBackendProtocolComponent> BackendProtocolComponent;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierUpgradeResultWidget> ActiveUpgradeResultWidget;

	FGuid PendingUpgradeItemId;
	int32 PendingExpectedEnhancementLevel = INDEX_NONE;
	FString PendingUpgradeIdempotencyKey;
	bool bUpgradeRequestInFlight = false;
	
};
