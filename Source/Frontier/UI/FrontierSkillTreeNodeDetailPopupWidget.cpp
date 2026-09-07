#include "UI/FrontierSkillTreeNodeDetailPopupWidget.h"

#include "Components/FrontierSkillTreeComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "SkillTree/FrontierSkillTreeDataAsset.h"
#include "Tags/FrontierGameplayTags.h"

namespace
{
FString GetWeaponFamilyDisplayName(const FGameplayTag WeaponFamilyTag)
{
	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	if (WeaponFamilyTag.MatchesTag(Tags.WeaponTypeSword)) return TEXT("검");
	if (WeaponFamilyTag.MatchesTag(Tags.WeaponTypeAxe)) return TEXT("도끼");
	if (WeaponFamilyTag.MatchesTag(Tags.WeaponTypeBow)) return TEXT("활");
	if (WeaponFamilyTag.MatchesTag(Tags.WeaponTypeSpear)) return TEXT("창");
	return WeaponFamilyTag.ToString();
}

FString GetElementDisplayName(const EFrontierElementalType ElementalType)
{
	switch (ElementalType)
	{
	case EFrontierElementalType::Fire: return TEXT("불");
	case EFrontierElementalType::Ice: return TEXT("얼음");
	case EFrontierElementalType::Poison: return TEXT("독");
	case EFrontierElementalType::Lightning: return TEXT("전기");
	case EFrontierElementalType::None:
	case EFrontierElementalType::Normal:
	default: return TEXT("일반");
	}
}
}

void UFrontierSkillTreeNodeDetailPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();
	HidePopup();
}

bool UFrontierSkillTreeNodeDetailPopupWidget::ShowNodeDetails(
	UFrontierSkillTreeComponent* SkillTreeComponent,
	const UFrontierSkillTreeDataAsset* SkillTreeData,
	const FGameplayTag NodeTag)
{
	FFrontierSkillTreeNodeDefinition Definition;
	if (!SkillTreeComponent || !SkillTreeData || !NodeTag.IsValid()
		|| !SkillTreeData->GetNodeDefinition(NodeTag, Definition))
	{
		HidePopup();
		return false;
	}

	DisplayedNodeTag = NodeTag;
	const int32 CurrentRank = SkillTreeComponent->GetNodeRank(NodeTag);
	const int32 MaxRank = FMath::Max(1, Definition.MaxRank);
	const int32 DisplayRankIndex = FMath::Min(CurrentRank, MaxRank - 1);
	const int32 PointCost = SkillTreeData->GetRankCost(Definition, DisplayRankIndex);

	if (DetailIconImage)
	{
		DetailIconImage->SetBrushFromTexture(Definition.Icon, true);
		DetailIconImage->SetVisibility(Definition.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DetailNameText)
	{
		DetailNameText->SetText(Definition.DisplayName);
	}
	if (DetailDescriptionText)
	{
		DetailDescriptionText->SetText(Definition.Description);
	}
	if (DetailRankText)
	{
		DetailRankText->SetText(FText::Format(
			NSLOCTEXT("FrontierSkillTreeUI", "DetailRankFormat", "랭크 {0}/{1}"),
			FText::AsNumber(CurrentRank),
			FText::AsNumber(MaxRank)));
	}
	if (DetailCostText)
	{
		DetailCostText->SetText(CurrentRank >= MaxRank
			? NSLOCTEXT("FrontierSkillTreeUI", "DetailMaxRank", "최대 랭크")
			: FText::Format(
				NSLOCTEXT("FrontierSkillTreeUI", "DetailCostFormat", "필요 포인트: {0}"),
				FText::AsNumber(PointCost)));
	}

	if (DetailPrerequisiteText)
	{
		TArray<FString> PrerequisiteLines;
		for (const FFrontierSkillTreePrerequisite& Prerequisite : Definition.Prerequisites)
		{
			FFrontierSkillTreeNodeDefinition PrerequisiteDefinition;
			const FString PrerequisiteName = SkillTreeData->GetNodeDefinition(
				Prerequisite.NodeTag,
				PrerequisiteDefinition)
				? PrerequisiteDefinition.DisplayName.ToString()
				: Prerequisite.NodeTag.ToString();
			PrerequisiteLines.Add(FString::Printf(
				TEXT("%s Lv.%d"),
				*PrerequisiteName,
				FMath::Max(1, Prerequisite.RequiredRank)));
		}
		DetailPrerequisiteText->SetText(PrerequisiteLines.IsEmpty()
			? NSLOCTEXT("FrontierSkillTreeUI", "NoPrerequisite", "선행 노드 없음")
			: FText::FromString(FString::Join(PrerequisiteLines, TEXT("\n"))));
	}

	if (DetailEffectText)
	{
		TArray<FString> EffectLines;
		if (const FFrontierSkillTreeRankDefinition* RankDefinition =
			SkillTreeData->GetRankDefinition(Definition, DisplayRankIndex))
		{
			for (const FFrontierSkillTreeStatModifier& Modifier : RankDefinition->StatModifiers)
			{
				EffectLines.Add(FString::Printf(TEXT("%s: %+.2f"), *Modifier.StatTag.ToString(), Modifier.Magnitude));
			}
			for (const FFrontierSkillTreeConditionalAttackModifier& Modifier : RankDefinition->ConditionalAttackModifiers)
			{
				EffectLines.Add(FString::Printf(
					TEXT("%s %s 공격력: %+.2f"),
					*GetWeaponFamilyDisplayName(Modifier.WeaponFamilyTag),
					*GetElementDisplayName(Modifier.ElementalType),
					Modifier.Magnitude));
			}
			for (const FGameplayTag& GrantedTag : RankDefinition->GrantedTags)
			{
				EffectLines.Add(GrantedTag.ToString());
			}
		}
		DetailEffectText->SetText(FText::FromString(FString::Join(EffectLines, TEXT("\n"))));
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
	ForceLayoutPrepass();
	return true;
}

void UFrontierSkillTreeNodeDetailPopupWidget::HidePopup()
{
	DisplayedNodeTag = FGameplayTag();
	SetVisibility(ESlateVisibility::Collapsed);
}
