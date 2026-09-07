#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "FrontierLoadoutSlotWidget.generated.h"

class UBorder;
class UFrontierInventoryDragDropOperation;
class UImage;
class UTexture2D;
class UTextBlock;
class UWidget;
class UFrontierInventoryWidget;
class UFrontierItemRarityBorderWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFrontierLoadoutSlotClickedSignature, UFrontierLoadoutSlotWidget*, SlotWidget, EFrontierEquipmentSlot, SlotType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFrontierLoadoutSlotDoubleClickedSignature, UFrontierLoadoutSlotWidget*, SlotWidget, EFrontierEquipmentSlot, SlotType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FFrontierLoadoutSlotDropSignature, UFrontierLoadoutSlotWidget*, SlotWidget, EFrontierEquipmentSlot, SlotType, UFrontierInventoryDragDropOperation*, DragOperation);

UCLASS(Abstract)
class FRONTIER_API UFrontierLoadoutSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void SynchronizeProperties() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UFUNCTION(BlueprintCallable, Category="Loadout Slot")
	void SetSlotData(EFrontierEquipmentSlot InSlotType, const FFrontierLoadoutSlot& InSlotData);

	UFUNCTION(BlueprintCallable, Category="Loadout Slot")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintCallable, Category="Loadout Slot")
	void BroadcastSlotClicked();

	UFUNCTION(BlueprintPure, Category="Loadout Slot")
	EFrontierEquipmentSlot GetSlotType() const;

	UFUNCTION(BlueprintPure, Category="Loadout Slot")
	FFrontierLoadoutSlot GetSlotData() const;

	UFUNCTION(BlueprintPure, Category="Loadout Slot")
	bool IsSlotOccupied() const;

	UFUNCTION(BlueprintPure, Category="Loadout Slot")
	FFrontierItemTemplateData GetItemTemplateData() const;

	UFUNCTION(BlueprintPure, Category="Loadout Slot")
	bool IsSelected() const;

	UFUNCTION(BlueprintCallable, Category="Loadout Slot")
	void SetOwningInventoryWidget(UFrontierInventoryWidget* InOwningInventoryWidget);

	UPROPERTY(BlueprintAssignable, Category="Loadout Slot")
	FFrontierLoadoutSlotClickedSignature OnSlotClicked;

	UPROPERTY(BlueprintAssignable, Category="Loadout Slot")
	FFrontierLoadoutSlotDoubleClickedSignature OnSlotDoubleClicked;

	UPROPERTY(BlueprintAssignable, Category="Loadout Slot")
	FFrontierLoadoutSlotDropSignature OnSlotDropped;

protected:
	virtual void NativeOnSlotDataChanged();
	void RefreshVisualState();
	FText GetSlotTypeDisplayText() const;
	void RequestShowTooltip();
	UFrontierInventoryWidget* ResolveOwningInventoryWidget() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loadout Slot")
	EFrontierEquipmentSlot SlotType = EFrontierEquipmentSlot::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loadout Slot")
	FFrontierLoadoutSlot SlotData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loadout Slot")
	bool bSelected = false;

	UPROPERTY(BlueprintReadOnly, Category="Loadout Slot", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UImage> ItemIconImage;

	UPROPERTY(BlueprintReadOnly, Category="Loadout Slot", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UImage> EmptySlotTypeImage;

	UPROPERTY(BlueprintReadOnly, Category="Loadout Slot", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> SlotNameText;

	UPROPERTY(BlueprintReadOnly, Category="Loadout Slot", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UBorder> SelectionBorder;

	UPROPERTY(BlueprintReadOnly, Category="Loadout Slot", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UWidget> OccupiedRoot;

	UPROPERTY(BlueprintReadOnly, Category="Loadout Slot", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UWidget> EmptyRoot;

	UPROPERTY(BlueprintReadOnly, Category="Loadout Slot", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UFrontierItemRarityBorderWidget> RarityBorderWidget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout Slot|Empty Icons")
	TSoftObjectPtr<UTexture2D> MainWeaponEmptyIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout Slot|Empty Icons")
	TSoftObjectPtr<UTexture2D> SubWeaponEmptyIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout Slot|Empty Icons")
	TSoftObjectPtr<UTexture2D> HelmetEmptyIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout Slot|Empty Icons")
	TSoftObjectPtr<UTexture2D> ChestEmptyIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout Slot|Empty Icons")
	TSoftObjectPtr<UTexture2D> GlovesEmptyIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout Slot|Empty Icons")
	TSoftObjectPtr<UTexture2D> BootsEmptyIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout Slot|Empty Icons")
	TSoftObjectPtr<UTexture2D> NecklaceEmptyIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout Slot|Empty Icons")
	TSoftObjectPtr<UTexture2D> RingEmptyIcon;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierInventoryWidget> OwningInventoryWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CachedEmptySlotIconTexture;

	EFrontierEquipmentSlot CachedEmptySlotIconType = EFrontierEquipmentSlot::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout Slot|Tooltip")
	float TooltipHoverDelay = 0.5f;

	FTimerHandle TooltipHoverTimerHandle;
	bool bSuppressTooltipUntilMouseLeave = false;
};
