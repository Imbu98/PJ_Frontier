#pragma once

#include "CoreMinimal.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "FrontierMaterialItemDataAsset.generated.h"

UCLASS(BlueprintType)
class FRONTIER_API UFrontierMaterialItemDataAsset : public UFrontierItemDataAsset
{
	GENERATED_BODY()

public:
	UFrontierMaterialItemDataAsset()
	{
		ItemType = EFrontierItemType::Material;
		Category = EFrontierItemCategory::Material;
		ElementalType = EFrontierElementalType::None;
		MaxStackCount = 999;
		MaxStack = 999;
		bStackable = true;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Material")
	FGameplayTag MaterialTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Material", meta=(ClampMin="0"))
	int32 CraftingValue = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Material", meta=(ClampMin="0"))
	int32 UpgradeValue = 0;
};
