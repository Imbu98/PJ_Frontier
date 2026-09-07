#include "Loot/FrontierLootItemPresetDataAsset.h"

#include "Frontier.h"

FFrontierItemInstance UFrontierLootItemPresetDataAsset::CreateTemporaryItemInstance(
	const FFrontierResolvedItemTemplateData& ResolvedItemTemplate,
	const int32 Quantity) const
{
	FRONTIER_LOG_FUNC();

	if (ItemTemplateId.IsNone()
		|| ResolvedItemTemplate.ItemTemplateId.IsNone()
		|| ResolvedItemTemplate.ItemTemplateId != ItemTemplateId)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Temporary loot preset received an invalid or mismatched ItemTemplate. Preset=%s Requested=%s Resolved=%s"),
			*GetNameSafe(this),
			*ItemTemplateId.ToString(),
			*ResolvedItemTemplate.ItemTemplateId.ToString());
		return FFrontierItemInstance();
	}

	FFrontierItemInstance ItemInstance;
	ItemInstance.ItemTemplateData = ResolvedItemTemplate;
	ItemInstance.Quantity = FMath::Max(1, Quantity);
	ItemInstance.FinalRarity = FinalRarity;
	ItemInstance.EnhancementLevel = FMath::Max(0, EnhancementLevel);
	ItemInstance.Durability = static_cast<float>(FMath::Max(0, ResolvedItemTemplate.MaxDurability));
	ItemInstance.BindState = ResolvedItemTemplate.Common.BindState;
	ItemInstance.InstanceTags = InstanceTags;
	ItemInstance.EnsureRuntimeIdentity();
	ItemInstance.RuntimeGeneratedStats.Reset(FinalStats.Num());
	for (const FFrontierAuthoredLootStat& AuthoredStat : FinalStats)
	{
		if (!AuthoredStat.StatTag.IsValid())
		{
			continue;
		}

		FFrontierRuntimeStatData& RuntimeStat = ItemInstance.RuntimeGeneratedStats.AddDefaulted_GetRef();
		RuntimeStat.OptionId = AuthoredStat.OptionId.IsNone()
			? AuthoredStat.StatTag.ToString()
			: AuthoredStat.OptionId.ToString();
		RuntimeStat.StatTag = AuthoredStat.StatTag;
		RuntimeStat.Unit = AuthoredStat.Unit;
		RuntimeStat.BaseValue = AuthoredStat.FinalValue;
		RuntimeStat.FinalValue = AuthoredStat.FinalValue;
		RuntimeStat.NormalizedValue = FMath::Clamp(AuthoredStat.NormalizedValue, 0.0f, 1.0f);
		RuntimeStat.bHasNormalizedValue = true;
	}

	ItemInstance.RuntimeGeneratedSkills.Reset(GeneratedSkills.Num());
	for (const FFrontierAuthoredLootSkill& AuthoredSkill : GeneratedSkills)
	{
		if (!AuthoredSkill.SkillTag.IsValid() && AuthoredSkill.SkillTemplateId.IsNone())
		{
			continue;
		}

		FFrontierRuntimeSkillData& RuntimeSkill = ItemInstance.RuntimeGeneratedSkills.AddDefaulted_GetRef();
		RuntimeSkill.SkillTemplateId = AuthoredSkill.SkillTemplateId.ToString();
		RuntimeSkill.SkillTag = AuthoredSkill.SkillTag;
		RuntimeSkill.SkillLevel = FMath::Max(1, AuthoredSkill.SkillLevel);
		RuntimeSkill.SlotIndex = FMath::Max(0, AuthoredSkill.SlotIndex);
	}

	FRONTIER_LOG(
		Log,
		TEXT("Created temporary authored loot item. Preset=%s ItemTemplateId=%s StatCount=%d SkillCount=%d"),
		*GetNameSafe(this),
		*ItemInstance.GetTemplateId().ToString(),
		ItemInstance.RuntimeGeneratedStats.Num(),
		ItemInstance.RuntimeGeneratedSkills.Num());
	return ItemInstance;
}
