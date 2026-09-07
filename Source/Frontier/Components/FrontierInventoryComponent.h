#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "FrontierInventoryComponent.generated.h"

class FCustomPropertyConditionState;
class UFrontierInventoryComponent;

USTRUCT()
struct FRONTIER_API FFrontierReplicatedInventorySlot : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	FFrontierInventorySlot Slot;
};

USTRUCT()
struct FRONTIER_API FFrontierReplicatedInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FFrontierReplicatedInventorySlot> Items;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParameters)
	{
		return FastArrayDeltaSerialize<FFrontierReplicatedInventorySlot, FFrontierReplicatedInventoryList>(Items, DeltaParameters, *this);
	}

	void SetOwningInventory(UFrontierInventoryComponent* InOwningInventory);
	void RebuildFromSlots(const TArray<FFrontierInventorySlot>& SourceSlots);
	void UpdateSlots(const TArray<FFrontierInventorySlot>& SourceSlots, TConstArrayView<int32> ChangedSlotIndices);
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

private:
	TWeakObjectPtr<UFrontierInventoryComponent> OwningInventory;
};

template<>
struct TStructOpsTypeTraits<FFrontierReplicatedInventoryList> : public TStructOpsTypeTraitsBase2<FFrontierReplicatedInventoryList>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFrontierInventoryChangedSignature, const TArray<FFrontierInventorySlot>&, Slots);

UCLASS(ClassGroup=(Frontier), meta=(BlueprintSpawnableComponent))
class FRONTIER_API UFrontierInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFrontierInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void GetReplicatedCustomConditionState(FCustomPropertyConditionState& OutActiveState) const override;

	UFUNCTION(BlueprintCallable, Category="Inventory")
	virtual bool AddItem(const FFrontierItemInstance& ItemInstance);

	UFUNCTION(BlueprintCallable, Category="Inventory")
	virtual bool RemoveItemAtSlot(int32 SlotIndex, int32 Quantity = 1);

	UFUNCTION(BlueprintCallable, Category="Inventory")
	virtual bool SwapSlots(int32 SourceSlotIndex, int32 TargetSlotIndex);

	UFUNCTION(BlueprintCallable, Category="Inventory")
	virtual bool MoveItemTo(UFrontierInventoryComponent* TargetInventory, int32 SourceSlotIndex);

	UFUNCTION(BlueprintCallable, Category="Inventory")
	virtual bool MoveItemToSlot(int32 SourceSlotIndex, UFrontierInventoryComponent* TargetInventory, int32 TargetSlotIndex);

	UFUNCTION(BlueprintCallable, Category="Inventory")
	virtual bool UseItemAtSlot(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category="Inventory")
	const TArray<FFrontierInventorySlot>& GetSlots() const;

	UFUNCTION(BlueprintPure, Category="Inventory")
	bool GetSlot(int32 SlotIndex, FFrontierInventorySlot& OutSlot) const;

	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool SetItemAtSlot(int32 SlotIndex, const FFrontierItemInstance& ItemInstance);

	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool ClearSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category="Inventory")
	void SetSlotsFromSnapshot(const TArray<FFrontierInventorySlot>& InSlots);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Inventory|Backend")
	bool SetAuthoritativeSlotCapacity(int32 NewCapacity);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Inventory|Backend")
	bool ApplyAuthoritativeInventoryState(const TArray<FFrontierInventorySlot>& InSlots);

	UFUNCTION(BlueprintPure, Category="Inventory")
	int32 GetSlotCount() const;

	/** Server-authoritative permanent capacity bonus. Existing occupied slots are never discarded. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Inventory")
	bool SetBonusSlotCount(int32 NewBonusSlotCount);

	UFUNCTION(BlueprintPure, Category="Inventory")
	int32 GetBonusSlotCount() const { return BonusSlotCount; }

	UFUNCTION(BlueprintPure, Category="Inventory")
	bool IsFull() const;

	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FFrontierInventoryChangedSignature OnInventoryChanged;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void OnRep_Slots();

	virtual void InitializeSlots();
	virtual bool CanAcceptItem(const FFrontierItemInstance& ItemInstance) const;
	virtual bool ShouldReplicateInventoryToOwnerOnly() const;
	void NormalizeSlotArray(TArray<FFrontierInventorySlot>& InOutSlots) const;
	int32 FindFirstEmptySlotIndex() const;
	int32 FindStackableSlotIndex(const FFrontierItemInstance& ItemInstance) const;
	void BroadcastInventoryChanged();
	void BroadcastInventorySlotChanged(int32 SlotIndex);
	void BroadcastInventorySlotsChanged(int32 FirstSlotIndex, int32 SecondSlotIndex);

	UFUNCTION()
	void OnRep_ReplicatedSlotCapacity();

	void QueueReplicatedViewRefresh();
	void RefreshSlotsFromReplication();
	void SynchronizeFullReplicationState();
	void SynchronizeChangedReplicationSlots(TConstArrayView<int32> ChangedSlotIndices);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1"))
	int32 SlotCount = 24;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Inventory")
	int32 BonusSlotCount = 0;

	// Compatibility view used by Blueprint and UI. Network state is stored in ReplicatedSlots.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Inventory")
	TArray<FFrontierInventorySlot> Slots;

	UPROPERTY(Replicated)
	FFrontierReplicatedInventoryList ReplicatedSlots;

	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedSlotCapacity)
	int32 ReplicatedSlotCapacity = 0;

private:
	friend struct FFrontierReplicatedInventoryList;

	bool bReplicatedViewRefreshQueued = false;
};
