#include "Skill/FrontierWeaponSkillGenerationDataAsset.h"

#include "Combat/FrontierDamageStatics.h"
#include "Frontier.h"
#include "Skill/FrontierSkillDataAsset.h"

namespace
{
EFrontierItemRarity ResolveSkillRarityWeightRarity(const FFrontierSkillRarityWeight& Entry)
{
	if (Entry.SkillRarity != EFrontierItemRarity::Common || !Entry.SkillRarityTag.IsValid())
	{
		return Entry.SkillRarity;
	}

	EFrontierItemRarity ParsedRarity = EFrontierItemRarity::Common;
	return TryParseItemRarity(Entry.SkillRarityTag.ToString(), ParsedRarity) ? ParsedRarity : Entry.SkillRarity;
}

EFrontierItemRarity ResolveSkillGroupRarity(const FFrontierSkillRarityCandidateGroup& Group)
{
	if (Group.SkillRarity != EFrontierItemRarity::Common || !Group.SkillRarityTag.IsValid())
	{
		return Group.SkillRarity;
	}

	EFrontierItemRarity ParsedRarity = EFrontierItemRarity::Common;
	return TryParseItemRarity(Group.SkillRarityTag.ToString(), ParsedRarity) ? ParsedRarity : Group.SkillRarity;
}

EFrontierElementalType ResolveCandidateElementalType(const FFrontierSkillCandidateByRarity& Candidate)
{
	return Candidate.ElementalType;
}
}

void UFrontierWeaponSkillGenerationDataAsset::GenerateWeaponSkills(
	const FGameplayTag WeaponTypeTag,
	const EFrontierElementalType WeaponElementalType,
	const int32 WeaponLevel,
	TArray<FFrontierGeneratedWeaponSkill>& OutGeneratedSkills) const
{
	

	OutGeneratedSkills.Reset();
	const int32 RandomSeed = static_cast<int32>(FDateTime::Now().GetTicks())
		^ FMath::Rand()
		^ GetTypeHash(WeaponTypeTag)
		^ static_cast<int32>(WeaponElementalType)
		^ WeaponLevel;
	FRandomStream RandomStream(RandomSeed);
	const int32 SkillCount = FMath::Clamp(SelectSkillCount(RandomStream), 1, 3);
	const UFrontierSkillDataAsset* SkillDataAsset = FindSkillDataAssetForWeaponType(WeaponTypeTag);
	if (!SkillDataAsset)
	{
		FRONTIER_LOG(Warning, TEXT("Weapon skill generation failed because no skill data asset was found for weapon type. WeaponType=%s"), *WeaponTypeTag.ToString());
		return;
	}

	TSet<FGameplayTag> SelectedSkillTags;
	bool bAdvancedAlreadySelected = false;

	for (int32 SlotIndex = 0; SlotIndex < SkillCount; ++SlotIndex)
	{
		EFrontierItemRarity SelectedRarity = SelectRarity(RandomStream);
		FFrontierSkillCandidateByRarity SelectedCandidate;
		bool bSelected = TrySelectCandidateForRarity(
			SkillDataAsset,
			WeaponElementalType,
			SelectedRarity,
			WeaponLevel,
			bAdvancedAlreadySelected,
			SelectedSkillTags,
			RandomStream,
			SelectedCandidate);

		if (!bSelected)
		{
			bSelected = TrySelectFallbackCandidate(
				SkillDataAsset,
				WeaponElementalType,
				WeaponLevel,
				bAdvancedAlreadySelected,
				SelectedSkillTags,
				RandomStream,
				SelectedRarity,
				SelectedCandidate);
		}

		if (!bSelected || !SelectedCandidate.SkillTag.IsValid())
		{
			FRONTIER_LOG(Warning, TEXT("Weapon skill generation skipped slot because no valid candidate was found. Slot=%d"), SlotIndex);
			continue;
		}

		FFrontierGeneratedWeaponSkill GeneratedSkill;
		GeneratedSkill.SkillTag = SelectedCandidate.SkillTag;
		GeneratedSkill.SkillRarity = SelectedRarity;
		GeneratedSkill.SkillRarityTag = ConvertItemRarityToGameplayTag(SelectedRarity);
		GeneratedSkill.bIsAdvancedSkill = SelectedCandidate.bIsAdvancedSkill;
		GeneratedSkill.SlotIndex = OutGeneratedSkills.Num();
		OutGeneratedSkills.Add(GeneratedSkill);

		SelectedSkillTags.Add(SelectedCandidate.SkillTag);
		if (SelectedCandidate.bIsAdvancedSkill)
		{
			bAdvancedAlreadySelected = true;
		}
	}

	FRONTIER_LOG(Log, TEXT("Generated weapon skills. WeaponType=%s Count=%d"), *WeaponTypeTag.ToString(), OutGeneratedSkills.Num());
}

UFrontierSkillDataAsset* UFrontierWeaponSkillGenerationDataAsset::FindSkillDataAssetForWeaponType(const FGameplayTag WeaponTypeTag) const
{
	if (!WeaponTypeTag.IsValid())
	{
		return nullptr;
	}

	for (const FFrontierWeaponTypeSkillDataAsset& Entry : WeaponTypeSkillDataAssets)
	{
		if (!Entry.RequiredWeaponTypeTag.IsValid()
			|| Entry.WeaponTypeSkillDataAsset.IsNull()
			|| !WeaponTypeTag.MatchesTag(Entry.RequiredWeaponTypeTag))
		{
			continue;
		}

		return Entry.WeaponTypeSkillDataAsset.LoadSynchronous();
	}

	return nullptr;
}

int32 UFrontierWeaponSkillGenerationDataAsset::SelectSkillCount(FRandomStream& RandomStream) const
{
	

	float TotalWeight = 0.0f;
	for (const FFrontierWeightedSkillCount& Entry : SkillCountWeights)
	{
		TotalWeight += FMath::Max(0.0f, Entry.Weight);
	}

	if (TotalWeight <= 0.0f)
	{
		return 1;
	}

	float Roll = RandomStream.FRandRange(0.0f, TotalWeight);
	for (const FFrontierWeightedSkillCount& Entry : SkillCountWeights)
	{
		Roll -= FMath::Max(0.0f, Entry.Weight);
		if (Roll <= 0.0f)
		{
			return FMath::Clamp(Entry.SkillCount, 1, 3);
		}
	}

	return 1;
}

EFrontierItemRarity UFrontierWeaponSkillGenerationDataAsset::SelectRarity(FRandomStream& RandomStream) const
{
	

	float TotalWeight = 0.0f;
	for (const FFrontierSkillRarityWeight& Entry : RarityWeights)
	{
		TotalWeight += FMath::Max(0.0f, Entry.Weight);
	}

	if (TotalWeight <= 0.0f)
	{
		return EFrontierItemRarity::Common;
	}

	float Roll = RandomStream.FRandRange(0.0f, TotalWeight);
	for (const FFrontierSkillRarityWeight& Entry : RarityWeights)
	{
		Roll -= FMath::Max(0.0f, Entry.Weight);
		if (Roll <= 0.0f)
		{
			return ResolveSkillRarityWeightRarity(Entry);
		}
	}

	return EFrontierItemRarity::Common;
}

bool UFrontierWeaponSkillGenerationDataAsset::IsCandidateAllowedByElement(
	const EFrontierElementalType WeaponElementalType,
	const EFrontierElementalType SkillElementalType) const
{
	

	if (WeaponElementalType == EFrontierElementalType::None
		|| SkillElementalType == EFrontierElementalType::None)
	{
		return true;
	}

	if (UFrontierDamageStatics::AreElementsOpposed(WeaponElementalType, SkillElementalType))
	{
		FRONTIER_LOG(Log, TEXT("Skill candidate rejected because its element is opposed to the weapon element. WeaponElement=%d SkillElement=%d"),
			static_cast<int32>(WeaponElementalType),
			static_cast<int32>(SkillElementalType));
		return false;
	}

	return true;
}

bool UFrontierWeaponSkillGenerationDataAsset::TrySelectCandidateForRarity(
	const UFrontierSkillDataAsset* SkillDataAsset,
	const EFrontierElementalType WeaponElementalType,
	const EFrontierItemRarity SkillRarity,
	const int32 WeaponLevel,
	const bool bAdvancedAlreadySelected,
	const TSet<FGameplayTag>& SelectedSkillTags,
	FRandomStream& RandomStream,
	FFrontierSkillCandidateByRarity& OutCandidate) const
{
	

	if (!SkillDataAsset)
	{
		return false;
	}

	TArray<const FFrontierSkillCandidateByRarity*> ValidCandidates;
	float TotalWeight = 0.0f;

	for (const FFrontierSkillRarityCandidateGroup& Group : SkillDataAsset->RarityGroups)
	{
		if (ResolveSkillGroupRarity(Group) != SkillRarity)
		{
			continue;
		}

		for (const FFrontierSkillCandidateByRarity& Candidate : Group.SkillCandidates)
		{
			if (!Candidate.SkillTag.IsValid()
				|| Candidate.Weight <= 0.0f
				|| SelectedSkillTags.Contains(Candidate.SkillTag)
				|| Candidate.MinWeaponLevel > WeaponLevel
				|| (bAdvancedAlreadySelected && Candidate.bIsAdvancedSkill)
				|| !IsCandidateAllowedByElement(WeaponElementalType, ResolveCandidateElementalType(Candidate)))
			{
				continue;
			}

			ValidCandidates.Add(&Candidate);
			TotalWeight += Candidate.Weight;
		}
	}

	if (ValidCandidates.IsEmpty() || TotalWeight <= 0.0f)
	{
		return false;
	}

	float Roll = RandomStream.FRandRange(0.0f, TotalWeight);
	for (const FFrontierSkillCandidateByRarity* Candidate : ValidCandidates)
	{
		Roll -= Candidate->Weight;
		if (Roll <= 0.0f)
		{
			OutCandidate = *Candidate;
			return true;
		}
	}

	OutCandidate = *ValidCandidates.Last();
	return true;
}

bool UFrontierWeaponSkillGenerationDataAsset::TrySelectFallbackCandidate(
	const UFrontierSkillDataAsset* SkillDataAsset,
	const EFrontierElementalType WeaponElementalType,
	const int32 WeaponLevel,
	const bool bAdvancedAlreadySelected,
	const TSet<FGameplayTag>& SelectedSkillTags,
	FRandomStream& RandomStream,
	EFrontierItemRarity& OutRarity,
	FFrontierSkillCandidateByRarity& OutCandidate) const
{
	

	for (const FFrontierSkillRarityWeight& RarityWeight : RarityWeights)
	{
		if (TrySelectCandidateForRarity(
			SkillDataAsset,
			WeaponElementalType,
			ResolveSkillRarityWeightRarity(RarityWeight),
			WeaponLevel,
			bAdvancedAlreadySelected,
			SelectedSkillTags,
			RandomStream,
			OutCandidate))
		{
			OutRarity = ResolveSkillRarityWeightRarity(RarityWeight);
			return true;
		}
	}

	return false;
}
