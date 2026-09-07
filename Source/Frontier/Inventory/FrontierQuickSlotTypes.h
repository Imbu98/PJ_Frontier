#pragma once

#include "CoreMinimal.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "FrontierQuickSlotTypes.generated.h"

UENUM(BlueprintType)
enum class EFrontierQuickSlotBindingType : uint8
{
	None,
	WeaponInstance,
	ConsumableTemplate
};

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierQuickSlotEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quick Slot")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quick Slot")
	bool bOccupied = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quick Slot")
	EFrontierQuickSlotBindingType BindingType = EFrontierQuickSlotBindingType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quick Slot")
	FGuid ItemInstanceId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quick Slot")
	FGuid RaidItemId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quick Slot")
	FName ItemTemplateId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quick Slot")
	EFrontierEquipmentSlot EquipSlot = EFrontierEquipmentSlot::None;

	bool MatchesItem(const FFrontierItemInstance& ItemInstance) const;
	void Reset(int32 InSlotIndex = INDEX_NONE);
};

