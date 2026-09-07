#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Skill/FrontierSkillTypes.h"
#include "FrontierSkillSlotWidget.generated.h"

class UBorder;
class UButton;
class UFrontierEquipmentSkillComponent;
class UFrontierSkillTooltipWidget;
class UImage;
class UMaterialInstanceDynamic;
class UTextBlock;

UCLASS()
class FRONTIER_API UFrontierSkillSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSkillSlot(UFrontierEquipmentSkillComponent* InSkillComponent, int32 InSlotIndex, FText InKeyText);
	void RefreshSlot();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleSkillHover();

	UFUNCTION()
	void HandleSkillUnhover();
	
	FLinearColor GetRarityColor() const;
	void UpdateCooldownMaterial(float CooldownPercent);
	void ClearSkillVisuals();
	void RefreshStaticSkillVisuals(const FFrontierGeneratedWeaponSkill& GeneratedSkill);
	void ShowSkillTooltip();
	void HideSkillTooltip();

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> SkillIconImage;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_SkillHorver;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SkillKeyText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> CoolDownProgressImage;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CooldownText;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierEquipmentSkillComponent> SkillComponent;

	int32 SlotIndex = INDEX_NONE;
	FText KeyText;
	
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CooldownMID;

	FGameplayTag CachedSkillTag;
	FFrontierSkillInfo CachedSkillInfo;
	bool bHasCachedSkillInfo = false;
	bool bSlotWasEmpty = true;

	/** Authored inside WBP_SkillSlotWidget and toggled on hover. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UFrontierSkillTooltipWidget> WBP_SkillTooltip;
};
