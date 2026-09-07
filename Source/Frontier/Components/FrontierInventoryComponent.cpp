#include "Components/FrontierInventoryComponent.h"

#include "AbilitySystem/Effects/FrontierHealingGameplayEffect.h"
#include "AbilitySystem/Effects/FrontierStaminaGameplayEffect.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Frontier.h"
#include "Game/FrontierPlayerState.h"
#include "GameplayEffect.h"
#include "Inventory/Items/FrontierConsumableItemDataAsset.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "Net/UnrealNetwork.h"
#include "Tags/FrontierGameplayTags.h"
#include "TimerManager.h"

void FFrontierReplicatedInventoryList::SetOwningInventory(UFrontierInventoryComponent* InOwningInventory)
{
	OwningInventory = InOwningInventory;
}

void FFrontierReplicatedInventoryList::RebuildFromSlots(const TArray<FFrontierInventorySlot>& SourceSlots)
{
	Items.Reset();
	MarkArrayDirty();

	for (const FFrontierInventorySlot& Slot : SourceSlots)
	{
		if (!Slot.bOccupied || !Slot.ItemInstance.IsValid())
		{
			continue;
		}

		FFrontierReplicatedInventorySlot& ReplicatedSlot = Items.AddDefaulted_GetRef();
		ReplicatedSlot.Slot = Slot;
		MarkItemDirty(ReplicatedSlot);
	}
}

void FFrontierReplicatedInventoryList::UpdateSlots(
	const TArray<FFrontierInventorySlot>& SourceSlots,
	const TConstArrayView<int32> ChangedSlotIndices)
{
	for (const int32 SlotIndex : ChangedSlotIndices)
	{
		if (!SourceSlots.IsValidIndex(SlotIndex))
		{
			continue;
		}

		const int32 ExistingIndex = Items.IndexOfByPredicate([SlotIndex](const FFrontierReplicatedInventorySlot& Entry)
		{
			return Entry.Slot.SlotIndex == SlotIndex;
		});

		const FFrontierInventorySlot& SourceSlot = SourceSlots[SlotIndex];
		if (!SourceSlot.bOccupied || !SourceSlot.ItemInstance.IsValid())
		{
			if (ExistingIndex != INDEX_NONE)
			{
				Items.RemoveAtSwap(ExistingIndex, EAllowShrinking::No);
				MarkArrayDirty();
			}
			continue;
		}

		if (ExistingIndex != INDEX_NONE)
		{
			FFrontierReplicatedInventorySlot& ExistingSlot = Items[ExistingIndex];
			ExistingSlot.Slot = SourceSlot;
			MarkItemDirty(ExistingSlot);
			continue;
		}

		FFrontierReplicatedInventorySlot& NewSlot = Items.AddDefaulted_GetRef();
		NewSlot.Slot = SourceSlot;
		MarkItemDirty(NewSlot);
	}
}

void FFrontierReplicatedInventoryList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (UFrontierInventoryComponent* Inventory = OwningInventory.Get())
	{
		Inventory->QueueReplicatedViewRefresh();
	}
}

namespace
{
	void ForceOwnerNetUpdate(const UActorComponent* Component)
	{
		AActor* OwnerActor = Component ? Component->GetOwner() : nullptr;
		if (OwnerActor && OwnerActor->HasAuthority())
		{
			OwnerActor->ForceNetUpdate();
		}
	}

	void ApplyHealthRestore(UAbilitySystemComponent* ASC, const float HealthDelta)
	{
		if (!ASC || HealthDelta <= 0.0f)
		{
			return;
		}

		FGameplayEffectSpecHandle HealSpecHandle = ASC->MakeOutgoingSpec(
			UFrontierHealingGameplayEffect::StaticClass(),
			1.0f,
			ASC->MakeEffectContext());

		if (!HealSpecHandle.IsValid() || !HealSpecHandle.Data.IsValid())
		{
			return;
		}

		HealSpecHandle.Data->SetSetByCallerMagnitude(FFrontierGameplayTags::Get().DataHealthDelta, HealthDelta);
		ASC->ApplyGameplayEffectSpecToSelf(*HealSpecHandle.Data.Get());
	}

	void ApplyStaminaRestore(UAbilitySystemComponent* ASC, const float StaminaDelta)
	{
		if (!ASC || StaminaDelta <= 0.0f)
		{
			return;
		}

		FGameplayEffectSpecHandle StaminaSpecHandle = ASC->MakeOutgoingSpec(
			UFrontierStaminaGameplayEffect::StaticClass(),
			1.0f,
			ASC->MakeEffectContext());

		if (!StaminaSpecHandle.IsValid() || !StaminaSpecHandle.Data.IsValid())
		{
			return;
		}

		StaminaSpecHandle.Data->SetSetByCallerMagnitude(FFrontierGameplayTags::Get().DataStaminaDelta, StaminaDelta);
		ASC->ApplyGameplayEffectSpecToSelf(*StaminaSpecHandle.Data.Get());
	}
}

UFrontierInventoryComponent::UFrontierInventoryComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
	ReplicatedSlots.SetOwningInventory(this);
}

void UFrontierInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UFrontierInventoryComponent, ReplicatedSlots, COND_Dynamic);
	DOREPLIFETIME_CONDITION(UFrontierInventoryComponent, ReplicatedSlotCapacity, COND_Dynamic);
}

void UFrontierInventoryComponent::GetReplicatedCustomConditionState(FCustomPropertyConditionState& OutActiveState) const
{
	Super::GetReplicatedCustomConditionState(OutActiveState);

	const ELifetimeCondition InventoryCondition = ShouldReplicateInventoryToOwnerOnly() ? COND_OwnerOnly : COND_None;
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierInventoryComponent, ReplicatedSlots, InventoryCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierInventoryComponent, ReplicatedSlotCapacity, InventoryCondition);
}

bool UFrontierInventoryComponent::AddItem(const FFrontierItemInstance& ItemInstance)
{
	FRONTIER_LOG(Log, TEXT("Inventory AddItem requested. Owner=%s TemplateId=%s Quantity=%d"), *GetNameSafe(GetOwner()), *ItemInstance.GetTemplateId().ToString(), ItemInstance.Quantity);

	if (!GetOwner() || !GetOwner()->HasAuthority() || !ItemInstance.IsValid() || !CanAcceptItem(ItemInstance))
	{
		return false;
	}

	FFrontierItemInstance MutableItemInstance = ItemInstance;
	MutableItemInstance.EnsureRuntimeIdentity();

	if (MutableItemInstance.IsStackable())
	{
		const int32 StackableSlotIndex = FindStackableSlotIndex(MutableItemInstance);
		if (Slots.IsValidIndex(StackableSlotIndex))
		{
			const int32 MaxStack = MutableItemInstance.GetMaxStack();
			Slots[StackableSlotIndex].ItemInstance.Quantity = FMath::Min(
				MaxStack,
				Slots[StackableSlotIndex].ItemInstance.Quantity + MutableItemInstance.Quantity);
			BroadcastInventorySlotChanged(StackableSlotIndex);
			return true;
		}
	}

	const int32 EmptySlotIndex = FindFirstEmptySlotIndex();
	if (!Slots.IsValidIndex(EmptySlotIndex))
	{
		return false;
	}

	FFrontierInventorySlot& Slot = Slots[EmptySlotIndex];
	Slot.SlotIndex = EmptySlotIndex;
	Slot.bOccupied = true;
	Slot.ItemInstance = MutableItemInstance;

	BroadcastInventorySlotChanged(EmptySlotIndex);
	return true;
}

bool UFrontierInventoryComponent::RemoveItemAtSlot(const int32 SlotIndex, const int32 Quantity)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Slots.IsValidIndex(SlotIndex) || Quantity <= 0)
	{
		return false;
	}

	FFrontierInventorySlot& Slot = Slots[SlotIndex];
	if (!Slot.bOccupied || !Slot.ItemInstance.IsValid())
	{
		return false;
	}

	Slot.ItemInstance.Quantity -= Quantity;
	if (Slot.ItemInstance.Quantity <= 0)
	{
		Slot = FFrontierInventorySlot();
		Slot.SlotIndex = SlotIndex;
	}

	BroadcastInventorySlotChanged(SlotIndex);
	return true;
}

bool UFrontierInventoryComponent::SwapSlots(const int32 SourceSlotIndex, const int32 TargetSlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Slots.IsValidIndex(SourceSlotIndex) || !Slots.IsValidIndex(TargetSlotIndex) || SourceSlotIndex == TargetSlotIndex)
	{
		return false;
	}

	Slots.Swap(SourceSlotIndex, TargetSlotIndex);
	Slots[SourceSlotIndex].SlotIndex = SourceSlotIndex;
	Slots[TargetSlotIndex].SlotIndex = TargetSlotIndex;
	BroadcastInventorySlotsChanged(SourceSlotIndex, TargetSlotIndex);
	return true;
}

bool UFrontierInventoryComponent::MoveItemTo(UFrontierInventoryComponent* TargetInventory, const int32 SourceSlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !TargetInventory || !Slots.IsValidIndex(SourceSlotIndex))
	{
		return false;
	}

	const FFrontierInventorySlot& SourceSlot = Slots[SourceSlotIndex];
	if (!SourceSlot.bOccupied || !SourceSlot.ItemInstance.IsValid())
	{
		return false;
	}

	if (!TargetInventory->AddItem(SourceSlot.ItemInstance))
	{
		return false;
	}

	return RemoveItemAtSlot(SourceSlotIndex, SourceSlot.ItemInstance.Quantity);
}

bool UFrontierInventoryComponent::MoveItemToSlot(const int32 SourceSlotIndex, UFrontierInventoryComponent* TargetInventory, const int32 TargetSlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !TargetInventory || !Slots.IsValidIndex(SourceSlotIndex))
	{
		return false;
	}

	if (TargetInventory == this)
	{
		if (!Slots.IsValidIndex(TargetSlotIndex))
		{
			return false;
		}

		return SwapSlots(SourceSlotIndex, TargetSlotIndex);
	}

	if (!TargetInventory->GetOwner() || !TargetInventory->GetOwner()->HasAuthority())
	{
		return false;
	}

	FFrontierInventorySlot SourceSlot;
	if (!GetSlot(SourceSlotIndex, SourceSlot) || !SourceSlot.bOccupied || !SourceSlot.ItemInstance.IsValid())
	{
		return false;
	}

	FFrontierInventorySlot TargetSlot;
	if (!TargetInventory->GetSlot(TargetSlotIndex, TargetSlot))
	{
		return false;
	}

	if (TargetSlot.bOccupied && TargetSlot.ItemInstance.IsValid())
	{
		if (!TargetInventory->SetItemAtSlot(TargetSlotIndex, SourceSlot.ItemInstance))
		{
			return false;
		}

		return SetItemAtSlot(SourceSlotIndex, TargetSlot.ItemInstance);
	}

	if (!TargetInventory->SetItemAtSlot(TargetSlotIndex, SourceSlot.ItemInstance))
	{
		return false;
	}

	return ClearSlot(SourceSlotIndex);
}

bool UFrontierInventoryComponent::UseItemAtSlot(const int32 SlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	FFrontierInventorySlot& Slot = Slots[SlotIndex];
	if (!Slot.bOccupied || !Slot.ItemInstance.IsValid())
	{
		return false;
	}

	if (Slot.ItemInstance.GetCategory() != EFrontierItemCategory::Consumable)
	{
		return false;
	}

	IAbilitySystemInterface* AbilitySystemOwner = Cast<IAbilitySystemInterface>(GetOwner());
	UAbilitySystemComponent* ASC = AbilitySystemOwner ? AbilitySystemOwner->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return false;
	}

	const float HealthRestoreAmount = Slot.ItemInstance.GetHealthRestoreAmount();
	const float StaminaRestoreAmount = Slot.ItemInstance.GetStaminaRestoreAmount();
	const float UseTime = Slot.ItemInstance.GetUseTime();
	const bool bHasDirectRestoreAmount = HealthRestoreAmount > 0.0f || StaminaRestoreAmount > 0.0f;
	const bool bHasConsumeEffect = !Slot.ItemInstance.GetConsumeEffectClass().IsNull();
	if (!Slot.ItemInstance.GetUseMontage().IsNull())
	{
		if (!bHasDirectRestoreAmount && !bHasConsumeEffect)
		{
			FRONTIER_LOG(Error, TEXT("Animated consumable is missing both restore amounts and a consume effect. Owner=%s Slot=%d Item=%s"),
				*GetNameSafe(GetOwner()),
				SlotIndex,
				*Slot.ItemInstance.GetTemplateId().ToString());
			return false;
		}

		AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(GetOwner());
		if (!PlayerCharacter)
		{
			const AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(GetOwner());
			PlayerCharacter = PlayerState ? Cast<AFrontierPlayerCharacter>(PlayerState->GetPawn()) : nullptr;
		}
		if (!PlayerCharacter || !PlayerCharacter->BeginConsumableUse(Slot.ItemInstance))
		{
			FRONTIER_LOG(Warning, TEXT("Animated consumable use rejected. Owner=%s Slot=%d Item=%s"),
				*GetNameSafe(GetOwner()),
				SlotIndex,
				*Slot.ItemInstance.GetTemplateId().ToString());
			return false;
		}

		return RemoveItemAtSlot(SlotIndex, 1);
	}

	if (bHasDirectRestoreAmount)
	{
		if (UseTime <= KINDA_SMALL_NUMBER)
		{
			ApplyHealthRestore(ASC, HealthRestoreAmount);
			ApplyStaminaRestore(ASC, StaminaRestoreAmount);
		}
		else
		{
			UWorld* World = GetWorld();
			if (!World)
			{
				return false;
			}

			constexpr float TickInterval = 0.2f;
			const int32 TickCount = FMath::Max(1, FMath::CeilToInt(UseTime / TickInterval));
			const float TickHealAmount = HealthRestoreAmount / static_cast<float>(TickCount);
			const float TickStaminaAmount = StaminaRestoreAmount / static_cast<float>(TickCount);
			const TWeakObjectPtr<UAbilitySystemComponent> WeakASC = ASC;

			for (int32 TickIndex = 0; TickIndex < TickCount; ++TickIndex)
			{
				FTimerDelegate HealDelegate;
				HealDelegate.BindLambda([WeakASC, TickHealAmount, TickStaminaAmount]()
				{
					if (UAbilitySystemComponent* ResolvedASC = WeakASC.Get())
					{
						ApplyHealthRestore(ResolvedASC, TickHealAmount);
						ApplyStaminaRestore(ResolvedASC, TickStaminaAmount);
					}
				});

				const float Delay = FMath::Min(UseTime, TickInterval * static_cast<float>(TickIndex + 1));
				FTimerHandle TimerHandle;
				World->GetTimerManager().SetTimer(TimerHandle, HealDelegate, Delay, false);
			}
		}

		return RemoveItemAtSlot(SlotIndex, 1);
	}

	TSubclassOf<UGameplayEffect> EffectClass = Slot.ItemInstance.GetConsumeEffectClass().LoadSynchronous();
	if (!EffectClass)
	{
		return false;
	}

	const UGameplayEffect* EffectCDO = EffectClass->GetDefaultObject<UGameplayEffect>();
	if (!EffectCDO)
	{
		return false;
	}

	const FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	ASC->ApplyGameplayEffectToSelf(EffectCDO, 1.0f, ContextHandle);
	return RemoveItemAtSlot(SlotIndex, 1);
}

const TArray<FFrontierInventorySlot>& UFrontierInventoryComponent::GetSlots() const
{
	return Slots;
}

bool UFrontierInventoryComponent::GetSlot(const int32 SlotIndex, FFrontierInventorySlot& OutSlot) const
{
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	OutSlot = Slots[SlotIndex];
	return true;
}

bool UFrontierInventoryComponent::SetItemAtSlot(const int32 SlotIndex, const FFrontierItemInstance& ItemInstance)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Slots.IsValidIndex(SlotIndex) || !ItemInstance.IsValid() || !CanAcceptItem(ItemInstance))
	{
		return false;
	}

	FFrontierItemInstance MutableItemInstance = ItemInstance;
	MutableItemInstance.EnsureRuntimeIdentity();

	FFrontierInventorySlot& Slot = Slots[SlotIndex];
	Slot.SlotIndex = SlotIndex;
	Slot.bOccupied = true;
	Slot.ItemInstance = MutableItemInstance;
	BroadcastInventorySlotChanged(SlotIndex);
	return true;
}

bool UFrontierInventoryComponent::ClearSlot(const int32 SlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	Slots[SlotIndex] = FFrontierInventorySlot();
	Slots[SlotIndex].SlotIndex = SlotIndex;
	BroadcastInventorySlotChanged(SlotIndex);
	return true;
}

void UFrontierInventoryComponent::SetSlotsFromSnapshot(const TArray<FFrontierInventorySlot>& InSlots)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	Slots = InSlots;
	NormalizeSlotArray(Slots);
	BroadcastInventoryChanged();
}

bool UFrontierInventoryComponent::SetAuthoritativeSlotCapacity(const int32 NewCapacity)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || NewCapacity <= 0)
	{
		return false;
	}

	const int32 DesiredSlotCount = FMath::Max(1, NewCapacity + BonusSlotCount);
	if (DesiredSlotCount < Slots.Num())
	{
		for (int32 SlotIndex = DesiredSlotCount; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			if (Slots[SlotIndex].bOccupied && Slots[SlotIndex].ItemInstance.IsValid())
			{
				FRONTIER_LOG(Warning, TEXT("Authoritative capacity shrink rejected because slot %d is occupied."), SlotIndex);
				return false;
			}
		}
	}

	SlotCount = NewCapacity;
	Slots.SetNum(DesiredSlotCount);
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		Slots[SlotIndex].SlotIndex = SlotIndex;
		if (!Slots[SlotIndex].bOccupied || !Slots[SlotIndex].ItemInstance.IsValid())
		{
			Slots[SlotIndex] = FFrontierInventorySlot();
			Slots[SlotIndex].SlotIndex = SlotIndex;
		}
	}

	BroadcastInventoryChanged();
	return true;
}

bool UFrontierInventoryComponent::ApplyAuthoritativeInventoryState(const TArray<FFrontierInventorySlot>& InSlots)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || InSlots.Num() <= 0)
	{
		return false;
	}

	SlotCount = InSlots.Num();
	Slots = InSlots;
	Slots.SetNum(FMath::Max(1, SlotCount + BonusSlotCount));
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		FFrontierInventorySlot& Slot = Slots[SlotIndex];
		Slot.SlotIndex = SlotIndex;
		if (!Slot.bOccupied || !Slot.ItemInstance.IsValid())
		{
			Slot = FFrontierInventorySlot();
			Slot.SlotIndex = SlotIndex;
			continue;
		}

		Slot.ItemInstance.EnsureRuntimeIdentity();
	}

	BroadcastInventoryChanged();
	return true;
}

int32 UFrontierInventoryComponent::GetSlotCount() const
{
	return FMath::Max(SlotCount + BonusSlotCount, FMath::Max(Slots.Num(), ReplicatedSlotCapacity));
}

bool UFrontierInventoryComponent::SetBonusSlotCount(const int32 NewBonusSlotCount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || NewBonusSlotCount < 0)
	{
		return false;
	}

	BonusSlotCount = NewBonusSlotCount;
	const int32 DesiredSlotCount = FMath::Max(1, SlotCount + BonusSlotCount);
	if (Slots.Num() < DesiredSlotCount)
	{
		const int32 PreviousNum = Slots.Num();
		Slots.SetNum(DesiredSlotCount);
		for (int32 SlotIndex = PreviousNum; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			Slots[SlotIndex].SlotIndex = SlotIndex;
		}
		BroadcastInventoryChanged();
	}
	else
	{
		SynchronizeFullReplicationState();
		ForceOwnerNetUpdate(this);
	}
	return true;
}

bool UFrontierInventoryComponent::IsFull() const
{
	return FindFirstEmptySlotIndex() == INDEX_NONE;
}

void UFrontierInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	ReplicatedSlots.SetOwningInventory(this);
	InitializeSlots();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		SynchronizeFullReplicationState();
	}
}

void UFrontierInventoryComponent::OnRep_Slots()
{
	OnInventoryChanged.Broadcast(Slots);
}

void UFrontierInventoryComponent::OnRep_ReplicatedSlotCapacity()
{
	QueueReplicatedViewRefresh();
}

void UFrontierInventoryComponent::InitializeSlots()
{
	if (!Slots.IsEmpty())
	{
		NormalizeSlotArray(Slots);
		return;
	}

	const int32 InitialSlotCount = FMath::Max(1, SlotCount + BonusSlotCount);
	Slots.Reserve(InitialSlotCount);
	for (int32 SlotIndex = 0; SlotIndex < InitialSlotCount; ++SlotIndex)
	{
		FFrontierInventorySlot Slot;
		Slot.SlotIndex = SlotIndex;
		Slots.Add(Slot);
	}
}

void UFrontierInventoryComponent::NormalizeSlotArray(TArray<FFrontierInventorySlot>& InOutSlots) const
{
	const int32 DesiredSlotCount = FMath::Max(SlotCount + BonusSlotCount, InOutSlots.Num());
	InOutSlots.SetNum(DesiredSlotCount);

	for (int32 SlotIndex = 0; SlotIndex < InOutSlots.Num(); ++SlotIndex)
	{
		FFrontierInventorySlot& Slot = InOutSlots[SlotIndex];
		Slot.SlotIndex = SlotIndex;
		if (!Slot.bOccupied || !Slot.ItemInstance.IsValid())
		{
			Slot.bOccupied = false;
			Slot.ItemInstance = FFrontierItemInstance();
			continue;
		}

		Slot.ItemInstance.EnsureRuntimeIdentity();
	}
}

bool UFrontierInventoryComponent::CanAcceptItem(const FFrontierItemInstance& ItemInstance) const
{
	return ItemInstance.IsValid();
}

bool UFrontierInventoryComponent::ShouldReplicateInventoryToOwnerOnly() const
{
	return false;
}

int32 UFrontierInventoryComponent::FindFirstEmptySlotIndex() const
{
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		if (!Slots[SlotIndex].bOccupied)
		{
			return SlotIndex;
		}
	}

	return INDEX_NONE;
}

int32 UFrontierInventoryComponent::FindStackableSlotIndex(const FFrontierItemInstance& ItemInstance) const
{
	if (!ItemInstance.IsStackable())
	{
		return INDEX_NONE;
	}

	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		const FFrontierInventorySlot& Slot = Slots[SlotIndex];
		if (!Slot.bOccupied || !Slot.ItemInstance.IsValid())
		{
			continue;
		}

		const int32 MaxStack = ItemInstance.GetMaxStack();
		if (Slot.ItemInstance.GetTemplateId() == ItemInstance.GetTemplateId() && Slot.ItemInstance.Quantity < MaxStack)
		{
			return SlotIndex;
		}
	}

	return INDEX_NONE;
}

void UFrontierInventoryComponent::BroadcastInventoryChanged()
{
	SynchronizeFullReplicationState();
	ForceOwnerNetUpdate(this);
	OnInventoryChanged.Broadcast(Slots);
}

void UFrontierInventoryComponent::BroadcastInventorySlotChanged(const int32 SlotIndex)
{
	const int32 ChangedSlotIndices[] = { SlotIndex };
	SynchronizeChangedReplicationSlots(ChangedSlotIndices);
	ForceOwnerNetUpdate(this);
	OnInventoryChanged.Broadcast(Slots);
}

void UFrontierInventoryComponent::BroadcastInventorySlotsChanged(const int32 FirstSlotIndex, const int32 SecondSlotIndex)
{
	const int32 ChangedSlotIndices[] = { FirstSlotIndex, SecondSlotIndex };
	SynchronizeChangedReplicationSlots(ChangedSlotIndices);
	ForceOwnerNetUpdate(this);
	OnInventoryChanged.Broadcast(Slots);
}

void UFrontierInventoryComponent::QueueReplicatedViewRefresh()
{
	if (bReplicatedViewRefreshQueued)
	{
		return;
	}

	bReplicatedViewRefreshQueued = true;
	if (UWorld* World = GetWorld())
	{
		const TWeakObjectPtr<UFrontierInventoryComponent> WeakThis(this);
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis]()
		{
			if (UFrontierInventoryComponent* Inventory = WeakThis.Get())
			{
				Inventory->bReplicatedViewRefreshQueued = false;
				Inventory->RefreshSlotsFromReplication();
			}
		}));
		return;
	}

	bReplicatedViewRefreshQueued = false;
	RefreshSlotsFromReplication();
}

void UFrontierInventoryComponent::RefreshSlotsFromReplication()
{
	int32 DesiredSlotCount = FMath::Max(SlotCount, ReplicatedSlotCapacity);
	for (const FFrontierReplicatedInventorySlot& ReplicatedSlot : ReplicatedSlots.Items)
	{
		DesiredSlotCount = FMath::Max(DesiredSlotCount, ReplicatedSlot.Slot.SlotIndex + 1);
	}

	TArray<FFrontierInventorySlot> RefreshedSlots;
	RefreshedSlots.SetNum(FMath::Max(0, DesiredSlotCount));
	for (int32 SlotIndex = 0; SlotIndex < RefreshedSlots.Num(); ++SlotIndex)
	{
		RefreshedSlots[SlotIndex].SlotIndex = SlotIndex;
	}

	for (const FFrontierReplicatedInventorySlot& ReplicatedSlot : ReplicatedSlots.Items)
	{
		const int32 SlotIndex = ReplicatedSlot.Slot.SlotIndex;
		if (!RefreshedSlots.IsValidIndex(SlotIndex) || !ReplicatedSlot.Slot.bOccupied || !ReplicatedSlot.Slot.ItemInstance.IsValid())
		{
			continue;
		}

		RefreshedSlots[SlotIndex] = ReplicatedSlot.Slot;
		RefreshedSlots[SlotIndex].SlotIndex = SlotIndex;
	}

	Slots = MoveTemp(RefreshedSlots);
	OnRep_Slots();
}

void UFrontierInventoryComponent::SynchronizeFullReplicationState()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	ReplicatedSlotCapacity = Slots.Num();
	ReplicatedSlots.RebuildFromSlots(Slots);
}

void UFrontierInventoryComponent::SynchronizeChangedReplicationSlots(const TConstArrayView<int32> ChangedSlotIndices)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	ReplicatedSlotCapacity = Slots.Num();
	ReplicatedSlots.UpdateSlots(Slots, ChangedSlotIndices);
}
