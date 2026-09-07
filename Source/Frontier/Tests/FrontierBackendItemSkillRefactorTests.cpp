#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Inventory/FrontierInventoryTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierItemTemplateElementValidationTest,
	"Frontier.Item.Template.ElementValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierItemTemplateElementValidationTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Common item template is not a DataTable row"),
		FFrontierItemTemplateData::StaticStruct()->IsChildOf(FTableRowBase::StaticStruct()));
	TestTrue(TEXT("Weapon template is a DataTable row"),
		FFrontierWeaponTemplateData::StaticStruct()->IsChildOf(FTableRowBase::StaticStruct()));
	TestTrue(TEXT("Armor template is a DataTable row"),
		FFrontierArmorTemplateData::StaticStruct()->IsChildOf(FTableRowBase::StaticStruct()));
	TestTrue(TEXT("Consumable template is a DataTable row"),
		FFrontierConsumableTemplateData::StaticStruct()->IsChildOf(FTableRowBase::StaticStruct()));
	TestTrue(TEXT("Material template is a DataTable row"),
		FFrontierMaterialTemplateData::StaticStruct()->IsChildOf(FTableRowBase::StaticStruct()));
	TestTrue(TEXT("Accessory template is a DataTable row"),
		FFrontierAccessoryTemplateData::StaticStruct()->IsChildOf(FTableRowBase::StaticStruct()));

	TestTrue(TEXT("Weapon + Normal is valid"), IsValidElementForCategory(EFrontierItemCategory::Weapon, EFrontierElementalType::Normal));
	TestTrue(TEXT("Weapon + Fire is valid"), IsValidElementForCategory(EFrontierItemCategory::Weapon, EFrontierElementalType::Fire));
	TestFalse(TEXT("Weapon + None is invalid"), IsValidElementForCategory(EFrontierItemCategory::Weapon, EFrontierElementalType::None));
	TestTrue(TEXT("Armor + None is valid"), IsValidElementForCategory(EFrontierItemCategory::Armor, EFrontierElementalType::None));
	TestTrue(TEXT("Armor + Normal is valid"), IsValidElementForCategory(EFrontierItemCategory::Armor, EFrontierElementalType::Normal));
	TestFalse(TEXT("Armor + Fire is invalid"), IsValidElementForCategory(EFrontierItemCategory::Armor, EFrontierElementalType::Fire));
	TestTrue(TEXT("Consumable + None is valid"), IsValidElementForCategory(EFrontierItemCategory::Consumable, EFrontierElementalType::None));
	TestTrue(TEXT("Accessory + None is valid"), IsValidElementForCategory(EFrontierItemCategory::Accessory, EFrontierElementalType::None));
	TestTrue(TEXT("Accessory + Normal is valid"), IsValidElementForCategory(EFrontierItemCategory::Accessory, EFrontierElementalType::Normal));
	TestFalse(TEXT("Accessory + Fire is invalid"), IsValidElementForCategory(EFrontierItemCategory::Accessory, EFrontierElementalType::Fire));

	EFrontierItemCategory ParsedCategory = EFrontierItemCategory::Material;
	TestTrue(TEXT("ACCESSORY backend category parses"), TryParseItemCategory(TEXT("ACCESSORY"), ParsedCategory));
	TestEqual(TEXT("Parsed backend category is Accessory"), ParsedCategory, EFrontierItemCategory::Accessory);
	return true;
}

#endif
