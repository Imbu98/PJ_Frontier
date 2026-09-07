#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Skill/FrontierSkillTypes.h"
#include "FrontierItemDataAsset.generated.h"

class UTexture2D;
class UStaticMesh;
class USkeletalMesh;
class UDataTable;

UCLASS(BlueprintType)
class FRONTIER_API UFrontierItemDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Item")
	FName GetTemplateId() const
	{
		return ItemId;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(DisplayName="TemplateId", ToolTip="Shared template ID for this item type. Use ItemInstanceId on FFrontierItemInstance for per-drop unique IDs."))
	FName ItemId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(MultiLine))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(DeprecatedProperty, DeprecationMessage="Use Category."))
	EFrontierItemType ItemType = EFrontierItemType::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	EFrontierItemCategory Category = EFrontierItemCategory::Material;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	FName SubCategory;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	EFrontierElementalType ElementalType = EFrontierElementalType::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(ClampMin="1", DeprecatedProperty, DeprecationMessage="Use MaxStack."))
	int32 MaxStackCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(ClampMin="1"))
	int32 MaxStack = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	bool bStackable = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(ClampMin="0"))
	int32 MaxDurability = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	EFrontierItemBindState BindState = EFrontierItemBindState::Unbound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	bool bTradable = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(ClampMin="0", DeprecatedProperty, DeprecationMessage="Use SellPriceGold."))
	int32 SellPrice = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(ClampMin="0"))
	int32 SellPriceGold = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(ClampMin="0.0", DeprecatedProperty, DeprecationMessage="Use ItemWeight."))
	float Weight = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(ClampMin="0.0"))
	float ItemWeight = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	TSoftObjectPtr<UStaticMesh> WorldMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	TSoftObjectPtr<USkeletalMesh> EquipmentMesh;

};
