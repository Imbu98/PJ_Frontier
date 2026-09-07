#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Inventory/Items/FrontierItemDataAsset.h"
#include "Inventory/Items/FrontierItemCatalogSubsystem.h"
#include "Inventory/Persistence/FrontierItemPersistenceTypes.h"
#include "Tags/FrontierGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierItemPersistenceDTOBridgeTest,
	"Frontier.Item.Persistence.DTOBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierItemPersistenceDTOBridgeTest::RunTest(const FString& Parameters)
{
	FFrontierGameplayTags::InitializeNativeGameplayTags();
	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();

	UFrontierItemDataAsset* ItemData = NewObject<UFrontierItemDataAsset>();
	TestNotNull(TEXT("Transient Item DataAsset exists"), ItemData);
	if (!ItemData)
	{
		return false;
	}
	ItemData->ItemId = TEXT("Item.Weapon.PersistenceTest");
	ItemData->ItemType = EFrontierItemType::Weapon;
	ItemData->Category = EFrontierItemCategory::Weapon;
	ItemData->ElementalType = EFrontierElementalType::Normal;
	ItemData->bStackable = false;
	const FFrontierResolvedItemTemplateData ItemTemplateData = UFrontierItemCatalogSubsystem::BuildTemplateDataFromItemData(*ItemData);

	const FGuid PersistentId = FGuid::NewGuid();
	FFrontierItemPersistenceDTO DTO;
	DTO.ItemInstanceId = PersistentId.ToString(EGuidFormats::DigitsWithHyphensLower);
	DTO.ItemTemplateId = ItemData->ItemId.ToString();
	DTO.Quantity = 1;
	DTO.Durability = 87.5;
	DTO.EnhancementLevel = 2;
	DTO.FinalRarityTag = Tags.ItemRarityRare.ToString();
	DTO.BindState = EFrontierItemBindState::CharacterBound;
	DTO.InstanceTags.Add(Tags.ItemTypeEquipment.ToString());
	DTO.CreatedAt = TEXT("2026-07-15T00:00:00Z");
	DTO.UpdatedAt = TEXT("2026-07-15T00:01:00Z");
	DTO.MetadataJson = TEXT("{\"event\":{\"season\":1}}");
	DTO.Source.SourceType = EFrontierItemSourceType::RaidLoot;
	DTO.Source.SourceId = TEXT("raid-test-1");
	DTO.Source.CorrelationId = TEXT("0190f7e5-2a54-7595-8b37-a5ef79b182b5");

	FFrontierItemOptionDTO& OptionDTO = DTO.RandomOptions.AddDefaulted_GetRef();
	OptionDTO.OptionId = Tags.StatAttackPower.ToString();
	OptionDTO.BaseValue = 10.0;
	OptionDTO.RandomValue = 3.0;
	OptionDTO.UpgradeValue = 3.0;
	OptionDTO.FinalValue = 16.0;

	FFrontierGeneratedItemSkillDTO& SkillDTO = DTO.GeneratedSkills.AddDefaulted_GetRef();
	SkillDTO.SkillId = Tags.AbilityAttackPrimary.ToString();
	SkillDTO.SlotIndex = 0;

	FFrontierItemInstance RuntimeItem;
	FString ConversionError;
	TestTrue(
		TEXT("Backend ItemDTO converts to the existing runtime item"),
		FFrontierItemPersistenceMapper::TryBuildRuntimeItem(DTO, ItemTemplateData, RuntimeItem, ConversionError));
	TestTrue(*FString::Printf(TEXT("Conversion error is empty: %s"), *ConversionError), ConversionError.IsEmpty());
	TestEqual(TEXT("Server itemInstanceId is preserved"), RuntimeItem.ItemInstanceId, PersistentId);
	TestEqual(TEXT("Template ID is mapped without an asset path"), RuntimeItem.GetTemplateId(), ItemData->ItemId);
	TestTrue(TEXT("TemplateData is stored on runtime item"), RuntimeItem.HasItemTemplateData());
	TestEqual(TEXT("Quantity is preserved"), RuntimeItem.Quantity, 1);
	TestEqual(TEXT("Durability is preserved"), RuntimeItem.Durability, 87.5f);
	TestEqual(TEXT("Rarity tag string migrates to enum"), RuntimeItem.FinalRarity, EFrontierItemRarity::Rare);
	TestEqual(TEXT("Character-bound DTO preserves bind state"), RuntimeItem.BindState, EFrontierItemBindState::CharacterBound);
	TestEqual(TEXT("Random option is reconstructed"), RuntimeItem.RuntimeGeneratedStats.Num(), 1);
	TestEqual(TEXT("Generated skill is reconstructed"), RuntimeItem.RuntimeGeneratedSkills.Num(), 1);
	if (RuntimeItem.RuntimeGeneratedStats.Num() == 1)
	{
		TestEqual(TEXT("Persistent final stat is reconstructed"), RuntimeItem.RuntimeGeneratedStats[0].FinalValue, 16.0f);
		TestEqual(TEXT("Runtime random value is reconstructed"), RuntimeItem.RuntimeGeneratedStats[0].RandomValue, 3.0f);
	}

	FFrontierItemPersistenceDTO MetadataOnlyDTO = DTO;
	MetadataOnlyDTO.RandomOptions.Reset();
	MetadataOnlyDTO.GeneratedSkills.Reset();
	MetadataOnlyDTO.MetadataJson = FString::Printf(
		TEXT("{\"randomOptions\":[{\"statTag\":\"%s\",\"baseValue\":10,\"rolledValue\":13,\"enhancementBonus\":2,\"reforgeBonus\":1,\"finalValue\":16,\"score\":5}],\"generatedSkills\":[{\"skillId\":\"%s\",\"skillRarityTag\":\"%s\",\"advanced\":true,\"slotIndex\":0}]}"),
		*Tags.StatAttackPower.ToString(),
		*Tags.AbilityAttackPrimary.ToString(),
		*Tags.ItemRarityRare.ToString());

	FFrontierItemInstance MetadataOnlyRuntimeItem;
	TestTrue(
		TEXT("Item metadata randomOptions/generatedSkills convert to runtime item"),
		FFrontierItemPersistenceMapper::TryBuildRuntimeItem(MetadataOnlyDTO, ItemTemplateData, MetadataOnlyRuntimeItem, ConversionError));
	TestEqual(TEXT("Metadata random option is reconstructed"), MetadataOnlyRuntimeItem.RuntimeGeneratedStats.Num(), 1);
	TestEqual(TEXT("Metadata generated skill is reconstructed"), MetadataOnlyRuntimeItem.RuntimeGeneratedSkills.Num(), 1);

	FFrontierItemPersistenceDTO CompactBackendDTO = DTO;
	CompactBackendDTO.FinalRarityTag = TEXT("COMMON");
	CompactBackendDTO.InstanceTags.Reset();
	CompactBackendDTO.InstanceTags.Add(TEXT("raid-found"));
	CompactBackendDTO.InstanceTags.Add(TEXT("insured"));
	CompactBackendDTO.RandomOptions.Reset();
	FFrontierItemOptionDTO& CompactOptionDTO = CompactBackendDTO.RandomOptions.AddDefaulted_GetRef();
	CompactOptionDTO.OptionId = TEXT("crit_chance");
	CompactOptionDTO.FinalValue = 12.0;
	CompactOptionDTO.RandomValue = 12.0;
	CompactBackendDTO.GeneratedSkills.Reset();
	FFrontierGeneratedItemSkillDTO& CompactSkillDTO = CompactBackendDTO.GeneratedSkills.AddDefaulted_GetRef();
	CompactSkillDTO.SkillId = TEXT("skill.bleed_shot");
	CompactSkillDTO.Level = 2;
	CompactSkillDTO.SlotIndex = 0;

	FFrontierItemInstance CompactBackendRuntimeItem;
	TestTrue(
		TEXT("Unknown backend extension tags/options do not reject the whole item"),
		FFrontierItemPersistenceMapper::TryBuildRuntimeItem(CompactBackendDTO, ItemTemplateData, CompactBackendRuntimeItem, ConversionError));
	TestEqual(TEXT("Backend instance tags are preserved as strings"), CompactBackendRuntimeItem.InstanceTags.Num(), 2);
	TestEqual(TEXT("String option IDs are preserved without a GameplayTag"), CompactBackendRuntimeItem.RuntimeGeneratedStats.Num(), 1);
	TestEqual(TEXT("SkillId-only generated skill is preserved for DataTable lookup"), CompactBackendRuntimeItem.RuntimeGeneratedSkills.Num(), 1);
	if (CompactBackendRuntimeItem.RuntimeGeneratedSkills.Num() == 1)
	{
		TestEqual(TEXT("SkillId-only generated skill keeps SkillId"), CompactBackendRuntimeItem.RuntimeGeneratedSkills[0].SkillTemplateId, FString(TEXT("skill.bleed_shot")));
		TestFalse(TEXT("SkillId-only generated skill has no resolved SkillTag yet"), CompactBackendRuntimeItem.RuntimeGeneratedSkills[0].SkillTag.IsValid());
	}
	TestEqual(TEXT("Compact backend item still preserves quantity"), CompactBackendRuntimeItem.Quantity, 1);

	// Existing server envelope fields must survive a runtime state update.
	RuntimeItem.Quantity = 2;
	TestTrue(
		TEXT("Runtime persistent state updates an existing DTO"),
		FFrontierItemPersistenceMapper::TryUpdateDTOFromRuntimeItem(RuntimeItem, DTO, ConversionError));
	TestEqual(TEXT("Runtime quantity updates"), DTO.Quantity, 2);
	TestEqual(TEXT("Runtime rarity persists as backend enum string"), DTO.FinalRarityTag, FString(TEXT("RARE")));
	TestEqual(TEXT("Server final stat value remains preserved"), DTO.RandomOptions[0].FinalValue, 16.0);
	TestTrue(TEXT("Existing metadata survives runtime conversion"), DTO.MetadataJson.Contains(TEXT("\"event\"")));
	TestFalse(TEXT("Runtime stats are not mirrored into metadata"), DTO.MetadataJson.Contains(TEXT("\"randomOptions\"")));
	TestFalse(TEXT("Runtime skills are not mirrored into metadata"), DTO.MetadataJson.Contains(TEXT("\"generatedSkills\"")));

	UFrontierItemDataAsset* StackableItemData = NewObject<UFrontierItemDataAsset>();
	StackableItemData->ItemId = TEXT("Item.Material.PersistenceStack");
	StackableItemData->ItemType = EFrontierItemType::Material;
	StackableItemData->bStackable = true;
	FFrontierItemInstance StackableItem;
	StackableItem.ItemTemplateData = UFrontierItemCatalogSubsystem::BuildTemplateDataFromItemData(*StackableItemData);
	StackableItem.Quantity = 5;
	StackableItem.ItemInstanceId = FGuid::NewGuid();
	const FGuid ServerAssignedStackId = StackableItem.ItemInstanceId;
	StackableItem.EnsureRuntimeIdentity();
	TestEqual(TEXT("A server-assigned stack ID is never discarded"), StackableItem.ItemInstanceId, ServerAssignedStackId);

	return true;
}

#endif
