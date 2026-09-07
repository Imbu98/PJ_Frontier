#pragma once

#include "CoreMinimal.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Equipment/FrontierOnlineEquipmentTypes.h"
#include "Inventory/FrontierOnlineInventoryTypes.h"
#include "Storage/FrontierOnlineStorageTypes.h"

class UFrontierItemCatalogSubsystem;

struct FRONTIER_API FFrontierBackendInventoryMapper
{
	static bool ConvertBackendEquipmentSlot(const FString& BackendSlot, EFrontierEquipmentSlot& OutSlot, FString& OutError);
	static bool ConvertEquipmentSlotToBackend(EFrontierEquipmentSlot Slot, FString& OutBackendSlot, FString& OutError);

	static bool ConvertBackendInventoryItemToRuntime(
		const FFrontierOnlineItemDTO& BackendItem,
		const UFrontierItemCatalogSubsystem& ItemCatalog,
		FFrontierItemInstance& OutRuntimeItem,
		FString& OutError);

	static bool ConvertBackendEquipmentItemToRuntime(
		const FFrontierOnlineItemDTO& BackendItem,
		const UFrontierItemCatalogSubsystem& ItemCatalog,
		FFrontierItemInstance& OutRuntimeItem,
		FString& OutError);

	static bool BuildRuntimeSlotsFromBackendInventory(
		const FFrontierOnlineInventoryData& BackendInventory,
		const UFrontierItemCatalogSubsystem& ItemCatalog,
		TArray<FFrontierInventorySlot>& OutSlots,
		FString& OutError);

	static bool BuildRuntimeSlotsFromBackendStorage(
		const FFrontierOnlineStorageData& BackendStorage,
		const UFrontierItemCatalogSubsystem& ItemCatalog,
		TArray<FFrontierInventorySlot>& OutSlots,
		FString& OutError);

	static bool BuildRuntimeLoadoutFromBackendEquipment(
		const FFrontierOnlineEquipmentData& BackendEquipment,
		const UFrontierItemCatalogSubsystem& ItemCatalog,
		TArray<FFrontierLoadoutSlot>& OutSlots,
		FString& OutError);
};
