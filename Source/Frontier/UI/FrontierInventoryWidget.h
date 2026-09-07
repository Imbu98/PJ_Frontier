#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "FrontierInventoryWidget.generated.h"

class AFrontierLootContainerActor;
class AFrontierPlayerState;
class UFrontierInventoryComponent;
class UFrontierLootInventoryComponent;
class UFrontierStorageComponent;
class UFrontierRaidInventoryComponent;
class UFrontierLoadoutComponent;
class UFrontierInventorySlotWidget;
class UFrontierLoadoutSlotWidget;
class UFrontierItemTooltipWidget;
class UFrontierItemRarityBorderWidget;
class UUniformGridPanel;
class UImage;
class UTextBlock;

UENUM(BlueprintType)
enum class EInventoryWidgetMode : uint8
{
	PlayerInventory,
	StorageView,
	UpgradeSelect,
	LootView,
	DeadPlayerLootView
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFrontierUpgradeItemSelectedSignature, const FFrontierItemInstance&, ItemInstance);

UCLASS()
class FRONTIER_API UFrontierInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UFUNCTION(BlueprintCallable, Category="Inventory")
	void SetObservedPlayerState(AFrontierPlayerState* InObservedPlayerState);

	UFUNCTION(BlueprintCallable, Category="Inventory")
	void SetObservedLootContainer(AFrontierLootContainerActor* InObservedLootContainer);

	UFUNCTION(BlueprintCallable, Category="Inventory")
	void SetInventoryWidgetMode(EInventoryWidgetMode InWidgetMode);

	UFUNCTION(BlueprintPure, Category="Inventory")
	EInventoryWidgetMode GetInventoryWidgetMode() const;

	UFUNCTION(BlueprintPure, Category="Inventory")
	AFrontierPlayerState* GetObservedPlayerState() const;

	UFUNCTION(BlueprintPure, Category="Inventory")
	UFrontierStorageComponent* GetStorageComponent() const;

	UFUNCTION(BlueprintPure, Category="Inventory")
	UFrontierRaidInventoryComponent* GetRaidInventoryComponent() const;

	UFUNCTION(BlueprintPure, Category="Inventory")
	UFrontierLoadoutComponent* GetLoadoutComponent() const;

	UFUNCTION(BlueprintPure, Category="Inventory")
	UFrontierLootInventoryComponent* GetLootInventoryComponent() const;

	UFUNCTION(BlueprintCallable, Category="Inventory")
	void RefreshFromPlayerState();

	UFUNCTION(BlueprintPure, Category="Inventory")
	bool IsUpgradeSelectMode() const;

	UFUNCTION(BlueprintPure, Category="Inventory")
	bool IsEquipmentItemInstance(const FFrontierItemInstance& ItemInstance) const;

	UFUNCTION(BlueprintPure, Category="Inventory")
	const TArray<FFrontierInventorySlot>& GetCachedLobbySlots() const;

	UFUNCTION(BlueprintPure, Category="Inventory")
	const TArray<FFrontierInventorySlot>& GetCachedRaidSlots() const;

	UFUNCTION(BlueprintPure, Category="Inventory")
	const TArray<FFrontierLoadoutSlot>& GetCachedLoadoutSlots() const;

	UFUNCTION(BlueprintCallable, Category="Inventory|Tooltip")
	void ShowItemTooltipForInventorySlot(UFrontierInventorySlotWidget* SlotWidget, const FFrontierItemInstance& ItemInstance);

	UFUNCTION(BlueprintCallable, Category="Inventory|Tooltip")
	void ShowItemTooltipForLoadoutSlot(UFrontierLoadoutSlotWidget* SlotWidget, const FFrontierItemInstance& ItemInstance);

	void ShowItemTooltipForWidget(UWidget* SourceWidget, const FFrontierItemInstance& ItemInstance);

	UFUNCTION(BlueprintCallable, Category="Inventory|Tooltip")
	void HideItemTooltip(UWidget* RequestingWidget);

	UPROPERTY(BlueprintAssignable, Category="Inventory|Upgrade")
	FFrontierUpgradeItemSelectedSignature OnUpgradeItemSelected;

protected:
	UFUNCTION()
	void HandleStorageChanged(const TArray<FFrontierInventorySlot>& InSlots);

	UFUNCTION()
	void HandleRaidInventoryChanged(const TArray<FFrontierInventorySlot>& InSlots);

	UFUNCTION()
	void HandleLoadoutChanged(const TArray<FFrontierLoadoutSlot>& InSlots);

	UFUNCTION()
	void HandleLootInventoryChanged(const TArray<FFrontierInventorySlot>& InSlots);

	UFUNCTION()
	void HandleDeadPlayerInventoryChanged(const TArray<FFrontierInventorySlot>& InSlots);

	UFUNCTION()
	void HandleDeadPlayerLoadoutChanged(const TArray<FFrontierLoadoutSlot>& InSlots);

	virtual void NativeOnInventoryDataChanged();
	void RebuildRaidInventoryGrid();
	void RebuildLoadoutGrid();
	void ApplyInventorySlotChanges(TArray<FFrontierInventorySlot>& CachedSlots, const TArray<FFrontierInventorySlot>& NewSlots, bool bAffectsDisplayedGrid);
	void ApplyLoadoutSlotChanges(const TArray<FFrontierLoadoutSlot>& NewSlots, bool bAffectsDisplayedGrid);
	void NormalizeInventorySlotsForDisplay(TArray<FFrontierInventorySlot>& InOutSlots, const UFrontierInventoryComponent* InventoryComponent) const;
	void NormalizeLoadoutSlotsForDisplay(TArray<FFrontierLoadoutSlot>& InOutSlots) const;
	void RefreshChangedInventorySlotWidgets(const TArray<int32>& ChangedSlotIndices);
	void RefreshChangedLoadoutSlotWidgets(const TArray<int32>& ChangedSlotIndices);
	const TArray<FFrontierInventorySlot>& GetDisplayedInventorySlots() const;
	bool CanUseSlotForUpgradeSelection(const FFrontierInventorySlot& InventorySlot) const;
	bool CanUseLoadoutSlotForUpgradeSelection(const FFrontierLoadoutSlot& LoadoutSlot) const;
	void ApplyUpgradeSelectionEnabledState();
	void BroadcastUpgradeItemSelected(const FFrontierItemInstance& ItemInstance);
	bool TryEquipRaidInventoryItem(int32 SlotIndex);
	void RefreshSelectionState();
	void RefreshSelectedItemInfo();
	void ClearSelection();

	UFUNCTION()
	void HandleInventorySlotClicked(UFrontierInventorySlotWidget* SlotWidget, int32 SlotIndex);

	UFUNCTION()
	void HandleLoadoutSlotClicked(UFrontierLoadoutSlotWidget* SlotWidget, EFrontierEquipmentSlot SlotType);

	UFUNCTION()
	void HandleInventorySlotDoubleClicked(UFrontierInventorySlotWidget* SlotWidget, int32 SlotIndex);

	UFUNCTION()
	void HandleInventorySlotRightClicked(UFrontierInventorySlotWidget* SlotWidget, int32 SlotIndex);

	UFUNCTION()
	void HandleInventorySlotAltRightClicked(UFrontierInventorySlotWidget* SlotWidget, int32 SlotIndex);

	UFUNCTION()
	void HandleLoadoutSlotDoubleClicked(UFrontierLoadoutSlotWidget* SlotWidget, EFrontierEquipmentSlot SlotType);

	UFUNCTION()
	void HandleInventorySlotDropped(UFrontierInventorySlotWidget* SlotWidget, int32 SlotIndex, class UFrontierInventoryDragDropOperation* DragOperation);

	UFUNCTION()
	void HandleLoadoutSlotDropped(UFrontierLoadoutSlotWidget* SlotWidget, EFrontierEquipmentSlot SlotType, class UFrontierInventoryDragDropOperation* DragOperation);

	void BindInventorySources();
	void UnbindInventorySources();
	void UpdateCachedData();
	void EnsureItemTooltipWidget();
	void ShowItemTooltipInternal(UWidget* SourceWidget, const FFrontierItemInstance& ItemInstance, bool bPlaceToLeft = false);
	void PositionTooltipWidget(UWidget* SourceWidget, bool bPlaceToLeft = false);

	UPROPERTY(Transient)
	TObjectPtr<AFrontierPlayerState> ObservedPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierLootContainerActor> ObservedLootContainer;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierStorageComponent> BoundStorageComponent;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierRaidInventoryComponent> BoundRaidInventoryComponent;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierLoadoutComponent> BoundLoadoutComponent;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierLootInventoryComponent> BoundLootInventoryComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory UI", meta=(AllowPrivateAccess="true"))
	EInventoryWidgetMode WidgetMode = EInventoryWidgetMode::PlayerInventory;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UFrontierInventorySlotWidget> InventorySlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UFrontierLoadoutSlotWidget> LoadoutSlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI", meta=(ClampMin="1", AllowPrivateAccess="true"))
	int32 InventoryGridColumns = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI", meta=(ClampMin="1", AllowPrivateAccess="true"))
	int32 LoadoutGridColumns = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Tooltip", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UFrontierItemTooltipWidget> ItemTooltipWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Tooltip", meta=(ClampMin="0.0", AllowPrivateAccess="true"))
	float TooltipOffsetX = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Tooltip", meta=(ClampMin="0.0", AllowPrivateAccess="true"))
	float TooltipOffsetY = 8.0f;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UUniformGridPanel> RaidInventoryGrid;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UUniformGridPanel> LoadoutGrid;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierItemTooltipWidget> ItemTooltipWidget;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> TooltipSourceWidget;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFrontierInventorySlotWidget>> CreatedInventorySlotWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFrontierLoadoutSlotWidget>> CreatedLoadoutSlotWidgets;

	UPROPERTY(Transient)
	int32 SelectedInventorySlotIndex = INDEX_NONE;

	UPROPERTY(Transient)
	EFrontierEquipmentSlot SelectedLoadoutSlotType = EFrontierEquipmentSlot::None;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UImage> SelectedItemIconImage;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UFrontierItemRarityBorderWidget> SelectedItemRarityBorderWidget;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> SelectedItemNameText;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> SelectedItemDescriptionText;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> SelectedItemTypeText;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> SelectedItemStatsText;

	UPROPERTY(BlueprintReadOnly, Category="Inventory", meta=(AllowPrivateAccess="true"))
	TArray<FFrontierInventorySlot> CachedLobbySlots;

	UPROPERTY(BlueprintReadOnly, Category="Inventory", meta=(AllowPrivateAccess="true"))
	TArray<FFrontierInventorySlot> CachedRaidSlots;

	UPROPERTY(BlueprintReadOnly, Category="Inventory", meta=(AllowPrivateAccess="true"))
	TArray<FFrontierLoadoutSlot> CachedLoadoutSlots;

	UPROPERTY(BlueprintReadOnly, Category="Inventory", meta=(AllowPrivateAccess="true"))
	TArray<FFrontierInventorySlot> CachedLootSlots;
	
	
	
	
	
};
