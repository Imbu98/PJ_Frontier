#include "Components/FrontierQuickSlotComponent.h"

#include "Components/FrontierLoadoutComponent.h"
#include "Components/FrontierRaidInventoryComponent.h"
#include "Game/FrontierPlayerState.h"
#include "Net/UnrealNetwork.h"

UFrontierQuickSlotComponent::UFrontierQuickSlotComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
	InitializeQuickSlots();
}

void UFrontierQuickSlotComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeQuickSlots();
}

void UFrontierQuickSlotComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UFrontierQuickSlotComponent, QuickSlots, COND_OwnerOnly);
}

bool UFrontierQuickSlotComponent::GetQuickSlot(const int32 SlotIndex, FFrontierQuickSlotEntry& OutEntry) const
{
	if (!QuickSlots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	OutEntry = QuickSlots[SlotIndex];
	return true;
}

bool UFrontierQuickSlotComponent::IsQuickSlotOccupied(const int32 SlotIndex) const
{
	return QuickSlots.IsValidIndex(SlotIndex) && QuickSlots[SlotIndex].bOccupied;
}

bool UFrontierQuickSlotComponent::SetQuickSlot(const int32 SlotIndex, const FFrontierQuickSlotEntry& Entry)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !QuickSlots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	FFrontierQuickSlotEntry NewEntry = Entry;
	NewEntry.SlotIndex = SlotIndex;
	QuickSlots[SlotIndex] = NewEntry;
	BroadcastQuickSlotsChanged();
	GetOwner()->ForceNetUpdate();
	return true;
}

bool UFrontierQuickSlotComponent::ClearQuickSlot(const int32 SlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !QuickSlots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	if (!QuickSlots[SlotIndex].bOccupied)
	{
		return true;
	}

	QuickSlots[SlotIndex].Reset(SlotIndex);
	BroadcastQuickSlotsChanged();
	GetOwner()->ForceNetUpdate();
	return true;
}

int32 UFrontierQuickSlotComponent::ClearQuickSlotsMatchingItem(const FFrontierItemInstance& ItemInstance)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !ItemInstance.IsValid())
	{
		return 0;
	}

	int32 ClearedCount = 0;
	for (int32 SlotIndex = 0; SlotIndex < QuickSlots.Num(); ++SlotIndex)
	{
		if (QuickSlots[SlotIndex].bOccupied && QuickSlots[SlotIndex].MatchesItem(ItemInstance))
		{
			QuickSlots[SlotIndex].Reset(SlotIndex);
			++ClearedCount;
		}
	}

	if (ClearedCount > 0)
	{
		BroadcastQuickSlotsChanged();
		GetOwner()->ForceNetUpdate();
	}

	return ClearedCount;
}

bool UFrontierQuickSlotComponent::MoveOrSwapQuickSlots(
	const int32 SourceSlotIndex,
	const int32 TargetSlotIndex)
{
	if (!GetOwner()
		|| !GetOwner()->HasAuthority()
		|| !QuickSlots.IsValidIndex(SourceSlotIndex)
		|| !QuickSlots.IsValidIndex(TargetSlotIndex)
		|| SourceSlotIndex == TargetSlotIndex
		|| !QuickSlots[SourceSlotIndex].bOccupied)
	{
		return false;
	}

	QuickSlots.Swap(SourceSlotIndex, TargetSlotIndex);
	QuickSlots[SourceSlotIndex].SlotIndex = SourceSlotIndex;
	QuickSlots[TargetSlotIndex].SlotIndex = TargetSlotIndex;
	BroadcastQuickSlotsChanged();
	GetOwner()->ForceNetUpdate();
	return true;
}

bool UFrontierQuickSlotComponent::ResolveItemForQuickSlot(
	const int32 SlotIndex,
	FFrontierItemInstance& OutItem) const
{
	OutItem = FFrontierItemInstance();
	if (!QuickSlots.IsValidIndex(SlotIndex) || !QuickSlots[SlotIndex].bOccupied)
	{
		return false;
	}

	const AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(GetOwner());
	const UFrontierRaidInventoryComponent* RaidInventory = PlayerState
		? PlayerState->GetRaidInventoryComponent()
		: nullptr;
	const UFrontierLoadoutComponent* Loadout = PlayerState
		? PlayerState->GetLoadoutComponent()
		: nullptr;
	const FFrontierQuickSlotEntry& Entry = QuickSlots[SlotIndex];

	if (RaidInventory)
	{
		for (const FFrontierInventorySlot& InventorySlot : RaidInventory->GetSlots())
		{
			if (InventorySlot.bOccupied
				&& Entry.MatchesItem(InventorySlot.ItemInstance))
			{
				OutItem = InventorySlot.ItemInstance;
				return true;
			}
		}
	}

	if (Loadout)
	{
		for (const FFrontierLoadoutSlot& LoadoutSlot : Loadout->GetLoadoutSlots())
		{
			if (LoadoutSlot.bOccupied
				&& Entry.MatchesItem(LoadoutSlot.ItemInstance))
			{
				OutItem = LoadoutSlot.ItemInstance;
				return true;
			}
		}
	}

	return false;
}

void UFrontierQuickSlotComponent::OnRep_QuickSlots()
{
	BroadcastQuickSlotsChanged();
}

void UFrontierQuickSlotComponent::InitializeQuickSlots()
{
	QuickSlots.SetNum(QuickSlotCount);
	for (int32 Index = 0; Index < QuickSlots.Num(); ++Index)
	{
		if (QuickSlots[Index].SlotIndex != Index)
		{
			QuickSlots[Index].Reset(Index);
		}
	}
}

void UFrontierQuickSlotComponent::BroadcastQuickSlotsChanged()
{
	OnQuickSlotsChanged.Broadcast();
}
