#include "SkillTree/FrontierSkillTreeDataAsset.h"

#include "Misc/DataValidation.h"
#include "Tags/FrontierGameplayTags.h"

namespace
{
void AddDefaultNode(
	TArray<FFrontierSkillTreeNodeDefinition>& Nodes,
	const FGameplayTag NodeTag,
	const FGameplayTag CategoryTag,
	const FText& DisplayName,
	const FText& Description,
	const FVector2D UIPosition,
	const FGameplayTag StatTag,
	const float MagnitudePerRank)
{
	FFrontierSkillTreeNodeDefinition& Node = Nodes.AddDefaulted_GetRef();
	Node.NodeTag = NodeTag;
	Node.TreeCategoryTag = CategoryTag;
	Node.DisplayName = DisplayName;
	Node.Description = Description;
	Node.UIPosition = UIPosition;
	Node.MaxRank = 5;
	Node.RankCosts = {1, 1, 2, 2, 3};

	Node.Ranks.SetNum(Node.MaxRank);
	for (FFrontierSkillTreeRankDefinition& Rank : Node.Ranks)
	{
		FFrontierSkillTreeStatModifier& Modifier = Rank.StatModifiers.AddDefaulted_GetRef();
		Modifier.StatTag = StatTag;
		Modifier.Magnitude = MagnitudePerRank;
	}
}

void AddConditionalAttackNode(
	TArray<FFrontierSkillTreeNodeDefinition>& Nodes,
	const FGameplayTag NodeTag,
	const FGameplayTag CategoryTag,
	const FText& DisplayName,
	const FText& Description,
	const FVector2D UIPosition,
	const FGameplayTag WeaponFamilyTag,
	const EFrontierElementalType ElementalType)
{
	FFrontierSkillTreeNodeDefinition& Node = Nodes.AddDefaulted_GetRef();
	Node.NodeTag = NodeTag;
	Node.TreeCategoryTag = CategoryTag;
	Node.DisplayName = DisplayName;
	Node.Description = Description;
	Node.UIPosition = UIPosition;
	Node.MaxRank = 5;
	Node.RankCosts = {1, 1, 2, 2, 3};

	// One rank definition is intentionally reused by the existing fallback rule.
	// Therefore +3 is added for every purchased rank without duplicating editor data five times.
	FFrontierSkillTreeRankDefinition& Rank = Node.Ranks.AddDefaulted_GetRef();
	FFrontierSkillTreeConditionalAttackModifier& Modifier =
		Rank.ConditionalAttackModifiers.AddDefaulted_GetRef();
	Modifier.WeaponFamilyTag = WeaponFamilyTag;
	Modifier.ElementalType = ElementalType;
	Modifier.Magnitude = 3.0f;
}

void AddWeaponElementAttackNodes(
	TArray<FFrontierSkillTreeNodeDefinition>& Nodes,
	const FFrontierGameplayTags& Tags)
{
	struct FWeaponTemplate
	{
		const TCHAR* TagName;
		FText DisplayName;
		FGameplayTag WeaponFamilyTag;
		FGameplayTag CategoryTag;
	};
	struct FElementTemplate
	{
		const TCHAR* TagName;
		FText DisplayName;
		EFrontierElementalType ElementalType;
	};

	const TArray<FWeaponTemplate> Weapons = {
		{TEXT("Sword"), NSLOCTEXT("FrontierSkillTree", "WeaponSword", "검"), Tags.WeaponTypeSword, Tags.SkillTreeCategorySword},
		{TEXT("Axe"), NSLOCTEXT("FrontierSkillTree", "WeaponAxe", "도끼"), Tags.WeaponTypeAxe, Tags.SkillTreeCategoryAxe},
		{TEXT("Bow"), NSLOCTEXT("FrontierSkillTree", "WeaponBow", "활"), Tags.WeaponTypeBow, Tags.SkillTreeCategoryBow},
		{TEXT("Spear"), NSLOCTEXT("FrontierSkillTree", "WeaponSpear", "창"), Tags.WeaponTypeSpear, Tags.SkillTreeCategorySpear}
	};
	const TArray<FElementTemplate> Elements = {
		{TEXT("Normal"), NSLOCTEXT("FrontierSkillTree", "ElementNormal", "일반"), EFrontierElementalType::Normal},
		{TEXT("Fire"), NSLOCTEXT("FrontierSkillTree", "ElementFire", "불"), EFrontierElementalType::Fire},
		{TEXT("Ice"), NSLOCTEXT("FrontierSkillTree", "ElementIce", "얼음"), EFrontierElementalType::Ice},
		{TEXT("Poison"), NSLOCTEXT("FrontierSkillTree", "ElementPoison", "독"), EFrontierElementalType::Poison},
		{TEXT("Lightning"), NSLOCTEXT("FrontierSkillTree", "ElementLightning", "전기"), EFrontierElementalType::Lightning}
	};

	for (int32 WeaponIndex = 0; WeaponIndex < Weapons.Num(); ++WeaponIndex)
	{
		const FWeaponTemplate& Weapon = Weapons[WeaponIndex];
		for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
		{
			const FElementTemplate& Element = Elements[ElementIndex];
			const FString NodeTagName = FString::Printf(
				TEXT("SkillTree.Node.Weapon.%s.%s.AttackPower"),
				Weapon.TagName,
				Element.TagName);
			AddConditionalAttackNode(
				Nodes,
				FGameplayTag::RequestGameplayTag(FName(*NodeTagName)),
				Weapon.CategoryTag,
				FText::Format(
					NSLOCTEXT("FrontierSkillTree", "WeaponElementAttackName", "{0} {1} 공격력"),
					Weapon.DisplayName,
					Element.DisplayName),
				FText::Format(
					NSLOCTEXT("FrontierSkillTree", "WeaponElementAttackDescription", "{0} 계열 무기로 {1} 속성 공격을 할 때 공격력이 증가합니다."),
					Weapon.DisplayName,
					Element.DisplayName),
				FVector2D(-750.0f + WeaponIndex * 500.0f, -500.0f + ElementIndex * 250.0f),
				Weapon.WeaponFamilyTag,
				Element.ElementalType);
		}
	}
}

bool IsLegacyBroadAttackNode(
	const FGameplayTag NodeTag)
{
	const FName TagName = NodeTag.GetTagName();
	return TagName == TEXT("SkillTree.Node.Weapon.SwordMastery")
		|| TagName == TEXT("SkillTree.Node.Weapon.AxeMastery")
		|| TagName == TEXT("SkillTree.Node.Fire.AttackMastery")
		|| TagName == TEXT("SkillTree.Node.Ice.AttackMastery")
		|| TagName == TEXT("SkillTree.Node.Poison.AttackMastery")
		|| TagName == TEXT("SkillTree.Node.Lightning.AttackMastery");
}

void BuildDefaultNodes(TArray<FFrontierSkillTreeNodeDefinition>& Nodes)
{
	Nodes.Reset();
	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();

	// Shared gate that designers can require at any authored rank before weapon branches.
	AddDefaultNode(Nodes, Tags.SkillTreeNodeCommonAttack, Tags.SkillTreeCategoryCommon,
		NSLOCTEXT("FrontierSkillTree", "CommonAttackTraining", "공격 훈련"),
		NSLOCTEXT("FrontierSkillTree", "CommonAttackTrainingDesc", "무기별 공격 훈련을 시작하기 위한 공통 기초 공격력입니다."),
		FVector2D(-1000.0f, 0.0f), Tags.StatAttackPower, 3.0f);

	// Weapon attack-speed passives remain weapon-wide rather than element-specific.
	AddDefaultNode(Nodes, Tags.SkillTreeNodeSwordAttackSpeed, Tags.SkillTreeCategorySword,
		NSLOCTEXT("FrontierSkillTree", "SwordAttackSpeed", "검 공격속도"),
		NSLOCTEXT("FrontierSkillTree", "SwordAttackSpeedDesc", "검 계열 기본 공격의 속도가 증가합니다."),
		FVector2D(-300.0f, -300.0f), Tags.StatSwordAttackSpeedBonus, 0.03f);
	AddDefaultNode(Nodes, Tags.SkillTreeNodeAxeAttackSpeed, Tags.SkillTreeCategoryAxe,
		NSLOCTEXT("FrontierSkillTree", "AxeAttackSpeed", "도끼 공격속도"),
		NSLOCTEXT("FrontierSkillTree", "AxeAttackSpeedDesc", "도끼 계열 기본 공격의 속도가 증가합니다."),
		FVector2D(-300.0f, -100.0f), Tags.StatAxeAttackSpeedBonus, 0.03f);

	// Survival branch.
	AddDefaultNode(Nodes, Tags.SkillTreeNodeMaxHealth, Tags.SkillTreeCategoryCommon,
		NSLOCTEXT("FrontierSkillTree", "MaxHealth", "체력 증가"),
		NSLOCTEXT("FrontierSkillTree", "MaxHealthDesc", "최대 체력이 증가합니다."),
		FVector2D(0.0f, -300.0f), Tags.StatMaxHealthBonus, 10.0f);
	AddDefaultNode(Nodes, Tags.SkillTreeNodeCommonDefense, Tags.SkillTreeCategoryCommon,
		NSLOCTEXT("FrontierSkillTree", "CommonDefense", "방어력 증가"),
		NSLOCTEXT("FrontierSkillTree", "CommonDefenseDesc", "모든 피해에 적용되는 일반 방어력이 증가합니다."),
		FVector2D(300.0f, -300.0f), Tags.StatDefense, 3.0f);
	AddDefaultNode(Nodes, Tags.SkillTreeNodeFireResistance, Tags.SkillTreeCategoryFire,
		NSLOCTEXT("FrontierSkillTree", "FireResistance", "불 방어력"),
		NSLOCTEXT("FrontierSkillTree", "FireResistanceDesc", "불 속성 저항력이 증가합니다."),
		FVector2D(600.0f, -600.0f), Tags.StatFireResistance, 4.0f);
	AddDefaultNode(Nodes, Tags.SkillTreeNodeIceResistance, Tags.SkillTreeCategoryIce,
		NSLOCTEXT("FrontierSkillTree", "IceResistance", "얼음 방어력"),
		NSLOCTEXT("FrontierSkillTree", "IceResistanceDesc", "얼음 속성 저항력이 증가합니다."),
		FVector2D(600.0f, -400.0f), Tags.StatIceResistance, 4.0f);
	AddDefaultNode(Nodes, Tags.SkillTreeNodePoisonResistance, Tags.SkillTreeCategoryPoison,
		NSLOCTEXT("FrontierSkillTree", "PoisonResistance", "독 방어력"),
		NSLOCTEXT("FrontierSkillTree", "PoisonResistanceDesc", "독 속성 저항력이 증가합니다."),
		FVector2D(600.0f, -200.0f), Tags.StatPoisonResistance, 4.0f);
	AddDefaultNode(Nodes, Tags.SkillTreeNodeLightningResistance, Tags.SkillTreeCategoryLightning,
		NSLOCTEXT("FrontierSkillTree", "LightningResistance", "전기 방어력"),
		NSLOCTEXT("FrontierSkillTree", "LightningResistanceDesc", "전기 속성 저항력이 증가합니다."),
		FVector2D(600.0f, 0.0f), Tags.StatLightningResistance, 4.0f);

	// Mobility branch.
	AddDefaultNode(Nodes, Tags.SkillTreeNodeMaxStamina, Tags.SkillTreeCategoryCommon,
		NSLOCTEXT("FrontierSkillTree", "MaxStamina", "스태미나 증가"),
		NSLOCTEXT("FrontierSkillTree", "MaxStaminaDesc", "최대 스태미나가 증가합니다."),
		FVector2D(0.0f, 300.0f), Tags.StatMaxStaminaBonus, 10.0f);
	AddDefaultNode(Nodes, Tags.SkillTreeNodeMoveSpeed, Tags.SkillTreeCategoryUtility,
		NSLOCTEXT("FrontierSkillTree", "MoveSpeed", "이동속도"),
		NSLOCTEXT("FrontierSkillTree", "MoveSpeedDesc", "걷기와 달리기 속도가 증가합니다."),
		FVector2D(300.0f, 200.0f), Tags.StatMoveSpeedBonus, 0.04f);
	AddDefaultNode(Nodes, Tags.SkillTreeNodeJumpPower, Tags.SkillTreeCategoryUtility,
		NSLOCTEXT("FrontierSkillTree", "JumpPower", "점프력"),
		NSLOCTEXT("FrontierSkillTree", "JumpPowerDesc", "점프력이 증가합니다."),
		FVector2D(300.0f, 400.0f), Tags.StatJumpPowerBonus, 0.05f);

	// Capacity branch. Slot totals remain server-authoritative in the skill tree component.
	AddDefaultNode(Nodes, Tags.SkillTreeNodeLobbyStorage, Tags.SkillTreeCategoryUtility,
		NSLOCTEXT("FrontierSkillTree", "LobbyStorage", "창고 용량 증가"),
		NSLOCTEXT("FrontierSkillTree", "LobbyStorageDesc", "로비 창고의 슬롯 수가 증가합니다."),
		FVector2D(0.0f, 600.0f), Tags.StatLobbyStorageSlotBonus, 4.0f);
	AddDefaultNode(Nodes, Tags.SkillTreeNodeRaidInventory, Tags.SkillTreeCategoryUtility,
		NSLOCTEXT("FrontierSkillTree", "RaidInventory", "레이드 인벤토리 용량 증가"),
		NSLOCTEXT("FrontierSkillTree", "RaidInventoryDesc", "레이드 인벤토리의 슬롯 수가 증가합니다."),
		FVector2D(300.0f, 600.0f), Tags.StatRaidInventorySlotBonus, 2.0f);

	// Exact weapon-family x element attack nodes.
	AddWeaponElementAttackNodes(Nodes, Tags);
}
}

UFrontierSkillTreeDataAsset::UFrontierSkillTreeDataAsset()
{
	BuildDefaultNodes(Nodes);
}

void UFrontierSkillTreeDataAsset::ResetToDefaultNodes()
{
#if WITH_EDITOR
	Modify();
#endif
	BuildDefaultNodes(Nodes);
#if WITH_EDITOR
	MarkPackageDirty();
#endif
}

void UFrontierSkillTreeDataAsset::MigrateToWeaponElementAttackNodes()
{
#if WITH_EDITOR
	Modify();
#endif
	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	Nodes.RemoveAll([](const FFrontierSkillTreeNodeDefinition& Node)
	{
		return IsLegacyBroadAttackNode(Node.NodeTag);
	});
	for (FFrontierSkillTreeNodeDefinition& Node : Nodes)
	{
		Node.Prerequisites.RemoveAll([](const FFrontierSkillTreePrerequisite& Prerequisite)
		{
			return IsLegacyBroadAttackNode(Prerequisite.NodeTag);
		});
	}

	TArray<FFrontierSkillTreeNodeDefinition> WeaponElementNodes;
	AddWeaponElementAttackNodes(WeaponElementNodes, Tags);
	for (FFrontierSkillTreeNodeDefinition& NewNode : WeaponElementNodes)
	{
		const bool bAlreadyExists = Nodes.ContainsByPredicate([&NewNode](const FFrontierSkillTreeNodeDefinition& ExistingNode)
		{
			return ExistingNode.NodeTag.MatchesTagExact(NewNode.NodeTag);
		});
		if (!bAlreadyExists)
		{
			Nodes.Add(MoveTemp(NewNode));
		}
	}
#if WITH_EDITOR
	MarkPackageDirty();
#endif
}

void UFrontierSkillTreeDataAsset::ClearAllPrerequisites()
{
#if WITH_EDITOR
	Modify();
#endif
	for (FFrontierSkillTreeNodeDefinition& Node : Nodes)
	{
		Node.Prerequisites.Reset();
	}
#if WITH_EDITOR
	MarkPackageDirty();
#endif
}

bool UFrontierSkillTreeDataAsset::GetNodeDefinition(
	const FGameplayTag NodeTag,
	FFrontierSkillTreeNodeDefinition& OutNode) const
{
	if (const FFrontierSkillTreeNodeDefinition* Node = FindNode(NodeTag))
	{
		OutNode = *Node;
		return true;
	}

	return false;
}

FGameplayTag UFrontierSkillTreeDataAsset::GetNodeVisualCategoryTag(const FGameplayTag NodeTag) const
{
	const FFrontierSkillTreeNodeDefinition* Node = FindNode(NodeTag);
	return Node ? Node->TreeCategoryTag : FGameplayTag();
}

TArray<FFrontierSkillTreeConnection> UFrontierSkillTreeDataAsset::GetAllConnections() const
{
	TArray<FFrontierSkillTreeConnection> Connections;
	for (const FFrontierSkillTreeNodeDefinition& TargetNode : Nodes)
	{
		for (const FFrontierSkillTreePrerequisite& Prerequisite : TargetNode.Prerequisites)
		{
			FFrontierSkillTreeConnection& Connection = Connections.AddDefaulted_GetRef();
			Connection.SourceNodeTag = Prerequisite.NodeTag;
			Connection.TargetNodeTag = TargetNode.NodeTag;
			Connection.RequiredSourceRank = FMath::Max(1, Prerequisite.RequiredRank);
		}
	}
	return Connections;
}

TArray<FGameplayTag> UFrontierSkillTreeDataAsset::GetRootNodeTags() const
{
	TArray<FGameplayTag> RootNodeTags;
	for (const FFrontierSkillTreeNodeDefinition& Node : Nodes)
	{
		if (Node.NodeTag.IsValid() && Node.Prerequisites.IsEmpty())
		{
			RootNodeTags.Add(Node.NodeTag);
		}
	}
	return RootNodeTags;
}

TArray<FGameplayTag> UFrontierSkillTreeDataAsset::GetChildNodeTags(const FGameplayTag SourceNodeTag) const
{
	TArray<FGameplayTag> ChildNodeTags;
	if (!SourceNodeTag.IsValid())
	{
		return ChildNodeTags;
	}

	for (const FFrontierSkillTreeNodeDefinition& Node : Nodes)
	{
		if (Node.Prerequisites.ContainsByPredicate([SourceNodeTag](const FFrontierSkillTreePrerequisite& Prerequisite)
			{
				return Prerequisite.NodeTag.MatchesTagExact(SourceNodeTag);
			}))
		{
			ChildNodeTags.Add(Node.NodeTag);
		}
	}
	return ChildNodeTags;
}

const FFrontierSkillTreeNodeDefinition* UFrontierSkillTreeDataAsset::FindNode(const FGameplayTag NodeTag) const
{
	return NodeTag.IsValid()
		? Nodes.FindByPredicate([NodeTag](const FFrontierSkillTreeNodeDefinition& Node)
		{
			return Node.NodeTag.MatchesTagExact(NodeTag);
		})
		: nullptr;
}

int32 UFrontierSkillTreeDataAsset::GetRankCost(
	const FFrontierSkillTreeNodeDefinition& Node,
	const int32 RankIndex) const
{
	if (Node.RankCosts.IsEmpty())
	{
		return 1;
	}

	return FMath::Max(0, Node.RankCosts[FMath::Clamp(RankIndex, 0, Node.RankCosts.Num() - 1)]);
}

const FFrontierSkillTreeRankDefinition* UFrontierSkillTreeDataAsset::GetRankDefinition(
	const FFrontierSkillTreeNodeDefinition& Node,
	const int32 RankIndex) const
{
	if (Node.Ranks.IsEmpty())
	{
		return nullptr;
	}

	return &Node.Ranks[FMath::Clamp(RankIndex, 0, Node.Ranks.Num() - 1)];
}

#if WITH_EDITOR
EDataValidationResult UFrontierSkillTreeDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TMap<FGameplayTag, const FFrontierSkillTreeNodeDefinition*> NodesByTag;

	auto AddError = [&Context, &Result](const FText& Error)
	{
		Context.AddError(Error);
		Result = EDataValidationResult::Invalid;
	};

	for (const FFrontierSkillTreeNodeDefinition& Node : Nodes)
	{
		if (!Node.NodeTag.IsValid())
		{
			AddError(NSLOCTEXT("FrontierSkillTree", "InvalidNodeTag", "Skill tree contains a node without a valid Node Tag."));
			continue;
		}
		if (NodesByTag.Contains(Node.NodeTag))
		{
			AddError(FText::Format(
				NSLOCTEXT("FrontierSkillTree", "DuplicateNodeTag", "Skill tree contains duplicate Node Tag '{0}'."),
				FText::FromString(Node.NodeTag.ToString())));
			continue;
		}
		NodesByTag.Add(Node.NodeTag, &Node);

		if (Node.MaxRank < 1)
		{
			AddError(FText::Format(
				NSLOCTEXT("FrontierSkillTree", "InvalidMaxRank", "Node '{0}' must have Max Rank of at least 1."),
				FText::FromString(Node.NodeTag.ToString())));
		}

		for (int32 RankIndex = 0; RankIndex < Node.Ranks.Num(); ++RankIndex)
		{
			const FFrontierSkillTreeRankDefinition& Rank = Node.Ranks[RankIndex];
			TSet<FString> UniqueConditionalModifiers;
			for (const FFrontierSkillTreeConditionalAttackModifier& Modifier : Rank.ConditionalAttackModifiers)
			{
				if (!Modifier.WeaponFamilyTag.IsValid())
				{
					AddError(FText::Format(
						NSLOCTEXT("FrontierSkillTree", "InvalidConditionalWeaponTag", "Node '{0}' rank {1} has a conditional attack modifier without a Weapon Family Tag."),
						FText::FromString(Node.NodeTag.ToString()),
						FText::AsNumber(RankIndex + 1)));
					continue;
				}

				const FString ModifierKey = FString::Printf(
					TEXT("%s:%d"),
					*Modifier.WeaponFamilyTag.ToString(),
					static_cast<int32>(Modifier.ElementalType));
				if (UniqueConditionalModifiers.Contains(ModifierKey))
				{
					AddError(FText::Format(
						NSLOCTEXT("FrontierSkillTree", "DuplicateConditionalAttackModifier", "Node '{0}' rank {1} contains the same weapon/element attack modifier more than once."),
						FText::FromString(Node.NodeTag.ToString()),
						FText::AsNumber(RankIndex + 1)));
				}
				UniqueConditionalModifiers.Add(ModifierKey);
			}
		}
	}

	for (const FFrontierSkillTreeNodeDefinition& Node : Nodes)
	{
		TSet<FGameplayTag> UniquePrerequisites;
		for (const FFrontierSkillTreePrerequisite& Prerequisite : Node.Prerequisites)
		{
			if (!Prerequisite.NodeTag.IsValid())
			{
				AddError(FText::Format(
					NSLOCTEXT("FrontierSkillTree", "InvalidPrerequisiteTag", "Node '{0}' has an invalid prerequisite tag."),
					FText::FromString(Node.NodeTag.ToString())));
				continue;
			}
			if (Prerequisite.NodeTag.MatchesTagExact(Node.NodeTag))
			{
				AddError(FText::Format(
					NSLOCTEXT("FrontierSkillTree", "SelfPrerequisite", "Node '{0}' cannot require itself."),
					FText::FromString(Node.NodeTag.ToString())));
			}
			if (UniquePrerequisites.Contains(Prerequisite.NodeTag))
			{
				AddError(FText::Format(
					NSLOCTEXT("FrontierSkillTree", "DuplicatePrerequisite", "Node '{0}' contains duplicate prerequisite '{1}'."),
					FText::FromString(Node.NodeTag.ToString()),
					FText::FromString(Prerequisite.NodeTag.ToString())));
				continue;
			}
			UniquePrerequisites.Add(Prerequisite.NodeTag);

			const FFrontierSkillTreeNodeDefinition* const* SourceNode = NodesByTag.Find(Prerequisite.NodeTag);
			if (!SourceNode)
			{
				AddError(FText::Format(
					NSLOCTEXT("FrontierSkillTree", "MissingPrerequisiteNode", "Node '{0}' references missing prerequisite '{1}'."),
					FText::FromString(Node.NodeTag.ToString()),
					FText::FromString(Prerequisite.NodeTag.ToString())));
				continue;
			}
			if (Prerequisite.RequiredRank < 1 || Prerequisite.RequiredRank > FMath::Max(1, (*SourceNode)->MaxRank))
			{
				AddError(FText::Format(
					NSLOCTEXT("FrontierSkillTree", "InvalidPrerequisiteRank", "Node '{0}' requires rank {1} from '{2}', but that rank is unavailable."),
					FText::FromString(Node.NodeTag.ToString()),
					FText::AsNumber(Prerequisite.RequiredRank),
					FText::FromString(Prerequisite.NodeTag.ToString())));
			}
		}
	}

	TSet<FGameplayTag> Visiting;
	TSet<FGameplayTag> Visited;
	TFunction<bool(FGameplayTag)> HasCycle = [&](const FGameplayTag NodeTag)
	{
		if (Visiting.Contains(NodeTag))
		{
			return true;
		}
		if (Visited.Contains(NodeTag))
		{
			return false;
		}

		const FFrontierSkillTreeNodeDefinition* const* Node = NodesByTag.Find(NodeTag);
		if (!Node)
		{
			return false;
		}

		Visiting.Add(NodeTag);
		for (const FFrontierSkillTreePrerequisite& Prerequisite : (*Node)->Prerequisites)
		{
			if (NodesByTag.Contains(Prerequisite.NodeTag) && HasCycle(Prerequisite.NodeTag))
			{
				return true;
			}
		}
		Visiting.Remove(NodeTag);
		Visited.Add(NodeTag);
		return false;
	};

	for (const TPair<FGameplayTag, const FFrontierSkillTreeNodeDefinition*>& Pair : NodesByTag)
	{
		if (HasCycle(Pair.Key))
		{
			AddError(FText::Format(
				NSLOCTEXT("FrontierSkillTree", "CyclicPrerequisite", "Skill tree contains a prerequisite cycle involving '{0}'."),
				FText::FromString(Pair.Key.ToString())));
			break;
		}
	}

	return Result == EDataValidationResult::NotValidated
		? EDataValidationResult::Valid
		: Result;
}
#endif
