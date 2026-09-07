#include "Components/FrontierLoadoutComponent.h"

#include "Components/FrontierInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Progression/FrontierUpgradeBalanceSubsystem.h"

namespace
{
	void ForceLoadoutOwnerNetUpdate(const UActorComponent* Component)
	{
		AActor* OwnerActor = Component ? Component->GetOwner() : nullptr;
		if (OwnerActor && OwnerActor->HasAuthority())
		{
			OwnerActor->ForceNetUpdate();
		}
	}
}

UFrontierLoadoutComponent::UFrontierLoadoutComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UFrontierLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Full item instances are private. Remote weapon visuals replicate through EquipmentComponent weapon actors.
	DOREPLIFETIME_CONDITION(UFrontierLoadoutComponent, LoadoutSlots, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UFrontierLoadoutComponent, TotalEquipmentScore, COND_OwnerOnly);
}

bool UFrontierLoadoutComponent::EquipItem(const EFrontierEquipmentSlot SlotType, const FFrontierItemInstance& ItemInstance)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || SlotType == EFrontierEquipmentSlot::None || !ItemInstance.IsValid() || !CanEquipItemToSlot(SlotType, ItemInstance))
	{
		return false;
	}

	if (FFrontierLoadoutSlot* Slot = FindMutableLoadoutSlotPtr(SlotType))
	{
		FFrontierItemInstance MutableItemInstance = ItemInstance;
		MutableItemInstance.EnsureRuntimeIdentity();
		Slot->bOccupied = true;
		Slot->ItemInstance = MutableItemInstance;
		BroadcastLoadoutChanged();
		return true;
	}

	return false;
}

bool UFrontierLoadoutComponent::EquipItemFromInventory(UFrontierInventoryComponent* SourceInventory, const int32 SourceSlotIndex, EFrontierEquipmentSlot RequestedSlot)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !SourceInventory)
	{
		return false;
	}

	FFrontierInventorySlot SourceSlot;
	if (!SourceInventory->GetSlot(SourceSlotIndex, SourceSlot) || !SourceSlot.bOccupied || !SourceSlot.ItemInstance.IsValid())
	{
		return false;
	}

	if (RequestedSlot == EFrontierEquipmentSlot::None && !ResolveEquipSlotForItem(SourceSlot.ItemInstance, RequestedSlot))
	{
		return false;
	}

	if (!CanEquipItemToSlot(RequestedSlot, SourceSlot.ItemInstance))
	{
		return false;
	}

	FFrontierLoadoutSlot* TargetSlot = FindMutableLoadoutSlotPtr(RequestedSlot);
	if (!TargetSlot)
	{
		return false;
	}

	const bool bHadEquippedItem = TargetSlot->bOccupied && TargetSlot->ItemInstance.IsValid();
	const FFrontierItemInstance PreviouslyEquippedItem = TargetSlot->ItemInstance;

	if (bHadEquippedItem)
	{
		if (!SourceInventory->SetItemAtSlot(SourceSlotIndex, PreviouslyEquippedItem))
		{
			return false;
		}
	}
	else if (!SourceInventory->ClearSlot(SourceSlotIndex))
	{
		return false;
	}

	TargetSlot->bOccupied = true;
	TargetSlot->ItemInstance = SourceSlot.ItemInstance;
	BroadcastLoadoutChanged();
	return true;
}

bool UFrontierLoadoutComponent::UnequipItem(const EFrontierEquipmentSlot SlotType)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || SlotType == EFrontierEquipmentSlot::None)
	{
		return false;
	}

	for (FFrontierLoadoutSlot& Slot : LoadoutSlots)
	{
		if (Slot.SlotType != SlotType)
		{
			continue;
		}

		Slot.bOccupied = false;
		Slot.ItemInstance = FFrontierItemInstance();
		BroadcastLoadoutChanged();
		return true;
	}

	return false;
}

bool UFrontierLoadoutComponent::UnequipItemToInventory(UFrontierInventoryComponent* TargetInventory, const EFrontierEquipmentSlot SlotType)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !TargetInventory || SlotType == EFrontierEquipmentSlot::None)
	{
		return false;
	}

	FFrontierLoadoutSlot* Slot = FindMutableLoadoutSlotPtr(SlotType);
	if (!Slot || !Slot->bOccupied || !Slot->ItemInstance.IsValid())
	{
		return false;
	}

	const FFrontierItemInstance EquippedItem = Slot->ItemInstance;
	if (!TargetInventory->AddItem(EquippedItem))
	{
		return false;
	}

	Slot->bOccupied = false;
	Slot->ItemInstance = FFrontierItemInstance();
	BroadcastLoadoutChanged();
	return true;
}

bool UFrontierLoadoutComponent::UnequipItemToInventorySlot(UFrontierInventoryComponent* TargetInventory, const EFrontierEquipmentSlot SlotType, const int32 TargetSlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !TargetInventory || SlotType == EFrontierEquipmentSlot::None)
	{
		return false;
	}

	FFrontierLoadoutSlot* LoadoutSlot = FindMutableLoadoutSlotPtr(SlotType);
	if (!LoadoutSlot || !LoadoutSlot->bOccupied || !LoadoutSlot->ItemInstance.IsValid())
	{
		return false;
	}

	FFrontierInventorySlot TargetInventorySlot;
	if (!TargetInventory->GetSlot(TargetSlotIndex, TargetInventorySlot))
	{
		return false;
	}

	const FFrontierItemInstance EquippedItem = LoadoutSlot->ItemInstance;

	if (TargetInventorySlot.bOccupied && TargetInventorySlot.ItemInstance.IsValid())
	{
		if (!CanEquipItemToSlot(SlotType, TargetInventorySlot.ItemInstance))
		{
			return false;
		}

		if (!TargetInventory->SetItemAtSlot(TargetSlotIndex, EquippedItem))
		{
			return false;
		}

		LoadoutSlot->ItemInstance = TargetInventorySlot.ItemInstance;
		LoadoutSlot->bOccupied = true;
		BroadcastLoadoutChanged();
		return true;
	}

	if (!TargetInventory->SetItemAtSlot(TargetSlotIndex, EquippedItem))
	{
		return false;
	}

	LoadoutSlot->bOccupied = false;
	LoadoutSlot->ItemInstance = FFrontierItemInstance();
	BroadcastLoadoutChanged();
	return true;
}

const TArray<FFrontierLoadoutSlot>& UFrontierLoadoutComponent::GetLoadoutSlots() const
{
	return LoadoutSlots;
}

float UFrontierLoadoutComponent::GetTotalEquipmentScore() const
{
	return TotalEquipmentScore;
}

bool UFrontierLoadoutComponent::FindLoadoutSlot(const EFrontierEquipmentSlot SlotType, FFrontierLoadoutSlot& OutSlot) const
{
	if (const FFrontierLoadoutSlot* Slot = FindLoadoutSlotPtr(SlotType))
	{
		OutSlot = *Slot;
		return true;
	}

	return false;
}

bool UFrontierLoadoutComponent::CanEquipItemToSlot(const EFrontierEquipmentSlot SlotType, const FFrontierItemInstance& ItemInstance) const
{
	if (SlotType == EFrontierEquipmentSlot::None || !ItemInstance.IsValid())
	{
		return false;
	}

	switch (ItemInstance.GetCategory())
	{
	case EFrontierItemCategory::Weapon:
	case EFrontierItemCategory::Armor:
	case EFrontierItemCategory::Accessory:
		return ItemInstance.GetEquipSlot() == SlotType;
	case EFrontierItemCategory::Consumable:
		return ItemInstance.GetAllowedEquipSlots().Contains(SlotType);
	default:
		return false;
	}
}

bool UFrontierLoadoutComponent::ResolveEquipSlotForItem(const FFrontierItemInstance& ItemInstance, EFrontierEquipmentSlot& OutSlotType) const
{
	OutSlotType = EFrontierEquipmentSlot::None;

	if (!ItemInstance.IsValid())
	{
		return false;
	}

	if (ItemInstance.GetCategory() == EFrontierItemCategory::Weapon
		|| ItemInstance.GetCategory() == EFrontierItemCategory::Armor
		|| ItemInstance.GetCategory() == EFrontierItemCategory::Accessory)
	{
		OutSlotType = ItemInstance.GetEquipSlot();
		return OutSlotType != EFrontierEquipmentSlot::None;
	}

	if (ItemInstance.GetCategory() == EFrontierItemCategory::Consumable)
	{
		const TArray<EFrontierEquipmentSlot>& AllowedEquipSlots = ItemInstance.GetAllowedEquipSlots();
		for (const EFrontierEquipmentSlot AllowedSlot : AllowedEquipSlots)
		{
			if (const FFrontierLoadoutSlot* ExistingSlot = FindLoadoutSlotPtr(AllowedSlot); ExistingSlot && !ExistingSlot->bOccupied)
			{
				OutSlotType = AllowedSlot;
				return true;
			}
		}

		if (!AllowedEquipSlots.IsEmpty())
		{
			OutSlotType = AllowedEquipSlots[0];
			return true;
		}
	}

	return false;
}

void UFrontierLoadoutComponent::SetLoadoutSlotsFromSnapshot(const TArray<FFrontierLoadoutSlot>& InSlots)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	InitializeDefaultSlots();

	TMap<EFrontierEquipmentSlot, FFrontierLoadoutSlot> SavedSlotsByType;
	for (const FFrontierLoadoutSlot& Slot : InSlots)
	{
		if (Slot.SlotType != EFrontierEquipmentSlot::None)
		{
			SavedSlotsByType.Add(Slot.SlotType, Slot);
		}
	}

	for (FFrontierLoadoutSlot& Slot : LoadoutSlots)
	{
		if (const FFrontierLoadoutSlot* SavedSlot = SavedSlotsByType.Find(Slot.SlotType))
		{
			Slot = *SavedSlot;
			if (!Slot.bOccupied || !Slot.ItemInstance.IsValid())
			{
				Slot.bOccupied = false;
				Slot.ItemInstance = FFrontierItemInstance();
				continue;
			}

			Slot.ItemInstance.EnsureRuntimeIdentity();
		}
		else
		{
			Slot.bOccupied = false;
			Slot.ItemInstance = FFrontierItemInstance();
		}
	}

	BroadcastLoadoutChanged();
}

const FFrontierLoadoutSlot* UFrontierLoadoutComponent::FindLoadoutSlotPtr(const EFrontierEquipmentSlot SlotType) const
{
	for (const FFrontierLoadoutSlot& Slot : LoadoutSlots)
	{
		if (Slot.SlotType == SlotType)
		{
			return &Slot;
		}
	}

	return nullptr;
}

FFrontierLoadoutSlot* UFrontierLoadoutComponent::FindMutableLoadoutSlotPtr(const EFrontierEquipmentSlot SlotType)
{
	for (FFrontierLoadoutSlot& Slot : LoadoutSlots)
	{
		if (Slot.SlotType == SlotType)
		{
			return &Slot;
		}
	}

	return nullptr;
}

void UFrontierLoadoutComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeDefaultSlots();
}

void UFrontierLoadoutComponent::OnRep_LoadoutSlots()
{
	BroadcastLoadoutChanged();
}

void UFrontierLoadoutComponent::InitializeDefaultSlots()
{
	if (!LoadoutSlots.IsEmpty())
	{
		return;
	}

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

	for (const EFrontierEquipmentSlot SlotType : DefaultSlots)
	{
		FFrontierLoadoutSlot Slot;
		Slot.SlotType = SlotType;
		LoadoutSlots.Add(Slot);
	}
}

void UFrontierLoadoutComponent::BroadcastLoadoutChanged()
{
	RecalculateTotalEquipmentScore();
	ForceLoadoutOwnerNetUpdate(this);
	OnLoadoutChanged.Broadcast(LoadoutSlots);
}

void UFrontierLoadoutComponent::RecalculateTotalEquipmentScore()
{
	TotalEquipmentScore = 0.0f;

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFrontierUpgradeBalanceSubsystem* UpgradeBalance = GameInstance
		? GameInstance->GetSubsystem<UFrontierUpgradeBalanceSubsystem>()
		: nullptr;

	for (FFrontierLoadoutSlot& Slot : LoadoutSlots)
	{
		if (!Slot.bOccupied || !Slot.ItemInstance.IsValid())
		{
			continue;
		}

		if (UpgradeBalance)
		{
			UpgradeBalance->RecalculateEquipmentScore(Slot.ItemInstance);
		}

		TotalEquipmentScore += FMath::Max(0.0f, Slot.ItemInstance.EquipmentScore);
	}
}
