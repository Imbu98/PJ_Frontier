#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/FrontierInventoryDragDropOperation.h"
#include "FrontierQuickSlotWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UWidget;
class UFrontierQuickSlotComponent;
class UFrontierRaidInventoryComponent;

UENUM(BlueprintType)
enum class EFrontierQuickSlotWidgetMode : uint8
{
	Edit,
	Action
};

UCLASS()
class FRONTIER_API UFrontierQuickSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	void SetQuickSlotComponent(UFrontierQuickSlotComponent* InComponent);
	void SetQuickSlotIndex(int32 InSlotIndex);
	void SetWidgetMode(EFrontierQuickSlotWidgetMode InMode);
	void RefreshSlot();

	UFUNCTION(BlueprintPure, Category="Quick Slot")
	int32 GetQuickSlotIndex() const { return QuickSlotIndex; }

	UFUNCTION(BlueprintPure, Category="Quick Slot")
	bool IsOccupied() const;

protected:
	void HandleQuickSlotsChanged();

	UFUNCTION()
	void HandleRaidInventoryChanged(const TArray<FFrontierInventorySlot>& Slots);

	void UnbindRaidInventory();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quick Slot")
	int32 QuickSlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quick Slot")
	EFrontierQuickSlotWidgetMode WidgetMode = EFrontierQuickSlotWidgetMode::Edit;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierQuickSlotComponent> QuickSlotComponent;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierRaidInventoryComponent> BoundRaidInventoryComponent;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> ItemIconImage;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> KeyText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> OccupiedRoot;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> EmptyRoot;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UBorder> SelectionBorder;
};
