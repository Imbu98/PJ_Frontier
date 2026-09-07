#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "FrontierInventoryCommandService.generated.h"

class AFrontierLootContainerActor;
class AFrontierPlayerController;
class AFrontierPlayerState;
class APlayerController;
class UFrontierBackendProtocolComponent;

/**
 * Server-side inventory command handler. PlayerController keeps RPC ownership and forwards validated commands here.
 */
UCLASS(ClassGroup=(Frontier))
class FRONTIER_API UFrontierInventoryCommandService : public UActorComponent
{
	GENERATED_BODY()

public:
	UFrontierInventoryCommandService();

	bool EquipRaidInventoryItemToLoadout(int32 SourceSlotIndex, EFrontierEquipmentSlot SlotType) const;
	bool UnequipLoadoutItemToRaidInventory(EFrontierEquipmentSlot SlotType) const;
	bool UnequipLoadoutItemToRaidInventorySlot(EFrontierEquipmentSlot SlotType, int32 TargetSlotIndex) const;
	bool SwapRaidInventorySlots(int32 SourceSlotIndex, int32 TargetSlotIndex) const;
	bool SwapStorageSlots(int32 SourceSlotIndex, int32 TargetSlotIndex) const;
	bool UseRaidInventoryItem(int32 SourceSlotIndex) const;
	bool RegisterQuickSlotFromRaidInventory(int32 QuickSlotIndex, int32 InventorySlotIndex) const;
	bool ClearQuickSlot(int32 QuickSlotIndex) const;
	bool MoveOrSwapQuickSlots(int32 SourceQuickSlotIndex, int32 TargetQuickSlotIndex) const;
	bool UseQuickSlot(int32 QuickSlotIndex) const;
	bool StoreRaidItemInStorage(int32 SourceRaidSlotIndex) const;
	bool StoreRaidItemInStorageSlot(int32 SourceRaidSlotIndex, int32 TargetLobbySlotIndex) const;
	bool WithdrawLobbyItemToRaidInventory(int32 SourceLobbySlotIndex) const;
	bool WithdrawLobbyItemToRaidInventorySlot(int32 SourceLobbySlotIndex, int32 TargetRaidSlotIndex) const;
	bool DropRaidInventoryItem(int32 SourceSlotIndex) const;
	bool DropLoadoutItem(EFrontierEquipmentSlot SlotType) const;
	bool LootContainerItem(AFrontierLootContainerActor* LootContainer, int32 SourceSlotIndex) const;
	bool LootContainerItemToRaidSlot(AFrontierLootContainerActor* LootContainer, int32 SourceLootSlotIndex, int32 TargetRaidSlotIndex) const;
	bool LootContainerLoadoutItem(AFrontierLootContainerActor* LootContainer, EFrontierEquipmentSlot SlotType) const;
	bool StoreRaidItemInLootContainer(AFrontierLootContainerActor* LootContainer, int32 SourceRaidSlotIndex) const;
	bool StoreRaidItemInLootContainerSlot(AFrontierLootContainerActor* LootContainer, int32 SourceRaidSlotIndex, int32 TargetLootSlotIndex) const;
	bool SwapLootContainerSlots(AFrontierLootContainerActor* LootContainer, int32 SourceLootSlotIndex, int32 TargetLootSlotIndex) const;
private:
	bool IsLobbyBackendCommandContext() const;
	APlayerController* GetAuthorityPlayerController() const;
	AFrontierPlayerController* GetAuthorityFrontierController() const;
	AFrontierPlayerState* GetFrontierPlayerState() const;
	UFrontierBackendProtocolComponent* GetBackendProtocolComponent() const;
	bool SpawnDroppedItem(const FFrontierItemInstance& ItemInstance) const;

};
