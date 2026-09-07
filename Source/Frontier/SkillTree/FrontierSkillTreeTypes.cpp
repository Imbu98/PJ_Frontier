#include "SkillTree/FrontierSkillTreeTypes.h"

#include "Tags/FrontierGameplayTags.h"

void FFrontierSkillTreeVisualPalette::ResolveCategoryColors(
	const FGameplayTag CategoryTag,
	FLinearColor& OutPrimary,
	FLinearColor& OutAccent) const
{
	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	if (CategoryTag.MatchesTag(Tags.SkillTreeCategorySword))
	{
		OutPrimary = SwordPrimary;
		OutAccent = SwordAccent;
		return;
	}
	if (CategoryTag.MatchesTag(Tags.SkillTreeCategoryAxe))
	{
		OutPrimary = AxePrimary;
		OutAccent = AxeAccent;
		return;
	}
	if (CategoryTag.MatchesTag(Tags.SkillTreeCategoryBow))
	{
		OutPrimary = BowPrimary;
		OutAccent = BowAccent;
		return;
	}
	if (CategoryTag.MatchesTag(Tags.SkillTreeCategorySpear))
	{
		OutPrimary = SpearPrimary;
		OutAccent = SpearAccent;
		return;
	}

	// Common, utility, legacy elemental categories, and empty categories all use the common treatment.
	OutPrimary = CommonPrimary;
	OutAccent = CommonAccent;
}

FLinearColor FFrontierSkillTreeVisualPalette::ResolveElementTint(
	const EFrontierElementalType ElementalType) const
{
	switch (ElementalType)
	{
	case EFrontierElementalType::Fire:
		return FireElementTint;
	case EFrontierElementalType::Ice:
		return IceElementTint;
	case EFrontierElementalType::Poison:
		return PoisonElementTint;
	case EFrontierElementalType::Lightning:
		return LightningElementTint;
	case EFrontierElementalType::None:
	case EFrontierElementalType::Normal:
	default:
		return NormalElementTint;
	}
}
