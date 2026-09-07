#include "Inventory/FrontierQuickSlotTypes.h"

bool FFrontierQuickSlotEntry::MatchesItem(const FFrontierItemInstance& ItemInstance) const
{
	if (!bOccupied || !ItemInstance.IsValid())
	{
		return false;
	}

	if (BindingType == EFrontierQuickSlotBindingType::ConsumableTemplate)
	{
		return ItemInstance.GetCategory() == EFrontierItemCategory::Consumable
			&& ItemInstance.GetTemplateId() == ItemTemplateId;
	}

	if (BindingType != EFrontierQuickSlotBindingType::WeaponInstance
		|| ItemInstance.GetCategory() != EFrontierItemCategory::Weapon)
	{
		return false;
	}

	if (ItemInstanceId.IsValid() && ItemInstance.ItemInstanceId == ItemInstanceId)
	{
		return true;
	}

	return RaidItemId.IsValid() && ItemInstance.RaidItemId == RaidItemId;
}

void FFrontierQuickSlotEntry::Reset(const int32 InSlotIndex)
{
	SlotIndex = InSlotIndex;
	bOccupied = false;
	BindingType = EFrontierQuickSlotBindingType::None;
	ItemInstanceId.Invalidate();
	RaidItemId.Invalidate();
	ItemTemplateId = NAME_None;
	EquipSlot = EFrontierEquipmentSlot::None;
}

