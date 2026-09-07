#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/FrontierInventoryComponent.h"
#include "Inventory/Items/FrontierItemCatalogSubsystem.h"
#include "Inventory/Items/FrontierItemDataAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierInventoryFastArrayStateTest,
	"Frontier.Inventory.Replication.FastArrayState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierInventoryFastArrayStateTest::RunTest(const FString& Parameters)
{
	UFrontierItemDataAsset* TestItemData = NewObject<UFrontierItemDataAsset>();
	TestNotNull(TEXT("Transient item data is available"), TestItemData);
	if (!TestItemData)
	{
		return false;
	}

	TestItemData->ItemId = TEXT("Test.FastArray.Item");

	TArray<FFrontierInventorySlot> SourceSlots;
	SourceSlots.SetNum(4);
	for (int32 SlotIndex = 0; SlotIndex < SourceSlots.Num(); ++SlotIndex)
	{
		SourceSlots[SlotIndex].SlotIndex = SlotIndex;
	}

	FFrontierInventorySlot& OccupiedSlot = SourceSlots[1];
	OccupiedSlot.bOccupied = true;
	OccupiedSlot.ItemInstance.ItemTemplateData = UFrontierItemCatalogSubsystem::BuildTemplateDataFromItemData(*TestItemData);
	OccupiedSlot.ItemInstance.Quantity = 3;

	FFrontierReplicatedInventoryList ReplicatedList;
	ReplicatedList.RebuildFromSlots(SourceSlots);

	TestEqual(TEXT("Only occupied slots are stored in the Fast Array"), ReplicatedList.Items.Num(), 1);
	TestEqual(TEXT("Occupied slot index is preserved"), ReplicatedList.Items[0].Slot.SlotIndex, 1);
	TestEqual(TEXT("Initial quantity is preserved"), ReplicatedList.Items[0].Slot.ItemInstance.Quantity, 3);

	SourceSlots[1].ItemInstance.Quantity = 7;
	const int32 UpdatedIndices[] = { 1 };
	ReplicatedList.UpdateSlots(SourceSlots, UpdatedIndices);

	TestEqual(TEXT("Updating a slot does not duplicate its entry"), ReplicatedList.Items.Num(), 1);
	TestEqual(TEXT("Changed quantity is synchronized"), ReplicatedList.Items[0].Slot.ItemInstance.Quantity, 7);

	SourceSlots[1] = FFrontierInventorySlot();
	SourceSlots[1].SlotIndex = 1;
	ReplicatedList.UpdateSlots(SourceSlots, UpdatedIndices);

	TestEqual(TEXT("Clearing a slot removes its sparse entry"), ReplicatedList.Items.Num(), 0);
	return true;
}

#endif
