#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Skill/FrontierSkillTypes.h"
#include "FrontierWeaponSkillGenerationDataAsset.generated.h"

class UFrontierSkillDataAsset;

UCLASS(BlueprintType)
class FRONTIER_API UFrontierWeaponSkillGenerationDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Skill")
	void GenerateWeaponSkills(FGameplayTag WeaponTypeTag, EFrontierElementalType WeaponElementalType, int32 WeaponLevel, TArray<FFrontierGeneratedWeaponSkill>& OutGeneratedSkills) const;

	UFUNCTION(BlueprintPure, Category="Skill")
	UFrontierSkillDataAsset* FindSkillDataAssetForWeaponType(FGameplayTag WeaponTypeTag) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Count", meta=(TitleProperty="SkillCount"))
	TArray<FFrontierWeightedSkillCount> SkillCountWeights;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Rarity", meta=(TitleProperty="SkillRarity"))
	TArray<FFrontierSkillRarityWeight> RarityWeights;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Candidates", meta=(TitleProperty="RequiredWeaponTypeTag"))
	TArray<FFrontierWeaponTypeSkillDataAsset> WeaponTypeSkillDataAssets;

protected:
	int32 SelectSkillCount(FRandomStream& RandomStream) const;
	EFrontierItemRarity SelectRarity(FRandomStream& RandomStream) const;
	bool IsCandidateAllowedByElement(EFrontierElementalType WeaponElementalType, EFrontierElementalType SkillElementalType) const;
	bool TrySelectCandidateForRarity(const UFrontierSkillDataAsset* SkillDataAsset, EFrontierElementalType WeaponElementalType, EFrontierItemRarity SkillRarity, int32 WeaponLevel, bool bAdvancedAlreadySelected, const TSet<FGameplayTag>& SelectedSkillTags, FRandomStream& RandomStream, FFrontierSkillCandidateByRarity& OutCandidate) const;
	bool TrySelectFallbackCandidate(const UFrontierSkillDataAsset* SkillDataAsset, EFrontierElementalType WeaponElementalType, int32 WeaponLevel, bool bAdvancedAlreadySelected, const TSet<FGameplayTag>& SelectedSkillTags, FRandomStream& RandomStream, EFrontierItemRarity& OutRarity, FFrontierSkillCandidateByRarity& OutCandidate) const;
};
