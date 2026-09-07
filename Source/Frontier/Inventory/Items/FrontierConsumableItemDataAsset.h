#pragma once

#include "CoreMinimal.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "FrontierConsumableItemDataAsset.generated.h"

class UGameplayEffect;
class UAnimMontage;
class UStaticMesh;

UCLASS(BlueprintType)
class FRONTIER_API UFrontierConsumableItemDataAsset : public UFrontierItemDataAsset
{
	GENERATED_BODY()

public:
	UFrontierConsumableItemDataAsset()
	{
		ItemType = EFrontierItemType::Consumable;
		Category = EFrontierItemCategory::Consumable;
		ElementalType = EFrontierElementalType::None;
		MaxStackCount = 99;
		MaxStack = 99;
		bStackable = true;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable")
	TArray<EFrontierEquipmentSlot> AllowedEquipSlots;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable")
	TSoftClassPtr<UGameplayEffect> ConsumeEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable", meta=(ClampMin="0.0"))
	float HealthRestoreAmount = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable", meta=(ClampMin="0.0"))
	float StaminaRestoreAmount = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable", meta=(ClampMin="0.0"))
	float UseTime = 0.0f;

	/** Optional montage played when this consumable is used. Recovery is applied by the potion notify in this montage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable|Use Animation")
	TSoftObjectPtr<UAnimMontage> UseMontage;

	/** Character skeletal-mesh socket where the held potion is shown during UseMontage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable|Use Animation")
	FName PotionSocketName = NAME_None;

	/** Static mesh shown in PotionSocketName during UseMontage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable|Use Animation")
	TSoftObjectPtr<UStaticMesh> PotionMesh;

	/** Optional local adjustment for the potion mesh after it is attached to PotionSocketName. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable|Use Animation")
	FTransform PotionMeshRelativeTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable")
	FGameplayTag CooldownGroupTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable")
	bool bUsableInRaid = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable")
	bool bUsableInLobby = false;
};
