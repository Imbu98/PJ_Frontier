#include "Components/FrontierLootComponent.h"

#include "Frontier.h"
#include "Engine/GameInstance.h"
#include "Inventory/Items/FrontierItemCatalogSubsystem.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "Loot/FrontierLootItemPresetDataAsset.h"
#include "Progression/FrontierRaidLootPoolSubsystem.h"

UFrontierLootComponent::UFrontierLootComponent()
{
	SetIsReplicatedByDefault(false);
	PrimaryComponentTick.bCanEverTick = false;
}

void UFrontierLootComponent::BeginPlay()
{
	Super::BeginPlay();
	if (IsBackendLootManaged() && GetOwner() && GetOwner()->HasAuthority())
	{
		if (UFrontierRaidLootPoolSubsystem* Pool = GetWorld()
			? GetWorld()->GetSubsystem<UFrontierRaidLootPoolSubsystem>()
			: nullptr)
		{
			Pool->RegisterLootComponent(this);
		}
	}
}

void UFrontierLootComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UFrontierRaidLootPoolSubsystem* Pool = GetWorld()
		? GetWorld()->GetSubsystem<UFrontierRaidLootPoolSubsystem>()
		: nullptr)
	{
		Pool->UnregisterLootComponent(this);
	}
	Super::EndPlay(EndPlayReason);
}

TArray<FFrontierInventorySlot> UFrontierLootComponent::GenerateLootSlots() const
{
	if (IsBackendLootManaged())
	{
		if (!bBackendLootPrepared)
		{
			FRONTIER_LOG(
				Warning,
				TEXT("Backend-managed loot requested before its pool entry was prepared. Owner=%s LootTableId=%s"),
				*GetNameSafe(GetOwner()),
				*LootTableId.ToString());
			return {};
		}
		return PreparedBackendLootSlots;
	}
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		FRONTIER_LOG(
			Error,
			TEXT("Dedicated server refused legacy local loot generation. Configure LootTableId. Owner=%s"),
			*GetNameSafe(GetOwner()));
		return {};
	}

	if (const UFrontierLootDropDataAsset* DropData = LootDropDataAsset)
	{
		return GenerateLootSlotsFromTable(DropData->LootTable, DropData->MinLootRolls, DropData->MaxLootRolls);
	}

	return GenerateLootSlotsFromTable(LootTable, MinLootRolls, MaxLootRolls);
}

void UFrontierLootComponent::AssignBackendLootSlots(TArray<FFrontierInventorySlot> InLootSlots)
{
	if (!IsBackendLootManaged() || bBackendLootPrepared)
	{
		return;
	}
	PreparedBackendLootSlots = MoveTemp(InLootSlots);
	bBackendLootPrepared = true;
	BackendLootPrepared.Broadcast();
}

TArray<FFrontierInventorySlot> UFrontierLootComponent::GenerateLootSlotsFromTable(
	const TArray<FFrontierLootTableEntry>& SourceLootTable,
	const int32 MinRolls,
	const int32 MaxRolls) const
{
	TArray<FFrontierInventorySlot> GeneratedSlots;
	if (SourceLootTable.IsEmpty())
	{
		return GeneratedSlots;
	}

	const int32 SafeMinRolls = FMath::Max(0, MinRolls);
	const int32 SafeMaxRolls = FMath::Max(SafeMinRolls, MaxRolls);
	const int32 RandomRollCount = FMath::RandRange(SafeMinRolls, SafeMaxRolls);
	const int32 RollCount = SourceLootTable.IsEmpty() ? 0 : FMath::Max(1, RandomRollCount);

	for (int32 RollIndex = 0; RollIndex < RollCount; ++RollIndex)
	{
		int32 TotalWeight = 0;
		for (const FFrontierLootTableEntry& Entry : SourceLootTable)
		{
			if ((!Entry.ItemPreset.IsNull() || !Entry.ItemTemplate.IsNull()) && Entry.Weight > 0)
			{
				TotalWeight += Entry.Weight;
			}
		}

		if (TotalWeight <= 0)
		{
			break;
		}

		int32 RandomWeight = FMath::RandRange(1, TotalWeight);
		for (const FFrontierLootTableEntry& Entry : SourceLootTable)
		{
			if ((Entry.ItemPreset.IsNull() && Entry.ItemTemplate.IsNull()) || Entry.Weight <= 0)
			{
				continue;
			}

			RandomWeight -= Entry.Weight;
			if (RandomWeight > 0)
			{
				continue;
			}

			const int32 Quantity = FMath::RandRange(
				FMath::Max(1, Entry.MinQuantity),
				FMath::Max(Entry.MinQuantity, Entry.MaxQuantity));
			FFrontierItemInstance ItemInstance;
			if (const UFrontierLootItemPresetDataAsset* ItemPreset = Entry.ItemPreset.LoadSynchronous())
			{
				const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
				const UFrontierItemCatalogSubsystem* ItemCatalog = GameInstance
					? GameInstance->GetSubsystem<UFrontierItemCatalogSubsystem>()
					: nullptr;
				const FFrontierResolvedItemTemplateData* ResolvedTemplate = ItemCatalog
					? ItemCatalog->ResolveItemTemplateData(ItemPreset->ItemTemplateId)
					: nullptr;
				if (ResolvedTemplate)
				{
					ItemInstance = ItemPreset->CreateTemporaryItemInstance(*ResolvedTemplate, Quantity);
				}
				else
				{
					FRONTIER_LOG(
						Warning,
						TEXT("Loot preset ItemTemplateId could not be resolved from the item catalog. Preset=%s ItemTemplateId=%s"),
						*GetNameSafe(ItemPreset),
						*ItemPreset->ItemTemplateId.ToString());
				}
			}
			else if (const UFrontierItemDataAsset* ItemTemplate = Entry.ItemTemplate.LoadSynchronous())
			{
				ItemInstance = FFrontierItemInstance::CreateFromItemData(ItemTemplate, Quantity);
			}
			if (!ItemInstance.IsValid())
			{
				FRONTIER_LOG(
					Warning,
					TEXT("Selected loot entry could not create a valid item. Owner=%s"),
					*GetNameSafe(GetOwner()));
				break;
			}

			// New raid loot has no permanent origin ID. Its raid ID remains stable through
			// inventory/loadout moves and is minted by the backend after extraction.
			if (!ItemInstance.RaidItemId.IsValid())
			{
				ItemInstance.RaidItemId = FGuid::NewGuid();
			}
			ItemInstance.RaidLootSourceId = FString::Printf(
				TEXT("ds-loot-%s"),
				*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
			FRONTIER_LOG(
				Log,
				TEXT("[RaidLootIdentity] Assigned dedicated-server lootSourceId. Owner=%s RaidItemId=%s LootSourceId=%s"),
				*GetNameSafe(GetOwner()),
				*ItemInstance.RaidItemId.ToString(EGuidFormats::DigitsWithHyphensLower),
				*ItemInstance.RaidLootSourceId);

			FFrontierInventorySlot LootSlot;
			LootSlot.SlotIndex = GeneratedSlots.Num();
			LootSlot.bOccupied = true;
			LootSlot.ItemInstance = MoveTemp(ItemInstance);
			GeneratedSlots.Add(LootSlot);
			break;
		}
	}

	return GeneratedSlots;
}
