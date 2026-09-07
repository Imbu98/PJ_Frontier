#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierEquippedSkillPanelWidget.generated.h"

class AFrontierPlayerState;
class UFrontierEquipmentSkillComponent;
class UTextBlock;

UCLASS()
class FRONTIER_API UFrontierEquippedSkillPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetObservedPlayerState(AFrontierPlayerState* InPlayerState);
	void SetSkillComponent(UFrontierEquipmentSkillComponent* InSkillComponent);
	void RefreshSkills();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	void BindSkillComponent();
	void UnbindSkillComponent();
	void HandleEquipmentSkillsChanged();
	FString BuildSkillLine(int32 SlotIndex) const;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_SkillList1;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_SkillList2;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_SkillList3;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierPlayerState> ObservedPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierEquipmentSkillComponent> SkillComponent;
	
};
