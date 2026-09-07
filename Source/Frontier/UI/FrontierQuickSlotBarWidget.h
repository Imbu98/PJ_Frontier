#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierQuickSlotBarWidget.generated.h"

class UFrontierQuickSlotComponent;
class UFrontierQuickSlotWidget;

UCLASS()
class FRONTIER_API UFrontierQuickSlotBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	void SetQuickSlotComponent(UFrontierQuickSlotComponent* InComponent);
	void SetEditMode(bool bInEditMode);
	void RefreshSlots();

protected:
	void ConfigureSlotWidgets();

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierQuickSlotWidget> QuickSlotWidget_1;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierQuickSlotWidget> QuickSlotWidget_2;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierQuickSlotWidget> QuickSlotWidget_3;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierQuickSlotComponent> QuickSlotComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quick Slot")
	bool bEditMode = false;
};
