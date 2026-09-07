#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierDropLootEntryWidget.generated.h"

class AFrontierDroppedItemActor;
class UBorder;
class UImage;
class UTextBlock;
class UDragDropOperation;
class UFrontierInventoryWidget;
struct FStreamableHandle;

UCLASS(Abstract)
class FRONTIER_API UFrontierDropLootEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	void SetObservedDroppedItem(AFrontierDroppedItemActor* InDroppedItem);
	AFrontierDroppedItemActor* GetObservedDroppedItem() const;
	void SetOwningInventoryWidget(UFrontierInventoryWidget* InOwningInventoryWidget);

private:
	void RefreshFromDroppedItem();
	void RequestIconAsset();
	void CancelIconLoad();
	void RequestShowTooltip();
	void RequestPickup();

	UPROPERTY(BlueprintReadOnly, Category="Dropped Loot", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UBorder> EntryBorder;

	UPROPERTY(BlueprintReadOnly, Category="Dropped Loot", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UImage> ItemIconImage;

	UPROPERTY(BlueprintReadOnly, Category="Dropped Loot", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(BlueprintReadOnly, Category="Dropped Loot", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> ItemSummaryText;

	TWeakObjectPtr<AFrontierDroppedItemActor> ObservedDroppedItem;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierInventoryWidget> OwningInventoryWidget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dropped Loot|Tooltip", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float TooltipHoverDelay = 0.5f;

	FTimerHandle TooltipHoverTimerHandle;
	TSharedPtr<FStreamableHandle> IconLoadHandle;
};
