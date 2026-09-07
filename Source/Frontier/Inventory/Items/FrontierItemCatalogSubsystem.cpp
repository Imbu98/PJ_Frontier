#include "Inventory/Items/FrontierItemCatalogSubsystem.h"

#include "Engine/DataTable.h"
#include "Frontier.h"
#include "Inventory/Items/FrontierArmorItemDataAsset.h"
#include "Inventory/Items/FrontierAccessoryItemDataAsset.h"
#include "Inventory/Items/FrontierConsumableItemDataAsset.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "Inventory/Items/FrontierItemTemplateSettings.h"
#include "Inventory/Items/FrontierWeaponItemDataAsset.h"

namespace
{
void NormalizeCommonTemplateData(FFrontierItemTemplateData& TemplateData)
{
	TemplateData.MaxStack = FMath::Max(1, TemplateData.MaxStack);
	TemplateData.SellPriceGold = FMath::Max(0, TemplateData.SellPriceGold);
	TemplateData.ItemWeight = FMath::Max(0.0f, TemplateData.ItemWeight);
	TemplateData.DropImpulseForward = FMath::Max(0.0f, TemplateData.DropImpulseForward);
	TemplateData.DropImpulseUpward = FMath::Max(0.0f, TemplateData.DropImpulseUpward);
}

bool ValidateCommonTemplateData(
	const FName ItemTemplateId,
	const FFrontierItemTemplateData& TemplateData,
	const EFrontierItemCategory ExpectedCategory,
	const UDataTable* SourceTable)
{
	if (ItemTemplateId.IsNone())
	{
		FRONTIER_LOG(Error, TEXT("Typed item template contains an empty RowName. Table=%s"), *GetNameSafe(SourceTable));
		return false;
	}
	if (TemplateData.Category != ExpectedCategory)
	{
		FRONTIER_LOG(Error, TEXT("Typed item template category mismatch. ItemTemplateId=%s Expected=%d Actual=%d Table=%s"),
			*ItemTemplateId.ToString(),
			static_cast<int32>(ExpectedCategory),
			static_cast<int32>(TemplateData.Category),
			*GetNameSafe(SourceTable));
		return false;
	}
	if (!IsValidElementForCategory(TemplateData.Category, TemplateData.ElementalType))
	{
		FRONTIER_LOG(Error, TEXT("Typed item template has an invalid elemental type. ItemTemplateId=%s Category=%d ElementalType=%d Table=%s"),
			*ItemTemplateId.ToString(),
			static_cast<int32>(TemplateData.Category),
			static_cast<int32>(TemplateData.ElementalType),
			*GetNameSafe(SourceTable));
		return false;
	}
	return true;
}

template <typename RowType>
UDataTable* LoadTypedDataTable(
	const TSoftObjectPtr<UDataTable>& DataTableAsset,
	const TCHAR* TablePurpose,
	TArray<TObjectPtr<UDataTable>>& LoadedTables)
{
	UDataTable* DataTable = DataTableAsset.LoadSynchronous();
	if (!DataTable)
	{
		FRONTIER_LOG(Warning, TEXT("%s DataTable is not configured."), TablePurpose);
		return nullptr;
	}
	if (DataTable->GetRowStruct() != RowType::StaticStruct())
	{
		FRONTIER_LOG(Error, TEXT("%s DataTable row struct mismatch. Table=%s Expected=%s Actual=%s"),
			TablePurpose,
			*GetNameSafe(DataTable),
			*GetNameSafe(RowType::StaticStruct()),
			*GetNameSafe(DataTable->GetRowStruct()));
		return nullptr;
	}

	LoadedTables.Add(DataTable);
	return DataTable;
}
}

void UFrontierItemCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RebuildCatalog();
}

const FFrontierResolvedItemTemplateData* UFrontierItemCatalogSubsystem::ResolveItemTemplateData(const FName ItemTemplateId) const
{
	return ItemTemplateId.IsNone() ? nullptr : ItemTemplateCatalog.Find(ItemTemplateId);
}

FFrontierResolvedItemTemplateData UFrontierItemCatalogSubsystem::BuildTemplateDataFromItemData(const UFrontierItemDataAsset& ItemData)
{
	FFrontierResolvedItemTemplateData ResolvedData;
	ResolvedData.ItemTemplateId = ItemData.GetTemplateId();
	FFrontierItemTemplateData& Common = ResolvedData.Common;
	Common.DisplayName = ItemData.DisplayName;
	Common.Description = ItemData.Description;
	Common.Category = ItemData.Category;
	Common.SubCategory = ItemData.SubCategory;
	Common.ElementalType = ItemData.ElementalType;
	Common.bStackable = ItemData.bStackable;
	Common.MaxStack = FMath::Max(1, ItemData.MaxStack > 1 ? ItemData.MaxStack : ItemData.MaxStackCount);
	Common.BindState = ItemData.BindState;
	Common.bTradable = ItemData.bTradable;
	Common.SellPriceGold = FMath::Max(0, ItemData.SellPriceGold > 0 ? ItemData.SellPriceGold : ItemData.SellPrice);
	Common.ItemWeight = FMath::Max(0.0f, ItemData.ItemWeight > 0.0f ? ItemData.ItemWeight : ItemData.Weight);
	Common.Icon = ItemData.Icon;
	ResolvedData.MaxDurability = FMath::Max(0, ItemData.MaxDurability);

	if (const UFrontierWeaponItemDataAsset* WeaponItemData = Cast<UFrontierWeaponItemDataAsset>(&ItemData))
	{
		ResolvedData.EquipSlot = WeaponItemData->EquipSlot;
		ResolvedData.EquipmentBaseValue = WeaponItemData->EquipmentData.BaseValue;
		ResolvedData.WeaponData = WeaponItemData->WeaponData;
		ResolvedData.WeaponActorClass = WeaponItemData->WeaponActorClass;
	}
	else if (const UFrontierArmorItemDataAsset* ArmorItemData = Cast<UFrontierArmorItemDataAsset>(&ItemData))
	{
		ResolvedData.EquipSlot = ArmorItemData->EquipSlot;
		ResolvedData.EquipmentBaseValue = ArmorItemData->EquipmentData.BaseValue;
		ResolvedData.EquipmentMesh = ItemData.EquipmentMesh;
		ResolvedData.ArmorAppearances = ArmorItemData->CharacterAppearances;
	}
	else if (const UFrontierAccessoryItemDataAsset* AccessoryItemData = Cast<UFrontierAccessoryItemDataAsset>(&ItemData))
	{
		ResolvedData.EquipSlot = AccessoryItemData->EquipSlot;
		ResolvedData.EquipmentBaseValue = AccessoryItemData->EquipmentData.BaseValue;
	}
	else if (const UFrontierConsumableItemDataAsset* ConsumableItemData = Cast<UFrontierConsumableItemDataAsset>(&ItemData))
	{
		ResolvedData.AllowedEquipSlots = ConsumableItemData->AllowedEquipSlots;
		ResolvedData.ConsumeEffectClass = ConsumableItemData->ConsumeEffectClass;
		ResolvedData.HealthRestoreAmount = ConsumableItemData->HealthRestoreAmount;
		ResolvedData.StaminaRestoreAmount = ConsumableItemData->StaminaRestoreAmount;
		ResolvedData.UseTime = ConsumableItemData->UseTime;
		ResolvedData.UseMontage = ConsumableItemData->UseMontage;
		ResolvedData.PotionSocketName = ConsumableItemData->PotionSocketName;
		ResolvedData.PotionMesh = ConsumableItemData->PotionMesh;
		ResolvedData.PotionMeshRelativeTransform = ConsumableItemData->PotionMeshRelativeTransform;
		ResolvedData.CooldownGroupTag = ConsumableItemData->CooldownGroupTag;
		ResolvedData.bUsableInRaid = ConsumableItemData->bUsableInRaid;
		ResolvedData.bUsableInLobby = ConsumableItemData->bUsableInLobby;
	}

	return ResolvedData;
}

void UFrontierItemCatalogSubsystem::RebuildCatalog()
{
	ItemTemplateCatalog.Reset();
	LoadedTemplateDataTables.Reset();

	LoadWeaponTemplateRows();
	LoadArmorTemplateRows();
	LoadConsumableTemplateRows();
	LoadMiscellaneousTemplateRows();
	LoadAccessoryTemplateRows();

	FRONTIER_LOG(Log, TEXT("Typed item catalog initialized. TemplateCount=%d LoadedTableCount=%d"),
		ItemTemplateCatalog.Num(),
		LoadedTemplateDataTables.Num());
}

bool UFrontierItemCatalogSubsystem::AddResolvedTemplate(
	const FName ItemTemplateId,
	FFrontierResolvedItemTemplateData&& TemplateData,
	const UDataTable* SourceTable)
{
	if (ItemTemplateCatalog.Contains(ItemTemplateId))
	{
		FRONTIER_LOG(Error, TEXT("Duplicate ItemTemplateId across typed DataTables. ItemTemplateId=%s Table=%s"),
			*ItemTemplateId.ToString(),
			*GetNameSafe(SourceTable));
		return false;
	}

	TemplateData.ItemTemplateId = ItemTemplateId;
	NormalizeCommonTemplateData(TemplateData.Common);
	TemplateData.MaxDurability = FMath::Max(0, TemplateData.MaxDurability);
	TemplateData.EquipmentBaseValue = FMath::Max(0.0f, TemplateData.EquipmentBaseValue);
	TemplateData.HealthRestoreAmount = FMath::Max(0.0f, TemplateData.HealthRestoreAmount);
	TemplateData.StaminaRestoreAmount = FMath::Max(0.0f, TemplateData.StaminaRestoreAmount);
	TemplateData.UseTime = FMath::Max(0.0f, TemplateData.UseTime);
	ItemTemplateCatalog.Add(ItemTemplateId, MoveTemp(TemplateData));
	return true;
}

void UFrontierItemCatalogSubsystem::LoadWeaponTemplateRows()
{
	const UFrontierItemTemplateSettings* Settings = GetDefault<UFrontierItemTemplateSettings>();
	UDataTable* DataTable = LoadTypedDataTable<FFrontierWeaponTemplateData>(
		Settings ? Settings->WeaponTemplateDataTable : TSoftObjectPtr<UDataTable>(),
		TEXT("Weapon template"),
		LoadedTemplateDataTables);
	if (!DataTable)
	{
		return;
	}

	for (const FName RowName : DataTable->GetRowNames())
	{
		const FFrontierWeaponTemplateData* Row = DataTable->FindRow<FFrontierWeaponTemplateData>(
			RowName,
			TEXT("FrontierWeaponTemplateCatalog"),
			false);
		if (!Row || !ValidateCommonTemplateData(RowName, Row->ItemTemplateData, EFrontierItemCategory::Weapon, DataTable))
		{
			continue;
		}

		FFrontierResolvedItemTemplateData ResolvedData;
		ResolvedData.Common = Row->ItemTemplateData;
		ResolvedData.EquipSlot = Row->EquipSlot;
		ResolvedData.EquipmentBaseValue = Row->EquipmentData.BaseValue;
		ResolvedData.MaxDurability = Row->MaxDurability;
		ResolvedData.WeaponData = Row->WeaponData;
		ResolvedData.WeaponActorClass = Row->WeaponActorClass;
		AddResolvedTemplate(RowName, MoveTemp(ResolvedData), DataTable);
	}
}

void UFrontierItemCatalogSubsystem::LoadArmorTemplateRows()
{
	const UFrontierItemTemplateSettings* Settings = GetDefault<UFrontierItemTemplateSettings>();
	UDataTable* DataTable = LoadTypedDataTable<FFrontierArmorTemplateData>(
		Settings ? Settings->ArmorTemplateDataTable : TSoftObjectPtr<UDataTable>(),
		TEXT("Armor template"),
		LoadedTemplateDataTables);
	if (!DataTable)
	{
		return;
	}

	for (const FName RowName : DataTable->GetRowNames())
	{
		const FFrontierArmorTemplateData* Row = DataTable->FindRow<FFrontierArmorTemplateData>(
			RowName,
			TEXT("FrontierArmorTemplateCatalog"),
			false);
		if (!Row || !ValidateCommonTemplateData(RowName, Row->ItemTemplateData, EFrontierItemCategory::Armor, DataTable))
		{
			continue;
		}

		FFrontierResolvedItemTemplateData ResolvedData;
		ResolvedData.Common = Row->ItemTemplateData;
		ResolvedData.EquipSlot = Row->EquipSlot;
		ResolvedData.EquipmentBaseValue = Row->EquipmentData.BaseValue;
		ResolvedData.MaxDurability = Row->MaxDurability;
		ResolvedData.ArmorAppearances = Row->CharacterAppearances;
		AddResolvedTemplate(RowName, MoveTemp(ResolvedData), DataTable);
	}
}

void UFrontierItemCatalogSubsystem::LoadConsumableTemplateRows()
{
	const UFrontierItemTemplateSettings* Settings = GetDefault<UFrontierItemTemplateSettings>();
	UDataTable* DataTable = LoadTypedDataTable<FFrontierConsumableTemplateData>(
		Settings ? Settings->ConsumableTemplateDataTable : TSoftObjectPtr<UDataTable>(),
		TEXT("Consumable template"),
		LoadedTemplateDataTables);
	if (!DataTable)
	{
		return;
	}

	for (const FName RowName : DataTable->GetRowNames())
	{
		const FFrontierConsumableTemplateData* Row = DataTable->FindRow<FFrontierConsumableTemplateData>(
			RowName,
			TEXT("FrontierConsumableTemplateCatalog"),
			false);
		if (!Row || !ValidateCommonTemplateData(RowName, Row->ItemTemplateData, EFrontierItemCategory::Consumable, DataTable))
		{
			continue;
		}

		FFrontierResolvedItemTemplateData ResolvedData;
		ResolvedData.Common = Row->ItemTemplateData;
		ResolvedData.ConsumeEffectClass = Row->ConsumeEffectClass;
		ResolvedData.HealthRestoreAmount = Row->HealthRestoreAmount;
		ResolvedData.StaminaRestoreAmount = Row->StaminaRestoreAmount;
		ResolvedData.UseTime = Row->UseTime;
		ResolvedData.UseMontage = Row->UseMontage;
		ResolvedData.PotionSocketName = Row->PotionSocketName;
		ResolvedData.PotionMesh = Row->PotionMesh;
		ResolvedData.PotionMeshRelativeTransform = Row->PotionMeshRelativeTransform;
		ResolvedData.CooldownGroupTag = Row->CooldownGroupTag;
		ResolvedData.bUsableInRaid = Row->bUsableInRaid;
		ResolvedData.bUsableInLobby = Row->bUsableInLobby;
		AddResolvedTemplate(RowName, MoveTemp(ResolvedData), DataTable);
	}
}

void UFrontierItemCatalogSubsystem::LoadMiscellaneousTemplateRows()
{
	const UFrontierItemTemplateSettings* Settings = GetDefault<UFrontierItemTemplateSettings>();
	UDataTable* DataTable = LoadTypedDataTable<FFrontierMaterialTemplateData>(
		Settings ? Settings->MiscellaneousTemplateDataTable : TSoftObjectPtr<UDataTable>(),
		TEXT("Miscellaneous template"),
		LoadedTemplateDataTables);
	if (!DataTable)
	{
		return;
	}

	for (const FName RowName : DataTable->GetRowNames())
	{
		const FFrontierMaterialTemplateData* Row = DataTable->FindRow<FFrontierMaterialTemplateData>(
			RowName,
			TEXT("FrontierMiscellaneousTemplateCatalog"),
			false);
		if (!Row || !ValidateCommonTemplateData(RowName, Row->ItemTemplateData, EFrontierItemCategory::Material, DataTable))
		{
			continue;
		}

		FFrontierResolvedItemTemplateData ResolvedData;
		ResolvedData.Common = Row->ItemTemplateData;
		AddResolvedTemplate(RowName, MoveTemp(ResolvedData), DataTable);
	}
}

void UFrontierItemCatalogSubsystem::LoadAccessoryTemplateRows()
{
	const UFrontierItemTemplateSettings* Settings = GetDefault<UFrontierItemTemplateSettings>();
	UDataTable* DataTable = LoadTypedDataTable<FFrontierAccessoryTemplateData>(
		Settings ? Settings->AccessoryTemplateDataTable : TSoftObjectPtr<UDataTable>(),
		TEXT("Accessory template"),
		LoadedTemplateDataTables);
	if (!DataTable)
	{
		return;
	}

	for (const FName RowName : DataTable->GetRowNames())
	{
		const FFrontierAccessoryTemplateData* Row = DataTable->FindRow<FFrontierAccessoryTemplateData>(
			RowName,
			TEXT("FrontierAccessoryTemplateCatalog"),
			false);
		if (!Row || !ValidateCommonTemplateData(RowName, Row->ItemTemplateData, EFrontierItemCategory::Accessory, DataTable))
		{
			continue;
		}

		FFrontierResolvedItemTemplateData ResolvedData;
		ResolvedData.Common = Row->ItemTemplateData;
		ResolvedData.EquipSlot = Row->EquipSlot;
		ResolvedData.EquipmentBaseValue = Row->EquipmentData.BaseValue;
		ResolvedData.MaxDurability = Row->MaxDurability;
		AddResolvedTemplate(RowName, MoveTemp(ResolvedData), DataTable);
	}
}
