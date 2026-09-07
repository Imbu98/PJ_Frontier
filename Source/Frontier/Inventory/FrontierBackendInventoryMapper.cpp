#include "Inventory/FrontierBackendInventoryMapper.h"

#include "Frontier.h"
#include "Inventory/Items/FrontierItemCatalogSubsystem.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "Inventory/Persistence/FrontierItemPersistenceTypes.h"

namespace
{
bool ConvertBackendBindState(const FString& BackendBindState, EFrontierItemBindState& OutBindState, FString& OutError)
{
	const FString Normalized = BackendBindState.ToUpper();
	if (Normalized == TEXT("NONE") || Normalized == TEXT("UNBOUND"))
	{
		OutBindState = EFrontierItemBindState::Unbound;
		return true;
	}
	if (Normalized == TEXT("ACCOUNT_BOUND"))
	{
		OutBindState = EFrontierItemBindState::AccountBound;
		return true;
	}
	if (Normalized == TEXT("CHARACTER_BOUND"))
	{
		OutBindState = EFrontierItemBindState::CharacterBound;
		return true;
	}

	OutError = FString::Printf(TEXT("Unknown backend bindState: %s"), *BackendBindState);
	return false;
}

bool ConvertBackendItemDTOToRuntime(
	const FFrontierOnlineItemDTO& BackendItem,
	int32 Quantity,
	const FString& CreatedAt,
	const TCHAR* ErrorPrefix,
	const UFrontierItemCatalogSubsystem& ItemCatalog,
	FFrontierItemInstance& OutRuntimeItem,
	FString& OutError)
{
	OutRuntimeItem = FFrontierItemInstance();
	OutError.Reset();

	FGuid ParsedItemInstanceId;
	if (!FGuid::Parse(BackendItem.ItemInstanceId, ParsedItemInstanceId) || !ParsedItemInstanceId.IsValid())
	{
		OutError = FString::Printf(TEXT("%s itemInstanceId is not a valid UUID."), ErrorPrefix);
		return false;
	}
	if (BackendItem.ItemTemplateId.IsEmpty())
	{
		OutError = FString::Printf(TEXT("%s itemTemplateId is empty."), ErrorPrefix);
		return false;
	}
	if (Quantity <= 0)
	{
		OutError = FString::Printf(TEXT("%s quantity must be greater than zero."), ErrorPrefix);
		return false;
	}

	const FName ItemTemplateId(*BackendItem.ItemTemplateId);
	const FFrontierResolvedItemTemplateData* ItemTemplateData = ItemCatalog.ResolveItemTemplateData(ItemTemplateId);
	if (!ItemTemplateData)
	{
		OutError = FString::Printf(TEXT("No ItemTemplateData found for %s itemTemplateId '%s'."), ErrorPrefix, *BackendItem.ItemTemplateId);
		return false;
	}

	EFrontierItemRarity ParsedRarity = EFrontierItemRarity::Common;
	if (!TryParseItemRarity(BackendItem.FinalRarityTag, ParsedRarity))
	{
		OutError = FString::Printf(TEXT("%s rarity/finalRarityTag is unknown: %s"), ErrorPrefix, *BackendItem.FinalRarityTag);
		return false;
	}

	EFrontierItemBindState BindState = EFrontierItemBindState::Unbound;
	if (!ConvertBackendBindState(BackendItem.BindState, BindState, OutError))
	{
		return false;
	}

	FFrontierItemPersistenceDTO PersistenceDTO;
	PersistenceDTO.ItemInstanceId = BackendItem.ItemInstanceId;
	PersistenceDTO.ItemTemplateId = BackendItem.ItemTemplateId;
	PersistenceDTO.Quantity = Quantity;
	if (BackendItem.bHasDurability)
	{
		PersistenceDTO.Durability = BackendItem.Durability;
	}
	PersistenceDTO.EnhancementLevel = BackendItem.EnhancementLevel;
	PersistenceDTO.FinalRarityTag = ConvertItemRarityToBackendString(ParsedRarity);
	PersistenceDTO.BindState = BindState;
	PersistenceDTO.InstanceTags = BackendItem.InstanceTags;
	PersistenceDTO.RandomOptions.Reset(BackendItem.RandomOptions.Num());
	for (const FFrontierOnlineItemOption& BackendOption : BackendItem.RandomOptions)
	{
		FFrontierItemOptionDTO& OptionDTO = PersistenceDTO.RandomOptions.AddDefaulted_GetRef();
		OptionDTO.OptionId = BackendOption.OptionId;
		OptionDTO.Unit = BackendOption.Unit;
		OptionDTO.BaseValue = BackendOption.BaseValue;
		OptionDTO.RandomValue = BackendOption.RandomValue;
		OptionDTO.UpgradeValue = BackendOption.UpgradeValue;
		OptionDTO.FinalValue = BackendOption.FinalValue;
		OptionDTO.NormalizedValue = BackendOption.NormalizedValue;
		OptionDTO.bHasNormalizedValue = BackendOption.bHasNormalizedValue;
	}
	PersistenceDTO.GeneratedSkills.Reset(BackendItem.GeneratedSkills.Num());
	for (const FFrontierOnlineGeneratedItemSkill& BackendSkill : BackendItem.GeneratedSkills)
	{
		FFrontierGeneratedItemSkillDTO& SkillDTO = PersistenceDTO.GeneratedSkills.AddDefaulted_GetRef();
		SkillDTO.SkillId = BackendSkill.SkillId;
		SkillDTO.Level = BackendSkill.Level;
		SkillDTO.SlotIndex = BackendSkill.SlotIndex;
	}
	PersistenceDTO.CreatedAt = CreatedAt;
	PersistenceDTO.AcquiredAt = BackendItem.AcquiredAt;
	PersistenceDTO.UpdatedAt = BackendItem.UpdatedAt;
	PersistenceDTO.MetadataJson = BackendItem.bHasMetadata ? BackendItem.MetadataJson : TEXT("{}");

	if (!FFrontierItemPersistenceMapper::TryBuildRuntimeItem(
		PersistenceDTO,
		*ItemTemplateData,
		OutRuntimeItem,
		OutError))
	{
		return false;
	}

	return true;
}
}

bool FFrontierBackendInventoryMapper::ConvertBackendEquipmentSlot(
	const FString& BackendSlot,
	EFrontierEquipmentSlot& OutSlot,
	FString& OutError)
{
	OutSlot = EFrontierEquipmentSlot::None;
	OutError.Reset();

	const FString Normalized = BackendSlot.ToUpper();
	if (Normalized == TEXT("MAIN_WEAPON") || Normalized == TEXT("PRIMARY_WEAPON"))
	{
		OutSlot = EFrontierEquipmentSlot::MainWeapon;
		return true;
	}
	if (Normalized == TEXT("SUB_WEAPON"))
	{
		OutSlot = EFrontierEquipmentSlot::SubWeapon;
		return true;
	}
	if (Normalized == TEXT("HELMET"))
	{
		OutSlot = EFrontierEquipmentSlot::Helmet;
		return true;
	}
	if (Normalized == TEXT("CHEST"))
	{
		OutSlot = EFrontierEquipmentSlot::Chest;
		return true;
	}
	if (Normalized == TEXT("GLOVES"))
	{
		OutSlot = EFrontierEquipmentSlot::Gloves;
		return true;
	}
	if (Normalized == TEXT("BOOTS"))
	{
		OutSlot = EFrontierEquipmentSlot::Boots;
		return true;
	}
	if (Normalized == TEXT("NECKLACE"))
	{
		OutSlot = EFrontierEquipmentSlot::Necklace;
		return true;
	}
	if (Normalized == TEXT("RING"))
	{
		OutSlot = EFrontierEquipmentSlot::Ring;
		return true;
	}

	OutError = FString::Printf(TEXT("Unknown backend equipment slot: %s"), *BackendSlot);
	return false;
}

bool FFrontierBackendInventoryMapper::ConvertEquipmentSlotToBackend(
	const EFrontierEquipmentSlot Slot,
	FString& OutBackendSlot,
	FString& OutError)
{
	OutBackendSlot.Reset();
	OutError.Reset();

	switch (Slot)
	{
	case EFrontierEquipmentSlot::MainWeapon:
		OutBackendSlot = TEXT("MAIN_WEAPON");
		return true;
	case EFrontierEquipmentSlot::SubWeapon:
		OutBackendSlot = TEXT("SUB_WEAPON");
		return true;
	case EFrontierEquipmentSlot::Helmet:
		OutBackendSlot = TEXT("HELMET");
		return true;
	case EFrontierEquipmentSlot::Chest:
		OutBackendSlot = TEXT("CHEST");
		return true;
	case EFrontierEquipmentSlot::Gloves:
		OutBackendSlot = TEXT("GLOVES");
		return true;
	case EFrontierEquipmentSlot::Boots:
		OutBackendSlot = TEXT("BOOTS");
		return true;
	case EFrontierEquipmentSlot::Necklace:
		OutBackendSlot = TEXT("NECKLACE");
		return true;
	case EFrontierEquipmentSlot::Ring:
		OutBackendSlot = TEXT("RING");
		return true;
	default:
		break;
	}

	OutError = FString::Printf(TEXT("Cannot convert equipment slot to backend value: %d"), static_cast<int32>(Slot));
	return false;
}

bool FFrontierBackendInventoryMapper::ConvertBackendInventoryItemToRuntime(
	const FFrontierOnlineItemDTO& BackendItem,
	const UFrontierItemCatalogSubsystem& ItemCatalog,
	FFrontierItemInstance& OutRuntimeItem,
	FString& OutError)
{
	return ConvertBackendItemDTOToRuntime(
		BackendItem,
		BackendItem.Quantity,
		BackendItem.CreatedAt,
		TEXT("inventory"),
		ItemCatalog,
		OutRuntimeItem,
		OutError);
}

bool FFrontierBackendInventoryMapper::ConvertBackendEquipmentItemToRuntime(
	const FFrontierOnlineItemDTO& BackendItem,
	const UFrontierItemCatalogSubsystem& ItemCatalog,
	FFrontierItemInstance& OutRuntimeItem,
	FString& OutError)
{
	return ConvertBackendItemDTOToRuntime(
		BackendItem,
		1,
		FString(),
		TEXT("equipment"),
		ItemCatalog,
		OutRuntimeItem,
		OutError);
}

bool FFrontierBackendInventoryMapper::BuildRuntimeSlotsFromBackendInventory(
	const FFrontierOnlineInventoryData& BackendInventory,
	const UFrontierItemCatalogSubsystem& ItemCatalog,
	TArray<FFrontierInventorySlot>& OutSlots,
	FString& OutError)
{
	OutSlots.Reset();
	OutError.Reset();

	const int32 SlotCapacity = BackendInventory.Container.SlotCapacity;
	if (SlotCapacity <= 0)
	{
		OutError = TEXT("Backend inventory slotCapacity must be greater than zero.");
		return false;
	}
	OutSlots.SetNum(SlotCapacity);
	for (int32 SlotIndex = 0; SlotIndex < SlotCapacity; ++SlotIndex)
	{
		OutSlots[SlotIndex].SlotIndex = SlotIndex;
		OutSlots[SlotIndex].bOccupied = false;
	}

	TSet<int32> SeenSlotIndices;
	for (const FFrontierOnlineItemDTO& BackendItem : BackendInventory.Slots)
	{
		if (BackendItem.SlotIndex < 0 || BackendItem.SlotIndex >= SlotCapacity)
		{
			OutError = FString::Printf(TEXT("Backend item slotIndex is outside slotCapacity. SlotIndex=%d Capacity=%d"),
				BackendItem.SlotIndex,
				SlotCapacity);
			return false;
		}
		if (SeenSlotIndices.Contains(BackendItem.SlotIndex))
		{
			OutError = FString::Printf(TEXT("Backend inventory contains duplicate slotIndex %d."), BackendItem.SlotIndex);
			return false;
		}
		SeenSlotIndices.Add(BackendItem.SlotIndex);

		FFrontierItemInstance RuntimeItem;
		if (!ConvertBackendInventoryItemToRuntime(BackendItem, ItemCatalog, RuntimeItem, OutError))
		{
			FRONTIER_LOG(Warning, TEXT("Backend item conversion failed. ItemInstanceId=%s ItemTemplateId=%s SlotIndex=%d Error=%s"),
				*BackendItem.ItemInstanceId,
				*BackendItem.ItemTemplateId,
				BackendItem.SlotIndex,
				*OutError);
			return false;
		}

		FFrontierInventorySlot& RuntimeSlot = OutSlots[BackendItem.SlotIndex];
		RuntimeSlot.SlotIndex = BackendItem.SlotIndex;
		RuntimeSlot.bOccupied = true;
		RuntimeSlot.ItemInstance = MoveTemp(RuntimeItem);
	}

	return true;
}

bool FFrontierBackendInventoryMapper::BuildRuntimeSlotsFromBackendStorage(
	const FFrontierOnlineStorageData& BackendStorage,
	const UFrontierItemCatalogSubsystem& ItemCatalog,
	TArray<FFrontierInventorySlot>& OutSlots,
	FString& OutError)
{
	OutSlots.Reset();
	OutError.Reset();

	const int32 SlotCapacity = BackendStorage.Container.SlotCapacity;
	if (SlotCapacity <= 0)
	{
		OutError = TEXT("Backend storage slotCapacity must be greater than zero.");
		return false;
	}
	OutSlots.SetNum(SlotCapacity);
	for (int32 SlotIndex = 0; SlotIndex < SlotCapacity; ++SlotIndex)
	{
		OutSlots[SlotIndex].SlotIndex = SlotIndex;
		OutSlots[SlotIndex].bOccupied = false;
	}

	TSet<int32> SeenSlotIndices;
	for (const FFrontierOnlineItemDTO& BackendItem : BackendStorage.Slots)
	{
		if (BackendItem.SlotIndex < 0 || BackendItem.SlotIndex >= SlotCapacity)
		{
			OutError = FString::Printf(TEXT("Backend storage item slotIndex is outside slotCapacity. SlotIndex=%d Capacity=%d"),
				BackendItem.SlotIndex,
				SlotCapacity);
			return false;
		}
		if (SeenSlotIndices.Contains(BackendItem.SlotIndex))
		{
			OutError = FString::Printf(TEXT("Backend storage contains duplicate slotIndex %d."), BackendItem.SlotIndex);
			return false;
		}
		SeenSlotIndices.Add(BackendItem.SlotIndex);

		FFrontierItemInstance RuntimeItem;
		if (!ConvertBackendItemDTOToRuntime(
			BackendItem,
			BackendItem.Quantity,
			BackendItem.CreatedAt,
			TEXT("storage"),
			ItemCatalog,
			RuntimeItem,
			OutError))
		{
			FRONTIER_LOG(Warning, TEXT("Backend storage item conversion failed. ItemInstanceId=%s ItemTemplateId=%s SlotIndex=%d Error=%s"),
				*BackendItem.ItemInstanceId,
				*BackendItem.ItemTemplateId,
				BackendItem.SlotIndex,
				*OutError);
			return false;
		}

		FFrontierInventorySlot& RuntimeSlot = OutSlots[BackendItem.SlotIndex];
		RuntimeSlot.SlotIndex = BackendItem.SlotIndex;
		RuntimeSlot.bOccupied = true;
		RuntimeSlot.ItemInstance = MoveTemp(RuntimeItem);
	}

	return true;
}

bool FFrontierBackendInventoryMapper::BuildRuntimeLoadoutFromBackendEquipment(
	const FFrontierOnlineEquipmentData& BackendEquipment,
	const UFrontierItemCatalogSubsystem& ItemCatalog,
	TArray<FFrontierLoadoutSlot>& OutSlots,
	FString& OutError)
{
	OutSlots.Reset();
	OutError.Reset();

	const EFrontierEquipmentSlot DefaultSlots[] = {
		EFrontierEquipmentSlot::MainWeapon,
		EFrontierEquipmentSlot::SubWeapon,
		EFrontierEquipmentSlot::Helmet,
		EFrontierEquipmentSlot::Chest,
		EFrontierEquipmentSlot::Gloves,
		EFrontierEquipmentSlot::Boots,
		EFrontierEquipmentSlot::Necklace,
		EFrontierEquipmentSlot::Ring
	};

	OutSlots.Reserve(UE_ARRAY_COUNT(DefaultSlots));
	for (const EFrontierEquipmentSlot SlotType : DefaultSlots)
	{
		FFrontierLoadoutSlot& RuntimeSlot = OutSlots.AddDefaulted_GetRef();
		RuntimeSlot.SlotType = SlotType;
		RuntimeSlot.bOccupied = false;
	}

	TSet<int32> SeenSlots;
	for (const FFrontierOnlineEquipmentSlot& BackendSlot : BackendEquipment.Slots)
	{
		EFrontierEquipmentSlot RuntimeSlotType = EFrontierEquipmentSlot::None;
		if (!ConvertBackendEquipmentSlot(BackendSlot.SlotType, RuntimeSlotType, OutError))
		{
			return false;
		}
		const int32 RuntimeSlotKey = static_cast<int32>(RuntimeSlotType);
		if (RuntimeSlotType == EFrontierEquipmentSlot::None || SeenSlots.Contains(RuntimeSlotKey))
		{
			OutError = FString::Printf(TEXT("Backend equipment contains duplicate or invalid slotType: %s"), *BackendSlot.SlotType);
			return false;
		}
		SeenSlots.Add(RuntimeSlotKey);

		FFrontierLoadoutSlot* RuntimeSlot = OutSlots.FindByPredicate([RuntimeSlotType](const FFrontierLoadoutSlot& Candidate)
		{
			return Candidate.SlotType == RuntimeSlotType;
		});
		if (!RuntimeSlot)
		{
			OutError = FString::Printf(TEXT("Backend equipment slot is unsupported by runtime loadout: %s"), *BackendSlot.SlotType);
			return false;
		}

		if (!BackendSlot.bHasItem)
		{
			RuntimeSlot->bOccupied = false;
			RuntimeSlot->ItemInstance = FFrontierItemInstance();
			continue;
		}

		FFrontierItemInstance RuntimeItem;
		if (!ConvertBackendEquipmentItemToRuntime(BackendSlot.Item, ItemCatalog, RuntimeItem, OutError))
		{
			FRONTIER_LOG(Warning, TEXT("Backend equipment item conversion failed. SlotType=%s ItemInstanceId=%s ItemTemplateId=%s Error=%s"),
				*BackendSlot.SlotType,
				*BackendSlot.Item.ItemInstanceId,
				*BackendSlot.Item.ItemTemplateId,
				*OutError);
			return false;
		}

		RuntimeSlot->bOccupied = true;
		RuntimeSlot->ItemInstance = MoveTemp(RuntimeItem);
	}

	return true;
}
