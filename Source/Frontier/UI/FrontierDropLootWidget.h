#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierDropLootWidget.generated.h"

class AFrontierDroppedItemActor;
class UScrollBox;
class UFrontierDropLootEntryWidget;
class UFrontierInventoryWidget;

UCLASS(Abstract)
class FRONTIER_API UFrontierDropLootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void SetObservedDroppedItems(const TArray<AFrontierDroppedItemActor*>& InDroppedItems);
	void ClearObservedDroppedItems();
	int32 GetObservedDroppedItemCount() const;
	void SetOwningInventoryWidget(UFrontierInventoryWidget* InOwningInventoryWidget);

private:
	void RebuildEntryOrder();
	void RefreshVisibility();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dropped Loot", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UFrontierDropLootEntryWidget> EntryWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category="Dropped Loot", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UScrollBox> DroppedItemList;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFrontierDropLootEntryWidget>> CreatedEntryWidgets;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierInventoryWidget> OwningInventoryWidget;
};
