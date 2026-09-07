#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystem/Effects/FrontierAggregatedStatsGameplayEffect.h"
#include "AbilitySystem/FrontierAttributeSet.h"
#include "Combat/FrontierDamageStatics.h"
#include "Components/FrontierSkillTreeComponent.h"
#include "Components/FrontierSkillTreePersistenceComponent.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierLobbyPlayerController.h"
#include "Game/FrontierPlayerState.h"
#include "SkillTree/FrontierSkillTreeDataAsset.h"
#include "Tags/FrontierGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierDamageRatingMultiplierTest,
	"Frontier.Combat.Elemental.RatingMultiplier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierDamageRatingMultiplierTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Zero rating preserves damage"), UFrontierDamageStatics::CalculateRatingDamageMultiplier(0.0f), 1.0f);
	TestTrue(TEXT("25 rating reduces damage by 20 percent"), FMath::IsNearlyEqual(
		UFrontierDamageStatics::CalculateRatingDamageMultiplier(25.0f), 0.8f));
	TestTrue(TEXT("100 rating halves damage"), FMath::IsNearlyEqual(
		UFrontierDamageStatics::CalculateRatingDamageMultiplier(100.0f), 0.5f));
	TestTrue(TEXT("Negative 100 rating increases damage by 50 percent"), FMath::IsNearlyEqual(
		UFrontierDamageStatics::CalculateRatingDamageMultiplier(-100.0f), 1.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierElementAttributeResolutionTest,
	"Frontier.Combat.Elemental.AttributeResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierElementAttributeResolutionTest::RunTest(const FString& Parameters)
{
	FFrontierGameplayTags::InitializeNativeGameplayTags();
	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	UFrontierAttributeSet* Attributes = NewObject<UFrontierAttributeSet>();
	TestNotNull(TEXT("Attribute set exists"), Attributes);
	if (!Attributes)
	{
		return false;
	}

	// Standalone AttributeSets have no owning ASC, so initialize their base/current
	// values directly instead of invoking the runtime ASC-aware setters.
	Attributes->InitFireAttackPower(11.0f);
	Attributes->InitIceAttackPower(12.0f);
	Attributes->InitLightningAttackPower(13.0f);
	Attributes->InitPoisonAttackPower(14.0f);
	Attributes->InitFireResistance(21.0f);
	Attributes->InitIceResistance(22.0f);
	Attributes->InitLightningResistance(23.0f);
	Attributes->InitPoisonResistance(24.0f);
	Attributes->InitSwordAttackPower(31.0f);
	Attributes->InitAxeAttackPower(32.0f);
	Attributes->InitSwordAttackSpeedBonus(0.15f);
	Attributes->InitAxeAttackSpeedBonus(0.12f);

	TestEqual(TEXT("Fire attack power resolves"), UFrontierDamageStatics::GetElementAttackPower(Attributes, EFrontierElementalType::Fire), 11.0f);
	TestEqual(TEXT("Ice attack power resolves"), UFrontierDamageStatics::GetElementAttackPower(Attributes, EFrontierElementalType::Ice), 12.0f);
	TestEqual(TEXT("Lightning attack power resolves"), UFrontierDamageStatics::GetElementAttackPower(Attributes, EFrontierElementalType::Lightning), 13.0f);
	TestEqual(TEXT("Poison attack power resolves"), UFrontierDamageStatics::GetElementAttackPower(Attributes, EFrontierElementalType::Poison), 14.0f);
	TestEqual(TEXT("Normal element has no specialized attack power"), UFrontierDamageStatics::GetElementAttackPower(Attributes, EFrontierElementalType::Normal), 0.0f);

	TestEqual(TEXT("Fire resistance resolves"), UFrontierDamageStatics::GetElementResistance(Attributes, EFrontierElementalType::Fire), 21.0f);
	TestEqual(TEXT("Ice resistance resolves"), UFrontierDamageStatics::GetElementResistance(Attributes, EFrontierElementalType::Ice), 22.0f);
	TestEqual(TEXT("Lightning resistance resolves"), UFrontierDamageStatics::GetElementResistance(Attributes, EFrontierElementalType::Lightning), 23.0f);
	TestEqual(TEXT("Poison resistance resolves"), UFrontierDamageStatics::GetElementResistance(Attributes, EFrontierElementalType::Poison), 24.0f);
	TestEqual(TEXT("Sword attack power attribute is available"), Attributes->GetSwordAttackPower(), 31.0f);
	TestEqual(TEXT("Axe attack power attribute is available"), Attributes->GetAxeAttackPower(), 32.0f);
	TestEqual(TEXT("Sword attack speed attribute is available"), Attributes->GetSwordAttackSpeedBonus(), 0.15f);
	TestEqual(TEXT("Axe attack speed attribute is available"), Attributes->GetAxeAttackSpeedBonus(), 0.12f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierElementalEnumRoutingTest,
	"Frontier.Combat.Elemental.EnumRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierElementalEnumRoutingTest::RunTest(const FString& Parameters)
{
	FFrontierGameplayTags::InitializeNativeGameplayTags();
	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();

	TestTrue(TEXT("Fire has advantage over Ice"),
		UFrontierDamageStatics::IsElementalAdvantage(EFrontierElementalType::Fire, EFrontierElementalType::Ice));
	TestTrue(TEXT("Ice is disadvantaged against Fire"),
		UFrontierDamageStatics::IsElementalDisadvantage(EFrontierElementalType::Ice, EFrontierElementalType::Fire));
	TestTrue(TEXT("Fire and Ice are opposed"),
		UFrontierDamageStatics::AreElementsOpposed(EFrontierElementalType::Fire, EFrontierElementalType::Ice));
	TestFalse(TEXT("Normal is never opposed"),
		UFrontierDamageStatics::AreElementsOpposed(EFrontierElementalType::Normal, EFrontierElementalType::Fire));
	TestEqual(TEXT("Fire enum resolves the fire hit cue"),
		UFrontierDamageStatics::GetHitGameplayCueTagForElementalType(EFrontierElementalType::Fire),
		Tags.GameplayCueHitFire);
	TestEqual(TEXT("Normal enum resolves the normal hit cue"),
		UFrontierDamageStatics::GetHitGameplayCueTagForElementalType(EFrontierElementalType::Normal),
		Tags.GameplayCueHitNormal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierAggregatedStatsEffectTest,
	"Frontier.Combat.Elemental.AggregatedStatsEffect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierAggregatedStatsEffectTest::RunTest(const FString& Parameters)
{
	const UFrontierAggregatedStatsGameplayEffect* Effect = GetDefault<UFrontierAggregatedStatsGameplayEffect>();
	TestNotNull(TEXT("Aggregated stats effect exists"), Effect);
	if (!Effect)
	{
		return false;
	}

	TestEqual(TEXT("Effect owns all common, elemental, and passive modifiers"), Effect->Modifiers.Num(), 20);
	TestEqual(TEXT("Effect is infinite"), Effect->DurationPolicy, EGameplayEffectDurationType::Infinite);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierSkillTreeDefinitionTest,
	"Frontier.SkillTree.DefinitionLookupAndRankFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierSkillTreeDefinitionTest::RunTest(const FString& Parameters)
{
	FFrontierGameplayTags::InitializeNativeGameplayTags();
	UFrontierSkillTreeDataAsset* Tree = NewObject<UFrontierSkillTreeDataAsset>();
	TestNotNull(TEXT("Transient skill tree exists"), Tree);
	if (!Tree)
	{
		return false;
	}

	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	TestEqual(TEXT("Default template contains 20 conditional attacks and 14 shared/passive nodes"), Tree->Nodes.Num(), 34);
	const TArray<FGameplayTag> ExpectedDefaultNodes = {
		Tags.SkillTreeNodeCommonAttack,
		Tags.SkillTreeNodeFireResistance,
		Tags.SkillTreeNodeIceResistance,
		Tags.SkillTreeNodePoisonResistance,
		Tags.SkillTreeNodeLightningResistance,
		Tags.SkillTreeNodeMoveSpeed,
		Tags.SkillTreeNodeJumpPower,
		Tags.SkillTreeNodeRaidInventory,
		Tags.SkillTreeNodeLobbyStorage,
		Tags.SkillTreeNodeMaxStamina,
		Tags.SkillTreeNodeMaxHealth,
		Tags.SkillTreeNodeCommonDefense,
		Tags.SkillTreeNodeAxeAttackSpeed,
		Tags.SkillTreeNodeSwordAttackSpeed
	};
	for (const FGameplayTag ExpectedNodeTag : ExpectedDefaultNodes)
	{
		TestNotNull(
			*FString::Printf(TEXT("Default node exists: %s"), *ExpectedNodeTag.ToString()),
			Tree->FindNode(ExpectedNodeTag));
	}

	static const TCHAR* WeaponNames[] = {TEXT("Sword"), TEXT("Axe"), TEXT("Bow"), TEXT("Spear")};
	static const TCHAR* ElementNames[] = {TEXT("Normal"), TEXT("Fire"), TEXT("Ice"), TEXT("Poison"), TEXT("Lightning")};
	for (const TCHAR* WeaponName : WeaponNames)
	{
		for (const TCHAR* ElementName : ElementNames)
		{
			const FGameplayTag ConditionalNodeTag = FGameplayTag::RequestGameplayTag(FName(*FString::Printf(
				TEXT("SkillTree.Node.Weapon.%s.%s.AttackPower"),
				WeaponName,
				ElementName)));
			const FFrontierSkillTreeNodeDefinition* ConditionalNode = Tree->FindNode(ConditionalNodeTag);
			TestNotNull(
				*FString::Printf(TEXT("Conditional node exists: %s/%s"), WeaponName, ElementName),
				ConditionalNode);
			if (ConditionalNode)
			{
				TestEqual(TEXT("Conditional template stores one reusable rank definition"), ConditionalNode->Ranks.Num(), 1);
				TestEqual(
					TEXT("Conditional template stores one exact-match modifier"),
					ConditionalNode->Ranks[0].ConditionalAttackModifiers.Num(),
					1);
			}
		}
	}

	FFrontierSkillTreeNodeDefinition& Node = Tree->Nodes.AddDefaulted_GetRef();
	Node.NodeTag = FGameplayTag::RequestGameplayTag(TEXT("Stat.AttackPower.Fire"), false);
	Node.MaxRank = 3;
	Node.RankCosts = {2, 4};
	Node.Ranks.SetNum(2);
	Node.Ranks[0].StatModifiers.Add({FFrontierGameplayTags::Get().StatFireAttackPower, 3.0f});
	Node.Ranks[1].StatModifiers.Add({FFrontierGameplayTags::Get().StatFireAttackPower, 5.0f});

	TestNotNull(TEXT("Node lookup uses an exact tag"), Tree->FindNode(Node.NodeTag));
	TestEqual(TEXT("First rank cost resolves"), Tree->GetRankCost(Node, 0), 2);
	TestEqual(TEXT("Second rank cost resolves"), Tree->GetRankCost(Node, 1), 4);
	TestEqual(TEXT("Last cost repeats for later ranks"), Tree->GetRankCost(Node, 2), 4);
	TestTrue(TEXT("Last rank definition repeats for later ranks"), Tree->GetRankDefinition(Node, 2) == &Node.Ranks[1]);

	TestNotNull(TEXT("Sword speed passive node exists"), Tree->FindNode(Tags.SkillTreeNodeSwordAttackSpeed));
	TestNotNull(TEXT("Common attack training gate exists"), Tree->FindNode(Tags.SkillTreeNodeCommonAttack));
	TestNotNull(TEXT("Axe speed passive node exists"), Tree->FindNode(Tags.SkillTreeNodeAxeAttackSpeed));
	TestNotNull(TEXT("Maximum health passive node exists"), Tree->FindNode(Tags.SkillTreeNodeMaxHealth));
	TestNotNull(TEXT("Maximum stamina passive node exists"), Tree->FindNode(Tags.SkillTreeNodeMaxStamina));
	TestNotNull(TEXT("Move speed passive node exists"), Tree->FindNode(Tags.SkillTreeNodeMoveSpeed));
	TestNotNull(TEXT("Jump power passive node exists"), Tree->FindNode(Tags.SkillTreeNodeJumpPower));
	TestNotNull(TEXT("Lobby storage passive node exists"), Tree->FindNode(Tags.SkillTreeNodeLobbyStorage));
	TestNotNull(TEXT("Raid inventory passive node exists"), Tree->FindNode(Tags.SkillTreeNodeRaidInventory));

	const FGameplayTag SwordNormalNodeTag = FGameplayTag::RequestGameplayTag(
		TEXT("SkillTree.Node.Weapon.Sword.Normal.AttackPower"));
	Node.Prerequisites.Add({SwordNormalNodeTag, 1});
	TestEqual(TEXT("Test prerequisite creates one connection"), Tree->GetAllConnections().Num(), 1);
	Tree->ClearAllPrerequisites();

	const TArray<FGameplayTag> RootNodes = Tree->GetRootNodeTags();
	TestEqual(TEXT("Every node in the test tree starts without a prerequisite"), RootNodes.Num(), Tree->Nodes.Num());
	const TArray<FFrontierSkillTreeConnection> Connections = Tree->GetAllConnections();
	TestEqual(TEXT("Default template leaves all connections for designers to configure"), Connections.Num(), 0);

	const FGameplayTag CustomTestNodeTag = Node.NodeTag;
	const int32 NodeCountBeforeMigration = Tree->Nodes.Num();
	Tree->MigrateToWeaponElementAttackNodes();
	TestEqual(TEXT("Migration does not duplicate existing combination nodes"), Tree->Nodes.Num(), NodeCountBeforeMigration);
	TestNotNull(
		TEXT("Migration preserves unrelated designer-authored nodes"),
		Tree->FindNode(CustomTestNodeTag));

	const AFrontierPlayerState* PlayerStateCDO = GetDefault<AFrontierPlayerState>();
	TestNotNull(TEXT("PlayerState owns a skill tree component"), PlayerStateCDO ? PlayerStateCDO->GetSkillTreeComponent() : nullptr);
	const AFrontierPlayerController* RaidControllerCDO = GetDefault<AFrontierPlayerController>();
	TestNotNull(
		TEXT("Raid controller owns temporary skill tree persistence"),
		RaidControllerCDO ? RaidControllerCDO->GetSkillTreePersistenceComponent() : nullptr);
	const AFrontierLobbyPlayerController* LobbyControllerCDO = GetDefault<AFrontierLobbyPlayerController>();
	TestNotNull(
		TEXT("Lobby controller owns temporary skill tree persistence"),
		LobbyControllerCDO ? LobbyControllerCDO->GetSkillTreePersistenceComponent() : nullptr);
	return true;
}

#endif
