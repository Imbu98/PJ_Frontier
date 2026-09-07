#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "FrontierSkillTreeNodeDetailPopupWidget.generated.h"

class UFrontierSkillTreeComponent;
class UFrontierSkillTreeDataAsset;
class UImage;
class UTextBlock;

/** Self-contained node information popup. Its screen position is managed by the tree parent. */
UCLASS(Abstract, Blueprintable)
class FRONTIER_API UFrontierSkillTreeNodeDetailPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Frontier|Skill Tree|Popup")
	bool ShowNodeDetails(
		UFrontierSkillTreeComponent* SkillTreeComponent,
		const UFrontierSkillTreeDataAsset* SkillTreeData,
		FGameplayTag NodeTag);

	UFUNCTION(BlueprintCallable, Category="Frontier|Skill Tree|Popup")
	void HidePopup();

	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree|Popup")
	FGameplayTag GetDisplayedNodeTag() const { return DisplayedNodeTag; }

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> DetailIconImage;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailNameText;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailDescriptionText;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailRankText;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailCostText;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailPrerequisiteText;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailEffectText;

private:
	FGameplayTag DisplayedNodeTag;
};
