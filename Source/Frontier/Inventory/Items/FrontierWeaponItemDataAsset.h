#pragma once

#include "CoreMinimal.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "FrontierWeaponItemDataAsset.generated.h"

class UFrontierWeaponDataAsset;
class UFrontierWeaponSkillGenerationDataAsset;
class AFrontierWeaponBase;

UCLASS(BlueprintType)
class FRONTIER_API UFrontierWeaponItemDataAsset : public UFrontierItemDataAsset
{
	GENERATED_BODY()

public:
	UFrontierWeaponItemDataAsset()
	{
		ItemType = EFrontierItemType::Weapon;
		Category = EFrontierItemCategory::Weapon;
		ElementalType = EFrontierElementalType::Normal;
		MaxStackCount = 1;
		MaxStack = 1;
		bStackable = false;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	TSoftObjectPtr<UFrontierWeaponDataAsset> WeaponData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	TSoftClassPtr<AFrontierWeaponBase> WeaponActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	EFrontierEquipmentSlot EquipSlot = EFrontierEquipmentSlot::MainWeapon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment")
	FFrontierEquipmentTemplateData EquipmentData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	FGameplayTag WeaponTypeTag;
};
