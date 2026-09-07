#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "UI/FrontierInventoryDragDropOperation.h"
#include "FrontierInventorySlotWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UWidget;
class UFrontierInventoryWidget;
class UFrontierItemRarityBorderWidget;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFrontierInventorySlotClickedSignature, UFrontierInventorySlotWidget*, SlotWidget, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFrontierInventorySlotDoubleClickedSignature, UFrontierInventorySlotWidget*, SlotWidget, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FFrontierInventorySlotDropSignature, UFrontierInventorySlotWidget*, SlotWidget, int32, SlotIndex, UFrontierInventoryDragDropOperation*, DragOperation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFrontierInventorySlotRightClickedSignature, UFrontierInventorySlotWidget*, SlotWidget, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFrontierInventorySlotAltRightClickedSignature, UFrontierInventorySlotWidget*, SlotWidget, int32, SlotIndex);

UCLASS(Abstract)
class FRONTIER_API UFrontierInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void SynchronizeProperties() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UFUNCTION(BlueprintCallable, Category="Inventory Slot")
	void SetSlotData(int32 InSlotIndex, const FFrontierInventorySlot& InSlotData);

	UFUNCTION(BlueprintCallable, Category="Inventory Slot")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintCallable, Category="Inventory Slot")
	void BroadcastSlotClicked();

	UFUNCTION(BlueprintCallable, Category="Inventory Slot")
	void SetInventoryCollectionType(EFrontierInventoryCollectionType InCollectionType);

	UFUNCTION(BlueprintPure, Category="Inventory Slot")
	int32 GetSlotIndex() const;

	UFUNCTION(BlueprintPure, Category="Inventory Slot")
	FFrontierInventorySlot GetSlotData() const;

	UFUNCTION(BlueprintPure, Category="Inventory Slot")
	bool IsSlotOccupied() const;

	UFUNCTION(BlueprintPure, Category="Inventory Slot")
	FFrontierItemTemplateData GetItemTemplateData() const;

	UFUNCTION(BlueprintPure, Category="Inventory Slot")
	bool IsSelected() const;

	UFUNCTION(BlueprintCallable, Category="Inventory Slot")
	void SetOwningInventoryWidget(UFrontierInventoryWidget* InOwningInventoryWidget);

	UPROPERTY(BlueprintAssignable, Category="Inventory Slot")
	FFrontierInventorySlotClickedSignature OnSlotClicked;

	UPROPERTY(BlueprintAssignable, Category="Inventory Slot")
	FFrontierInventorySlotDoubleClickedSignature OnSlotDoubleClicked;

	UPROPERTY(BlueprintAssignable, Category="Inventory Slot")
	FFrontierInventorySlotDropSignature OnSlotDropped;

	UPROPERTY(BlueprintAssignable, Category="Inventory Slot")
	FFrontierInventorySlotRightClickedSignature OnSlotRightClicked;

	UPROPERTY(BlueprintAssignable, Category="Inventory Slot")
	FFrontierInventorySlotAltRightClickedSignature OnSlotAltRightClicked;

protected:
	virtual void NativeOnSlotDataChanged();
	void RefreshVisualState();
	void RequestPresentationAssets();
	void CancelPresentationLoads();
	void RequestShowTooltip();
	UFrontierInventoryWidget* ResolveOwningInventoryWidget() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory Slot")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory Slot")
	FFrontierInventorySlot SlotData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory Slot")
	bool bSelected = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory Slot")
	EFrontierInventoryCollectionType InventoryCollectionType = EFrontierInventoryCollectionType::RaidInventory;

	UPROPERTY(BlueprintReadOnly, Category="Inventory Slot", meta=(BindWidgetOptional))
	TObjectPtr<UImage> ItemIconImage;

	UPROPERTY(BlueprintReadOnly, Category="Inventory Slot", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> QuantityText;

	UPROPERTY(BlueprintReadOnly, Category="Inventory Slot", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UBorder> SelectionBorder;

	UPROPERTY(BlueprintReadOnly, Category="Inventory Slot", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UWidget> OccupiedRoot;

	UPROPERTY(BlueprintReadOnly, Category="Inventory Slot", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UWidget> EmptyRoot;

	UPROPERTY(BlueprintReadOnly, Category="Inventory Slot", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UFrontierItemRarityBorderWidget> RarityBorderWidget;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierInventoryWidget> OwningInventoryWidget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory Slot|Tooltip")
	float TooltipHoverDelay = 0.5f;

	FTimerHandle TooltipHoverTimerHandle;
	TSharedPtr<FStreamableHandle> ItemDataLoadHandle;
	TSharedPtr<FStreamableHandle> IconLoadHandle;
	bool bSuppressTooltipUntilMouseLeave = false;
};
