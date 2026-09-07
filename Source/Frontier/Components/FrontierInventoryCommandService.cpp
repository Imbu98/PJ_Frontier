#include "Components/FrontierInventoryCommandService.h"

#include "Components/FrontierLoadoutComponent.h"
#include "Components/FrontierBackendProtocolComponent.h"
#include "Components/FrontierEquipmentComponent.h"
#include "Components/FrontierQuickSlotComponent.h"
#include "Components/FrontierRaidInventoryComponent.h"
#include "Components/FrontierStorageComponent.h"
#include "Frontier.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierLobbyPlayerController.h"
#include "Game/FrontierPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Loot/FrontierDroppedItemActor.h"
#include "Loot/FrontierLootContainerActor.h"

UFrontierInventoryCommandService::UFrontierInventoryCommandService()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UFrontierInventoryCommandService::EquipRaidInventoryItemToLoadout(
	const int32 SourceSlotIndex,
	const EFrontierEquipmentSlot SlotType) const
{
	AFrontierPlayerState* PlayerState = GetFrontierPlayerState();
	UFrontierRaidInventoryComponent* RaidInventory = PlayerState ? PlayerState->GetRaidInventoryComponent() : nullptr;
	UFrontierLoadoutComponent* Loadout = PlayerState ? PlayerState->GetLoadoutComponent() : nullptr;
	UFrontierQuickSlotComponent* QuickSlots = PlayerState ? PlayerState->GetQuickSlotComponent() : nullptr;
	if (!IsLobbyBackendCommandContext())
	{
		FFrontierInventorySlot SourceSlot;
		if (!RaidInventory
			|| !Loadout
			|| !RaidInventory->GetSlot(SourceSlotIndex, SourceSlot)
			|| !SourceSlot.bOccupied
			|| !SourceSlot.ItemInstance.IsValid())
		{
			return false;
		}

		const FFrontierItemInstance EquippedItem = SourceSlot.ItemInstance;
		const bool bEquipped = Loadout->EquipItemFromInventory(RaidInventory, SourceSlotIndex, SlotType);
		if (bEquipped && QuickSlots)
		{
			QuickSlots->ClearQuickSlotsMatchingItem(EquippedItem);
		}
		return bEquipped;
	}

	FFrontierInventorySlot SourceSlot;
	if (!RaidInventory
		|| !Loadout
		|| !RaidInventory->GetSlot(SourceSlotIndex, SourceSlot)
		|| !SourceSlot.bOccupied
		|| !SourceSlot.ItemInstance.IsValid()
		|| !SourceSlot.ItemInstance.ItemInstanceId.IsValid()
		|| !Loadout->CanEquipItemToSlot(SlotType, SourceSlot.ItemInstance))
	{
		return false;
	}

	FFrontierLoadoutSlot ExistingLoadoutSlot;
	const bool bReplaceExisting = Loadout->FindLoadoutSlot(SlotType, ExistingLoadoutSlot)
		&& ExistingLoadoutSlot.bOccupied
		&& ExistingLoadoutSlot.ItemInstance.IsValid();

	UFrontierBackendProtocolComponent* BackendProtocol = GetBackendProtocolComponent();
	const bool bRequested = BackendProtocol
		&& BackendProtocol->RequestEquipInventoryItem(
			SourceSlot.ItemInstance.ItemInstanceId,
			SlotType,
			bReplaceExisting,
			bReplaceExisting ? TOptional<int32>(SourceSlotIndex) : TOptional<int32>());
	if (bRequested && QuickSlots)
	{
		QuickSlots->ClearQuickSlotsMatchingItem(SourceSlot.ItemInstance);
	}
	return bRequested;
}

bool UFrontierInventoryCommandService::UnequipLoadoutItemToRaidInventory(const EFrontierEquipmentSlot SlotType) const
{
	AFrontierPlayerState* PlayerState = GetFrontierPlayerState();
	UFrontierRaidInventoryComponent* RaidInventory = PlayerState ? PlayerState->GetRaidInventoryComponent() : nullptr;
	UFrontierLoadoutComponent* Loadout = PlayerState ? PlayerState->GetLoadoutComponent() : nullptr;
	if (!IsLobbyBackendCommandContext())
	{
		return RaidInventory && Loadout && Loadout->UnequipItemToInventory(RaidInventory, SlotType);
	}

	FFrontierLoadoutSlot LoadoutSlot;
	if (!Loadout
		|| !Loadout->FindLoadoutSlot(SlotType, LoadoutSlot)
		|| !LoadoutSlot.bOccupied
		|| !LoadoutSlot.ItemInstance.IsValid()
		|| !LoadoutSlot.ItemInstance.ItemInstanceId.IsValid())
	{
		return false;
	}

	UFrontierBackendProtocolComponent* BackendProtocol = GetBackendProtocolComponent();
	return BackendProtocol
		&& BackendProtocol->RequestUnequipItem(SlotType, LoadoutSlot.ItemInstance.ItemInstanceId);
}

bool UFrontierInventoryCommandService::UnequipLoadoutItemToRaidInventorySlot(
	const EFrontierEquipmentSlot SlotType,
	const int32 TargetSlotIndex) const
{
	AFrontierPlayerState* PlayerState = GetFrontierPlayerState();
	UFrontierRaidInventoryComponent* RaidInventory = PlayerState ? PlayerState->GetRaidInventoryComponent() : nullptr;
	UFrontierLoadoutComponent* Loadout = PlayerState ? PlayerState->GetLoadoutComponent() : nullptr;
	if (!IsLobbyBackendCommandContext())
	{
		return RaidInventory && Loadout && Loadout->UnequipItemToInventorySlot(RaidInventory, SlotType, TargetSlotIndex);
	}

	FFrontierInventorySlot TargetSlot;
	FFrontierLoadoutSlot LoadoutSlot;
	if (!RaidInventory
		|| !Loadout
		|| !RaidInventory->GetSlot(TargetSlotIndex, TargetSlot)
		|| (TargetSlot.bOccupied && TargetSlot.ItemInstance.IsValid())
		|| !Loadout->FindLoadoutSlot(SlotType, LoadoutSlot)
		|| !LoadoutSlot.bOccupied
		|| !LoadoutSlot.ItemInstance.IsValid()
		|| !LoadoutSlot.ItemInstance.ItemInstanceId.IsValid())
	{
		return false;
	}

	UFrontierBackendProtocolComponent* BackendProtocol = GetBackendProtocolComponent();
	return BackendProtocol
		&& BackendProtocol->RequestUnequipItem(SlotType, LoadoutSlot.ItemInstance.ItemInstanceId, TargetSlotIndex);
}

bool UFrontierInventoryCommandService::SwapRaidInventorySlots(const int32 SourceSlotIndex, const int32 TargetSlotIndex) const
{
	AFrontierPlayerState* PlayerState = GetFrontierPlayerState();
	UFrontierRaidInventoryComponent* RaidInventory = PlayerState ? PlayerState->GetRaidInventoryComponent() : nullptr;
	if (!IsLobbyBackendCommandContext())
	{
		return RaidInventory && RaidInventory->SwapSlots(SourceSlotIndex, TargetSlotIndex);
	}

	FFrontierInventorySlot SourceSlot;
	FFrontierInventorySlot TargetSlot;
	if (!RaidInventory
		|| !RaidInventory->GetSlot(SourceSlotIndex, SourceSlot)
		|| !RaidInventory->GetSlot(TargetSlotIndex, TargetSlot)
		|| !SourceSlot.bOccupied
		|| !SourceSlot.ItemInstance.IsValid()
		|| !SourceSlot.ItemInstance.ItemInstanceId.IsValid())
	{
		return false;
	}

	UFrontierBackendProtocolComponent* BackendProtocol = GetBackendProtocolComponent();
	if (TargetSlot.bOccupied && TargetSlot.ItemInstance.IsValid() && TargetSlot.ItemInstance.ItemInstanceId.IsValid())
	{
		return BackendProtocol
			&& BackendProtocol->RequestSwapInventoryItems(
				SourceSlotIndex,
				TargetSlotIndex,
				SourceSlot.ItemInstance.ItemInstanceId,
				TargetSlot.ItemInstance.ItemInstanceId);
	}

	return BackendProtocol
		&& BackendProtocol->RequestMoveInventoryItem(SourceSlot.ItemInstance.ItemInstanceId, TargetSlotIndex);
}

bool UFrontierInventoryCommandService::SwapStorageSlots(const int32 SourceSlotIndex, const int32 TargetSlotIndex) const
{
	if (!IsLobbyBackendCommandContext())
	{
		return false;
	}

	AFrontierPlayerState* PlayerState = GetFrontierPlayerState();
	UFrontierStorageComponent* Storage = PlayerState ? PlayerState->GetStorageComponent() : nullptr;
	FFrontierInventorySlot SourceSlot;
	FFrontierInventorySlot TargetSlot;
	if (!Storage
		|| !Storage->GetSlot(SourceSlotIndex, SourceSlot)
		|| !Storage->GetSlot(TargetSlotIndex, TargetSlot)
		|| !SourceSlot.bOccupied
		|| !SourceSlot.ItemInstance.IsValid()
		|| !SourceSlot.ItemInstance.ItemInstanceId.IsValid())
	{
		return false;
	}

	UFrontierBackendProtocolComponent* BackendProtocol = GetBackendProtocolComponent();
	if (TargetSlot.bOccupied && TargetSlot.ItemInstance.IsValid() && TargetSlot.ItemInstance.ItemInstanceId.IsValid())
	{
		return BackendProtocol
			&& BackendProtocol->RequestSwapStorageItems(
				SourceSlotIndex,
				TargetSlotIndex,
				SourceSlot.ItemInstance.ItemInstanceId,
				TargetSlot.ItemInstance.ItemInstanceId);
	}

	return BackendProtocol
		&& BackendProtocol->RequestMoveStorageItem(SourceSlot.ItemInstance.ItemInstanceId, TargetSlotIndex);
}

bool UFrontierInventoryCommandService::UseRaidInventoryItem(const int32 SourceSlotIndex) const
{
	AFrontierPlayerState* PlayerState = GetFrontierPlayerState();
	UFrontierRaidInventoryComponent* RaidInventory = PlayerState ? PlayerState->GetRaidInventoryComponent() : nullptr;
	return RaidInventory && RaidInventory->UseItemAtSlot(SourceSlotIndex);
}

bool UFrontierInventoryCommandService::RegisterQuickSlotFromRaidInventory(
	const int32 QuickSlotIndex,
	const int32 InventorySlotIndex) const
{
	AFrontierPlayerState* PlayerState = GetFrontierPlayerState();
	UFrontierQuickSlotComponent* QuickSlots = PlayerState ? PlayerState->GetQuickSlotComponent() : nullptr;
	UFrontierRaidInventoryComponent* RaidInventory = PlayerState ? PlayerState->GetRaidInventoryComponent() : nullptr;
	FFrontierInventorySlot InventorySlot;
	if (!QuickSlots
		|| !RaidInventory
		|| QuickSlotIndex < 0
		|| QuickSlotIndex >= UFrontierQuickSlotComponent::QuickSlotCount
		|| !RaidInventory->GetSlot(InventorySlotIndex, InventorySlot)
		|| !InventorySlot.bOccupied
		|| !InventorySlot.ItemInstance.IsValid())
	{
		return false;
	}

	const FFrontierItemInstance& Item = InventorySlot.ItemInstance;
	FFrontierQuickSlotEntry Entry;
	Entry.SlotIndex = QuickSlotIndex;
	Entry.bOccupied = true;
	Entry.ItemTemplateId = Item.GetTemplateId();
	Entry.EquipSlot = Item.GetEquipSlot();

	if (Item.GetCategory() == EFrontierItemCategory::Weapon)
	{
		Entry.BindingType = EFrontierQuickSlotBindingType::WeaponInstance;
		Entry.ItemInstanceId = Item.ItemInstanceId;
		Entry.RaidItemId = Item.RaidItemId;
	}
	else if (Item.GetCategory() == EFrontierItemCategory::Consumable && Item.IsUsableInRaid())
	{
		Entry.BindingType = EFrontierQuickSlotBindingType::ConsumableTemplate;
	}
	else
	{
		return false;
	}

	for (int32 ExistingQuickSlotIndex = 0;
		ExistingQuickSlotIndex < UFrontierQuickSlotComponent::QuickSlotCount;
		++ExistingQuickSlotIndex)
	{
		if (ExistingQuickSlotIndex == QuickSlotIndex)
		{
			continue;
		}

		FFrontierQuickSlotEntry ExistingEntry;
		if (QuickSlots->GetQuickSlot(ExistingQuickSlotIndex, ExistingEntry)
			&& ExistingEntry.bOccupied
			&& ExistingEntry.MatchesItem(Item))
		{
			return false;
		}
	}

	return QuickSlots->SetQuickSlot(QuickSlotIndex, Entry);
}

bool UFrontierInventoryCommandService::ClearQuickSlot(const int32 QuickSlotIndex) const
{
	AFrontierPlayerState* PlayerState = GetFrontierPlayerState();
	UFrontierQuickSlotComponent* QuickSlots = PlayerState ? PlayerState->GetQuickSlotComponent() : nullptr;
	return QuickSlots && QuickSlots->ClearQuickSlot(QuickSlotIndex);
}

bool UFrontierInventoryCommandService::MoveOrSwapQuickSlots(
	const int32 SourceQuickSlotIndex,
	const int32 TargetQuickSlotIndex) const
{
	AFrontierPlayerState* PlayerState = GetFrontierPlayerState();
	UFrontierQuickSlotComponent* QuickSlots = PlayerState ? PlayerState->GetQuickSlotComponent() : nullptr;
	return QuickSlots && QuickSlots->MoveOrSwapQuickSlots(SourceQuickSlotIndex, TargetQuickSlotIndex);
}

bool UFrontierInventoryCommandService::UseQuickSlot(const int32 QuickSlotIndex) const
{
	AFrontierPlayerState* PlayerState = GetFrontierPlayerState();
	UFrontierQuickSlotComponent* QuickSlots = PlayerState ? PlayerState->GetQuickSlotComponent() : nullptr;
	UFrontierRaidInventoryComponent* RaidInventory = PlayerState ? PlayerState->GetRaidInventoryComponent() : nullptr;
	UFrontierLoadoutComponent* Loadout = PlayerState ? PlayerState->GetLoadoutComponent() : nullptr;
	FFrontierQuickSlotEntry Entry;
	if (!QuickSlots || !RaidInventory || !Loadout || !QuickSlots->GetQuickSlot(QuickSlotIndex, Entry) || !Entry.bOccupied)
	{
		return false;
	}

	if (Entry.BindingType == EFrontierQuickSlotBindingType::ConsumableTemplate)
	{
		for (int32 SlotIndex = 0; SlotIndex < RaidInventory->GetSlots().Num(); ++SlotIndex)
		{
			const FFrontierInventorySlot& InventorySlot = RaidInventory->GetSlots()[SlotIndex];
			if (InventorySlot.bOccupied
				&& InventorySlot.ItemInstance.IsValid()
				&& Entry.MatchesItem(InventorySlot.ItemInstance)
				&& InventorySlot.ItemInstance.IsUsableInRaid())
			{
				return RaidInventory->UseItemAtSlot(SlotIndex);
			}
		}
		return false;
	}

	if (Entry.BindingType != EFrontierQuickSlotBindingType::WeaponInstance)
	{
		return false;
	}

	for (int32 InventorySlotIndex = 0; InventorySlotIndex < RaidInventory->GetSlots().Num(); ++InventorySlotIndex)
	{
		const FFrontierInventorySlot& InventorySlot = RaidInventory->GetSlots()[InventorySlotIndex];
		if (!InventorySlot.bOccupied || !InventorySlot.ItemInstance.IsValid() || !Entry.MatchesItem(InventorySlot.ItemInstance))
		{
			continue;
		}

		// Copy both items before EquipItemFromInventory mutates the inventory/loadout.
		// The previous loadout item is the weapon that must become the new quick-slot item.
		const FFrontierItemInstance QuickSlotWeapon = InventorySlot.ItemInstance;
		const EFrontierEquipmentSlot EquipSlot = QuickSlotWeapon.GetEquipSlot();
		if (EquipSlot == EFrontierEquipmentSlot::None)
		{
			return false;
		}

		FFrontierLoadoutSlot PreviousLoadoutSlot;
		const bool bHasPreviousWeapon = Loadout->FindLoadoutSlot(EquipSlot, PreviousLoadoutSlot)
			&& PreviousLoadoutSlot.bOccupied
			&& PreviousLoadoutSlot.ItemInstance.IsValid();

		if (IsLobbyBackendCommandContext())
		{
			return EquipRaidInventoryItemToLoadout(InventorySlotIndex, EquipSlot);
		}

		if (!Loadout->EquipItemFromInventory(RaidInventory, InventorySlotIndex, EquipSlot))
		{
			return false;
		}

		if (bHasPreviousWeapon)
		{
			FFrontierQuickSlotEntry SwappedEntry = Entry;
			SwappedEntry.bOccupied = true;
			SwappedEntry.BindingType = EFrontierQuickSlotBindingType::WeaponInstance;
			SwappedEntry.ItemInstanceId = PreviousLoadoutSlot.ItemInstance.ItemInstanceId;
			SwappedEntry.RaidItemId = PreviousLoadoutSlot.ItemInstance.RaidItemId;
			SwappedEntry.ItemTemplateId = PreviousLoadoutSlot.ItemInstance.GetTemplateId();
			SwappedEntry.EquipSlot = PreviousLoadoutSlot.ItemInstance.GetEquipSlot();
			QuickSlots->SetQuickSlot(QuickSlotIndex, SwappedEntry);
		}

		if (AFrontierPlayerController* Controller = GetAuthorityFrontierController())
		{
			APawn* Pawn = Controller->GetPawn();
			if (UFrontierEquipmentComponent* Equipment = Pawn ? Pawn->FindComponentByClass<UFrontierEquipmentComponent>() : nullptr)
			{
				Equipment->SelectWeaponByLoadoutSlot(EquipSlot);
			}
		}
		return true;
	}

	for (const FFrontierLoadoutSlot& LoadoutSlot : Loadout->GetLoadoutSlots())
	{
		if (LoadoutSlot.bOccupied
			&& LoadoutSlot.ItemInstance.IsValid()
			&& Entry.MatchesItem(LoadoutSlot.ItemInstance))
		{
			if (AFrontierPlayerController* Controller = GetAuthorityFrontierController())
			{
				APawn* Pawn = Controller->GetPawn();
				if (UFrontierEquipmentComponent* Equipment = Pawn ? Pawn->FindComponentByClass<UFrontierEquipmentComponent>() : nullptr)
				{
					return Equipment->SelectWeaponByLoadoutSlot(LoadoutSlot.SlotType);
				}
			}
			return false;
		}
	}

	return false;
}

bool UFrontierInventoryCommandService::StoreRaidItemInStorage(const int32 SourceRaidSlotIndex) const
{
	if (!IsLobbyBackendCommandContext())
	{
		return false;
	}

	AFrontierPlayerState* PlayerState = GetFrontierPlayerState();
	UFrontierRaidInventoryComponent* RaidInventory = PlayerState ? PlayerState->GetRaidInventoryComponent() : nullptr;
	FFrontierInventorySlot SourceSlot;
	if (!RaidInventory
		|| !RaidInventory->GetSlot(SourceRaidSlotIndex, SourceSlot)
		|| !SourceSlot.bOccupied
		|| !SourceSlot.ItemInstance.IsValid()
		|| !SourceSlot.ItemInstance.ItemInstanceId.IsValid())
	{
		return false;
	}

	UFrontierBackendProtocolComponent* BackendProtocol = GetBackendProtocolComponent();
	return BackendProtocol
		&& BackendProtocol->RequestDepositStorageItem(SourceSlot.ItemInstance.ItemInstanceId);
}

bool UFrontierInventoryCommandService::StoreRaidItemInStorageSlot(
	const int32 SourceRaidSlotIndex,
	const int32 TargetLobbySlotIndex) const
{
	if (!IsLobbyBackendCommandContext())
	{
		return false;
	}

	AFrontierPlayerState* PlayerState = GetFrontierPlayerState();
	UFrontierRaidInventoryComponent* RaidInventory = PlayerState ? PlayerState->GetRaidInventoryComponent() : nullptr;
	UFrontierStorageComponent* Storage = PlayerState ? PlayerState->GetStorageComponent() : nullptr;
	FFrontierInventorySlot SourceSlot;
	FFrontierInventorySlot TargetSlot;
	if (!RaidInventory
		|| !Storage
		|| TargetLobbySlotIndex < 0
		|| !RaidInventory->GetSlot(SourceRaidSlotIndex, SourceSlot)
		|| !Storage->GetSlot(TargetLobbySlotIndex, TargetSlot)
		|| !SourceSlot.bOccupied
		|| !SourceSlot.ItemInstance.IsValid()
		|| !SourceSlot.ItemInstance.ItemInstanceId.IsValid())
	{
		return false;
	}

	UFrontierBackendProtocolComponent* BackendProtocol = GetBackendProtocolComponent();
	if (TargetSlot.bOccupied && TargetSlot.ItemInstance.IsValid() && TargetSlot.ItemInstance.ItemInstanceId.IsValid())
	{
		return BackendProtocol
			&& BackendProtocol->RequestSwapRaidInventoryWithStorage(
				SourceRaidSlotIndex,
				TargetLobbySlotIndex,
				SourceSlot.ItemInstance.ItemInstanceId,
				TargetSlot.ItemInstance.ItemInstanceId);
	}

	return BackendProtocol
		&& BackendProtocol->RequestDepositStorageItem(
			SourceSlot.ItemInstance.ItemInstanceId,
			TOptional<int32>(TargetLobbySlotIndex));
}

bool UFrontierInventoryCommandService::WithdrawLobbyItemToRaidInventory(const int32 SourceLobbySlotIndex) const
{
	if (!IsLobbyBackendCommandContext())
	{
		return false;
	}

	AFrontierPlayerState* PlayerState = GetFrontierPlayerState();
	UFrontierStorageComponent* Storage = PlayerState ? PlayerState->GetStorageComponent() : nullptr;
	FFrontierInventorySlot SourceSlot;
	if (!Storage
		|| !Storage->GetSlot(SourceLobbySlotIndex, SourceSlot)
		|| !SourceSlot.bOccupied
		|| !SourceSlot.ItemInstance.IsValid()
		|| !SourceSlot.ItemInstance.ItemInstanceId.IsValid())
	{
		return false;
	}

	UFrontierBackendProtocolComponent* BackendProtocol = GetBackendProtocolComponent();
	return BackendProtocol
		&& BackendProtocol->RequestWithdrawStorageItem(SourceSlot.ItemInstance.ItemInstanceId);
}

bool UFrontierInventoryCommandService::WithdrawLobbyItemToRaidInventorySlot(
	const int32 SourceLobbySlotIndex,
	const int32 TargetRaidSlotIndex) const
{
	if (!IsLobbyBackendCommandContext())
	{
		return false;
	}

	AFrontierPlayerState* PlayerState = GetFrontierPlayerState();
	UFrontierStorageComponent* Storage = PlayerState ? PlayerState->GetStorageComponent() : nullptr;
	UFrontierRaidInventoryComponent* RaidInventory = PlayerState ? PlayerState->GetRaidInventoryComponent() : nullptr;
	FFrontierInventorySlot SourceSlot;
	FFrontierInventorySlot TargetSlot;
	if (!Storage
		|| !RaidInventory
		|| TargetRaidSlotIndex < 0
		|| !Storage->GetSlot(SourceLobbySlotIndex, SourceSlot)
		|| !RaidInventory->GetSlot(TargetRaidSlotIndex, TargetSlot)
		|| !SourceSlot.bOccupied
		|| !SourceSlot.ItemInstance.IsValid()
		|| !SourceSlot.ItemInstance.ItemInstanceId.IsValid())
	{
		return false;
	}

	UFrontierBackendProtocolComponent* BackendProtocol = GetBackendProtocolComponent();
	if (TargetSlot.bOccupied && TargetSlot.ItemInstance.IsValid() && TargetSlot.ItemInstance.ItemInstanceId.IsValid())
	{
		return BackendProtocol
			&& BackendProtocol->RequestSwapStorageWithRaidInventory(
				SourceLobbySlotIndex,
				TargetRaidSlotIndex,
				SourceSlot.ItemInstance.ItemInstanceId,
				TargetSlot.ItemInstance.ItemInstanceId);
	}

	return BackendProtocol
		&& BackendProtocol->RequestWithdrawStorageItem(
			SourceSlot.ItemInstance.ItemInstanceId,
			TOptional<int32>(TargetRaidSlotIndex));
}

bool UFrontierInventoryCommandService::DropRaidInventoryItem(const int32 SourceSlotIndex) const
{
	AFrontierPlayerState* PlayerState = GetFrontierPlayerState();
	UFrontierRaidInventoryComponent* RaidInventory = PlayerState ? PlayerState->GetRaidInventoryComponent() : nullptr;
	FFrontierInventorySlot SourceSlot;
	if (!RaidInventory
		|| !RaidInventory->GetSlot(SourceSlotIndex, SourceSlot)
		|| !SourceSlot.bOccupied
		|| !SourceSlot.ItemInstance.IsValid()
		|| !SpawnDroppedItem(SourceSlot.ItemInstance))
	{
		return false;
	}

	return RaidInventory->ClearSlot(SourceSlotIndex);
}

bool UFrontierInventoryCommandService::DropLoadoutItem(const EFrontierEquipmentSlot SlotType) const
{
	AFrontierPlayerState* PlayerState = GetFrontierPlayerState();
	UFrontierLoadoutComponent* Loadout = PlayerState ? PlayerState->GetLoadoutComponent() : nullptr;
	FFrontierLoadoutSlot LoadoutSlot;
	if (!Loadout
		|| SlotType == EFrontierEquipmentSlot::None
		|| !Loadout->FindLoadoutSlot(SlotType, LoadoutSlot)
		|| !LoadoutSlot.bOccupied
		|| !LoadoutSlot.ItemInstance.IsValid()
		|| !SpawnDroppedItem(LoadoutSlot.ItemInstance))
	{
		return false;
	}

	return Loadout->UnequipItem(SlotType);
}

bool UFrontierInventoryCommandService::LootContainerItem(
	AFrontierLootContainerActor* LootContainer,
	const int32 SourceSlotIndex) const
{
	AFrontierPlayerController* Controller = GetAuthorityFrontierController();
	return Controller && LootContainer && LootContainer->LootItemToPlayer(Controller, SourceSlotIndex);
}

bool UFrontierInventoryCommandService::LootContainerItemToRaidSlot(
	AFrontierLootContainerActor* LootContainer,
	const int32 SourceLootSlotIndex,
	const int32 TargetRaidSlotIndex) const
{
	AFrontierPlayerController* Controller = GetAuthorityFrontierController();
	return Controller && LootContainer
		&& LootContainer->LootItemToPlayerSlot(Controller, SourceLootSlotIndex, TargetRaidSlotIndex);
}

bool UFrontierInventoryCommandService::LootContainerLoadoutItem(
	AFrontierLootContainerActor* LootContainer,
	const EFrontierEquipmentSlot SlotType) const
{
	AFrontierPlayerController* Controller = GetAuthorityFrontierController();
	return Controller && LootContainer
		&& LootContainer->LootDeadPlayerLoadoutItemToPlayer(Controller, SlotType);
}

bool UFrontierInventoryCommandService::StoreRaidItemInLootContainer(
	AFrontierLootContainerActor* LootContainer,
	const int32 SourceRaidSlotIndex) const
{
	AFrontierPlayerController* Controller = GetAuthorityFrontierController();
	return Controller && LootContainer && LootContainer->StorePlayerItem(Controller, SourceRaidSlotIndex);
}

bool UFrontierInventoryCommandService::StoreRaidItemInLootContainerSlot(
	AFrontierLootContainerActor* LootContainer,
	const int32 SourceRaidSlotIndex,
	const int32 TargetLootSlotIndex) const
{
	AFrontierPlayerController* Controller = GetAuthorityFrontierController();
	return Controller && LootContainer
		&& LootContainer->StorePlayerItemAtSlot(Controller, SourceRaidSlotIndex, TargetLootSlotIndex);
}

bool UFrontierInventoryCommandService::SwapLootContainerSlots(
	AFrontierLootContainerActor* LootContainer,
	const int32 SourceLootSlotIndex,
	const int32 TargetLootSlotIndex) const
{
	AFrontierPlayerController* Controller = GetAuthorityFrontierController();
	return Controller && LootContainer
		&& LootContainer->SwapLootSlotsForPlayer(Controller, SourceLootSlotIndex, TargetLootSlotIndex);
}

APlayerController* UFrontierInventoryCommandService::GetAuthorityPlayerController() const
{
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	return Controller && Controller->HasAuthority() ? Controller : nullptr;
}

bool UFrontierInventoryCommandService::IsLobbyBackendCommandContext() const
{
	return Cast<AFrontierLobbyPlayerController>(GetAuthorityPlayerController()) != nullptr;
}

AFrontierPlayerController* UFrontierInventoryCommandService::GetAuthorityFrontierController() const
{
	APlayerController* Controller = GetAuthorityPlayerController();
	return Cast<AFrontierPlayerController>(Controller);
}

AFrontierPlayerState* UFrontierInventoryCommandService::GetFrontierPlayerState() const
{
	APlayerController* Controller = GetAuthorityPlayerController();
	return Controller ? Controller->GetPlayerState<AFrontierPlayerState>() : nullptr;
}

UFrontierBackendProtocolComponent* UFrontierInventoryCommandService::GetBackendProtocolComponent() const
{
	APlayerController* Controller = GetAuthorityPlayerController();
	return Controller ? Controller->FindComponentByClass<UFrontierBackendProtocolComponent>() : nullptr;
}

bool UFrontierInventoryCommandService::SpawnDroppedItem(const FFrontierItemInstance& ItemInstance) const
{
	APlayerController* Controller = GetAuthorityPlayerController();
	APawn* ControlledPawn = Controller ? Controller->GetPawn() : nullptr;
	UWorld* World = Controller ? Controller->GetWorld() : nullptr;
	if (!ControlledPawn || !World || !ItemInstance.IsValid())
	{
		return false;
	}

	const FVector Forward = ControlledPawn->GetActorForwardVector().GetSafeNormal();
	const FVector DropSpawnLocation = ControlledPawn->GetActorLocation() + Forward * 120.0f + FVector(0.0f, 0.0f, 30.0f);
	const FTransform SpawnTransform(ControlledPawn->GetActorRotation(), DropSpawnLocation);
	AFrontierDroppedItemActor* DroppedActor = World->SpawnActorDeferred<AFrontierDroppedItemActor>(
		AFrontierDroppedItemActor::StaticClass(),
		SpawnTransform,
		nullptr,
		ControlledPawn,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!DroppedActor)
	{
		return false;
	}

	DroppedActor->InitializeDroppedItem(ItemInstance);
	UGameplayStatics::FinishSpawningActor(DroppedActor, SpawnTransform);
	return true;
}
