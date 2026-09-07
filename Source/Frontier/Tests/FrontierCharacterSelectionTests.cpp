#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Character/FrontierCharacterAppearanceDataAsset.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Components/FrontierEquipmentComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "UObject/UnrealType.h"
#include "Save/FrontierCharacterSaveGame.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierCharacterAppearanceMappingTest,
	"Frontier.Character.Appearance.Mapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierCharacterAppearanceMappingTest::RunTest(const FString& Parameters)
{
	UFrontierCharacterAppearanceDataAsset* AppearanceData =
		NewObject<UFrontierCharacterAppearanceDataAsset>();
	TestNotNull(TEXT("Transient appearance DataAsset exists"), AppearanceData);
	if (!AppearanceData)
	{
		return false;
	}

	FFrontierCharacterAppearanceData& DarkKnight =
		AppearanceData->CharacterAppearances.AddDefaulted_GetRef();
	DarkKnight.CharacterType = EFrontierCharacterType::DarkKnight;

	FFrontierCharacterAppearanceData& DarkLady =
		AppearanceData->CharacterAppearances.AddDefaulted_GetRef();
	DarkLady.CharacterType = EFrontierCharacterType::DarkLady;

	TestTrue(
		TEXT("DarkLady resolves its own appearance"),
		AppearanceData->FindAppearance(EFrontierCharacterType::DarkLady) == &DarkLady);
	TestTrue(
		TEXT("An invalid type falls back to DarkKnight"),
		AppearanceData->FindAppearance(static_cast<EFrontierCharacterType>(255)) == &DarkKnight);

	const UFrontierCharacterSaveGame* SaveGame =
		NewObject<UFrontierCharacterSaveGame>();
	TestEqual(
		TEXT("New saves default to DarkKnight"),
		SaveGame->SelectedCharacterType,
		EFrontierCharacterType::DarkKnight);

	FFrontierItemInstance ArmorItem;
	ArmorItem.ItemTemplateData.ItemTemplateId = TEXT("armor_character_mapping_test");
	ArmorItem.ItemTemplateData.Common.Category = EFrontierItemCategory::Armor;

	UStaticMesh* KnightArmor = NewObject<UStaticMesh>();
	USkeletalMesh* LadyArmor = NewObject<USkeletalMesh>();
	FFrontierArmorAppearanceData& KnightAppearance =
		ArmorItem.ItemTemplateData.ArmorAppearances.AddDefaulted_GetRef();
	KnightAppearance.CharacterType = EFrontierCharacterType::DarkKnight;
	KnightAppearance.EquipmentStaticMesh = KnightArmor;
	FFrontierArmorAppearanceData& LadyAppearance =
		ArmorItem.ItemTemplateData.ArmorAppearances.AddDefaulted_GetRef();
	LadyAppearance.CharacterType = EFrontierCharacterType::DarkLady;
	LadyAppearance.EquipmentSkeletalMesh = LadyArmor;

	FFrontierArmorAppearanceData ResolvedArmor;
	TestTrue(
		TEXT("DarkKnight resolves the static armor mapping"),
		ArmorItem.ResolveArmorAppearance(EFrontierCharacterType::DarkKnight, ResolvedArmor));
	TestTrue(
		TEXT("DarkKnight mapping uses its static mesh"),
		ResolvedArmor.EquipmentStaticMesh.Get() == KnightArmor);
	TestTrue(
		TEXT("DarkLady resolves the skeletal armor mapping"),
		ArmorItem.ResolveArmorAppearance(EFrontierCharacterType::DarkLady, ResolvedArmor));
	TestTrue(
		TEXT("DarkLady mapping uses its skeletal mesh"),
		ResolvedArmor.EquipmentSkeletalMesh.Get() == LadyArmor);

	const FProperty* CharacterTypeProperty =
		FindFProperty<FProperty>(
			AFrontierPlayerCharacter::StaticClass(),
			TEXT("SelectedCharacterType"));
	TestTrue(
		TEXT("SelectedCharacterType is registered for replication"),
		CharacterTypeProperty
			&& CharacterTypeProperty->HasAnyPropertyFlags(CPF_Net));

	const UFunction* CharacterTypeRpc =
		AFrontierPlayerCharacter::StaticClass()->FindFunctionByName(
			TEXT("ServerSetCharacterType"));
	TestTrue(
		TEXT("Character type request is a reliable server RPC"),
		CharacterTypeRpc
			&& CharacterTypeRpc->HasAllFunctionFlags(
				FUNC_Net | FUNC_NetServer | FUNC_NetReliable));

	const FProperty* ArmorVisualsProperty =
		FindFProperty<FProperty>(
			UFrontierEquipmentComponent::StaticClass(),
			TEXT("ReplicatedArmorVisuals"));
	TestTrue(
		TEXT("Public armor visuals are registered for replication"),
		ArmorVisualsProperty
			&& ArmorVisualsProperty->HasAnyPropertyFlags(CPF_Net));
	return true;
}

#endif
