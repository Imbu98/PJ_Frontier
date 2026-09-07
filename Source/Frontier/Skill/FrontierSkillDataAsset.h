#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Skill/FrontierSkillTypes.h"
#include "FrontierSkillDataAsset.generated.h"

UCLASS(BlueprintType)
class FRONTIER_API UFrontierSkillDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Skill")
	bool GetSkillInfo(FGameplayTag SkillTag, FFrontierSkillInfo& OutSkillInfo) const;

	const FFrontierSkillInfo* FindSkillInfo(FGameplayTag SkillTag) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill", meta=(TitleProperty="SkillTag"))
	TArray<FFrontierSkillInfo> Skills;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Generation", meta=(TitleProperty="SkillRarity"))
	TArray<FFrontierSkillRarityCandidateGroup> RarityGroups;
};
