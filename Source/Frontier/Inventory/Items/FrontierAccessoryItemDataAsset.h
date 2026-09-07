#pragma once

#include "CoreMinimal.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "FrontierAccessoryItemDataAsset.generated.h"

/** Optional local/template asset representation for an equippable accessory. */
UCLASS(BlueprintType)
class FRONTIER_API UFrontierAccessoryItemDataAsset : public UFrontierItemDataAsset
{
	GENERATED_BODY()

public:
	UFrontierAccessoryItemDataAsset()
	{
		ItemType = EFrontierItemType::Accessory;
		Category = EFrontierItemCategory::Accessory;
		ElementalType = EFrontierElementalType::None;
		MaxStackCount = 1;
		MaxStack = 1;
		bStackable = false;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Accessory")
	EFrontierEquipmentSlot EquipSlot = EFrontierEquipmentSlot::Necklace;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment")
	FFrontierEquipmentTemplateData EquipmentData;
};
