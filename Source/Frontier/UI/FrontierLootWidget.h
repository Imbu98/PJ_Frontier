#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "FrontierLootWidget.generated.h"

class AFrontierLootContainerActor;
class AFrontierPlayerState;
class UFrontierInventorySlotWidget;
class UFrontierLootInventoryComponent;
class UFrontierRaidInventoryComponent;
class UFrontierInventoryWidget;
class UFrontierItemRarityBorderWidget;
class UUniformGridPanel;
class UImage;
class UTextBlock;

UCLASS()
class FRONTIER_API UFrontierLootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category="Loot")
	void SetObservedLootContainer(AFrontierLootContainerActor* InLootContainer);

	UFUNCTION(BlueprintCallable, Category="Loot")
	void SetObservedPlayerState(AFrontierPlayerState* InPlayerState);

	UFUNCTION(BlueprintCallable, Category="Loot")
	void SetOwningInventoryWidget(UFrontierInventoryWidget* InInventoryWidget);

protected:
	UFUNCTION()
	void HandleLootInventoryChanged(const TArray<FFrontierInventorySlot>& InSlots);

	UFUNCTION()
	void HandleRaidInventoryChanged(const TArray<FFrontierInventorySlot>& InSlots);

	void RefreshFromSources();
	void RefreshLootActorText();
	void RebuildLootGrid();
	void RebuildRaidInventoryGrid();
	void ApplyInventorySlotChanges(TArray<FFrontierInventorySlot>& CachedSlots, const TArray<FFrontierInventorySlot>& NewSlots, bool bLootGrid);
	void RefreshChangedSlotWidgets(const TArray<FFrontierInventorySlot>& Slots, const TArray<int32>& ChangedSlotIndices, bool bLootGrid);
	void RefreshSelection();
	void RefreshSelectedItemInfo();

	UFUNCTION()
	void HandleLootSlotClicked(UFrontierInventorySlotWidget* SlotWidget, int32 SlotIndex);

	UFUNCTION()
	void HandleLootSlotDoubleClicked(UFrontierInventorySlotWidget* SlotWidget, int32 SlotIndex);

	UFUNCTION()
	void HandleLootSlotDropped(UFrontierInventorySlotWidget* SlotWidget, int32 SlotIndex, class UFrontierInventoryDragDropOperation* DragOperation);

	UFUNCTION()
	void HandleRaidSlotClicked(UFrontierInventorySlotWidget* SlotWidget, int32 SlotIndex);

	UFUNCTION()
	void HandleRaidSlotDoubleClicked(UFrontierInventorySlotWidget* SlotWidget, int32 SlotIndex);

	UFUNCTION()
	void HandleRaidSlotDropped(UFrontierInventorySlotWidget* SlotWidget, int32 SlotIndex, class UFrontierInventoryDragDropOperation* DragOperation);

	void UnbindLootSource();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot UI", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UFrontierInventorySlotWidget> InventorySlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot UI", meta=(ClampMin="1", AllowPrivateAccess="true"))
	int32 LootGridColumns = 5;

	UPROPERTY(BlueprintReadOnly, Category="Loot UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UUniformGridPanel> LootGrid;

	UPROPERTY(BlueprintReadOnly, Category="Loot UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UUniformGridPanel> RaidInventoryGrid;

	UPROPERTY(BlueprintReadOnly, Category="Loot UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> Text_lootactor;

	UPROPERTY(BlueprintReadOnly, Category="Loot UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UImage> SelectedItemIconImage;

	UPROPERTY(BlueprintReadOnly, Category="Loot UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UFrontierItemRarityBorderWidget> SelectedItemRarityBorderWidget;

	UPROPERTY(BlueprintReadOnly, Category="Loot UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> SelectedItemNameText;

	UPROPERTY(BlueprintReadOnly, Category="Loot UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> SelectedItemDescriptionText;

	UPROPERTY(BlueprintReadOnly, Category="Loot UI", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> SelectedItemStatsText;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierLootContainerActor> ObservedLootContainer;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierPlayerState> ObservedPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierLootInventoryComponent> BoundLootInventoryComponent;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierRaidInventoryComponent> BoundRaidInventoryComponent;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierInventoryWidget> OwningInventoryWidget;

	UPROPERTY(Transient)
	TArray<FFrontierInventorySlot> CachedLootSlots;

	UPROPERTY(Transient)
	TArray<FFrontierInventorySlot> CachedRaidSlots;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFrontierInventorySlotWidget>> CreatedLootSlotWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFrontierInventorySlotWidget>> CreatedRaidSlotWidgets;

	UPROPERTY(Transient)
	int32 SelectedLootSlotIndex = INDEX_NONE;

	UPROPERTY(Transient)
	int32 SelectedRaidSlotIndex = INDEX_NONE;
};
