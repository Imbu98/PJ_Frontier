#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierSkillBarWidget.generated.h"

class UFrontierEquipmentSkillComponent;
class UFrontierSkillSlotWidget;

UCLASS()
class FRONTIER_API UFrontierSkillBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFrontierSkillBarWidget(const FObjectInitializer& ObjectInitializer);

	void SetSkillComponent(UFrontierEquipmentSkillComponent* InSkillComponent);
	void RefreshSlots();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
	void HandleEquipmentSkillsChanged();
	void CacheBoundSlotWidgets();
	FText GetKeyTextForSlot(int32 SlotIndex) const;

	UPROPERTY(EditDefaultsOnly, Category="Skill")
	TArray<FText> KeyTexts;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierEquipmentSkillComponent> SkillComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFrontierSkillSlotWidget>> SlotWidgets;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierSkillSlotWidget> WBP_SkillSlotWidget;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierSkillSlotWidget> WBP_SkillSlotWidget_1;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierSkillSlotWidget> WBP_SkillSlotWidget_2;

	float RefreshAccumulator = 0.0f;
};
