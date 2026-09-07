#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "FrontierRaidEntryWarningWidget.generated.h"

class UButton;
class UPanelWidget;
class UFrontierInventorySlotWidget;
class UFrontierInventoryWidget;

/** Warning shown when a selected map rejects one or more equipment items. */
UCLASS(Blueprintable)
class FRONTIER_API UFrontierRaidEntryWarningWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Rebuilds the warning contents and makes this widget visible. */
	UFUNCTION(BlueprintCallable, Category="Raid Entry Warning")
	void ShowLimitedItems(const TArray<FFrontierInventorySlot>& LimitedItems);

	UFUNCTION(BlueprintCallable, Category="Raid Entry Warning")
	void HideWarning();

	UFUNCTION(BlueprintCallable, Category="Raid Entry Warning")
	void SetOwningInventoryWidget(UFrontierInventoryWidget* InOwningInventoryWidget);

protected:
	UFUNCTION()
	void HandleCloseButtonClicked();

	UButton* ResolveCloseButton();
	UPanelWidget* ResolveLimitedItemInfoPanel();

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_Close;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UPanelWidget> Horizontal_LimitedItemInfo;

	/** Concrete inventory-slot WBP used for each limited item. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid Entry Warning")
	TSubclassOf<UFrontierInventorySlotWidget> LimitedItemSlotWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierInventoryWidget> OwningInventoryWidget;
};
