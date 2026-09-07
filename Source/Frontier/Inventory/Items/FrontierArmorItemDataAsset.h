#pragma once

#include "CoreMinimal.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "FrontierArmorItemDataAsset.generated.h"

UCLASS(BlueprintType)
class FRONTIER_API UFrontierArmorItemDataAsset : public UFrontierItemDataAsset
{
	GENERATED_BODY()

public:
	UFrontierArmorItemDataAsset()
	{
		ItemType = EFrontierItemType::Armor;
		Category = EFrontierItemCategory::Armor;
		ElementalType = EFrontierElementalType::None;
		MaxStackCount = 1;
		MaxStack = 1;
		bStackable = false;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Armor")
	EFrontierEquipmentSlot EquipSlot = EFrontierEquipmentSlot::Chest;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment")
	FFrontierEquipmentTemplateData EquipmentData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Armor")
	TArray<FFrontierItemStatModifier> Resistances;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Armor|Appearance")
	TArray<FFrontierArmorAppearanceData> CharacterAppearances;
};
