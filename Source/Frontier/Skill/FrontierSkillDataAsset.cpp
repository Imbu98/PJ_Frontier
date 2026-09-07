#include "Skill/FrontierSkillDataAsset.h"

#include "Frontier.h"

bool UFrontierSkillDataAsset::GetSkillInfo(const FGameplayTag SkillTag, FFrontierSkillInfo& OutSkillInfo) const
{
	if (const FFrontierSkillInfo* SkillInfo = FindSkillInfo(SkillTag))
	{
		OutSkillInfo = *SkillInfo;
		return true;
	}

	return false;
}

const FFrontierSkillInfo* UFrontierSkillDataAsset::FindSkillInfo(const FGameplayTag SkillTag) const
{
	if (!SkillTag.IsValid())
	{
		return nullptr;
	}

	for (const FFrontierSkillInfo& SkillInfo : Skills)
	{
		if (SkillInfo.SkillTag == SkillTag)
		{
			return &SkillInfo;
		}
	}

	return nullptr;
}
