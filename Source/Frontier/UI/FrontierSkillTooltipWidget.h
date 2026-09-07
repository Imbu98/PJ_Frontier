#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Skill/FrontierSkillTypes.h"
#include "FrontierSkillTooltipWidget.generated.h"

class UImage;
class UTextBlock;

UCLASS()
class FRONTIER_API UFrontierSkillTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Skill Tooltip")
	void SetSkillInfo(FFrontierSkillInfo InSkillInfo, int32 InSkillLevel = 1, float InCooldownOverride = -1.0f);

	UFUNCTION(BlueprintCallable, Category="Skill Tooltip")
	void ClearSkillInfo();

protected:
	virtual void NativeConstruct() override;

	void RefreshSkillTooltip();
	FText BuildFormattedDescription() const;
	FText FormatDescriptionValue(const FFrontierSkillDescriptionParameter& Parameter, float Value) const;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> SkillIcon;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> SkillDescriptionText;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> SkillNameText;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> SkillLevelText;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> SkillCost;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> SkillCoolTime;

	UPROPERTY(Transient)
	FFrontierSkillInfo CurrentSkillInfo;

	int32 CurrentSkillLevel = 1;
	float CooldownOverride = -1.0f;
};
