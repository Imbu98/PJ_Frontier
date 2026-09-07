#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/FrontierQuickSlotTypes.h"
#include "FrontierQuickSlotComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FFrontierQuickSlotsChangedSignature);

UCLASS(ClassGroup=(Frontier), meta=(BlueprintSpawnableComponent))
class FRONTIER_API UFrontierQuickSlotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFrontierQuickSlotComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static constexpr int32 QuickSlotCount = 3;

	UFUNCTION(BlueprintPure, Category="Quick Slot")
	const TArray<FFrontierQuickSlotEntry>& GetQuickSlots() const { return QuickSlots; }

	UFUNCTION(BlueprintPure, Category="Quick Slot")
	bool GetQuickSlot(int32 SlotIndex, FFrontierQuickSlotEntry& OutEntry) const;

	UFUNCTION(BlueprintPure, Category="Quick Slot")
	bool IsQuickSlotOccupied(int32 SlotIndex) const;

	bool SetQuickSlot(int32 SlotIndex, const FFrontierQuickSlotEntry& Entry);
	bool ClearQuickSlot(int32 SlotIndex);
	int32 ClearQuickSlotsMatchingItem(const FFrontierItemInstance& ItemInstance);
	bool MoveOrSwapQuickSlots(int32 SourceSlotIndex, int32 TargetSlotIndex);

	bool ResolveItemForQuickSlot(int32 SlotIndex, FFrontierItemInstance& OutItem) const;

	FFrontierQuickSlotsChangedSignature OnQuickSlotsChanged;

protected:
	UFUNCTION()
	void OnRep_QuickSlots();

	void InitializeQuickSlots();
	void BroadcastQuickSlotsChanged();

	UPROPERTY(ReplicatedUsing=OnRep_QuickSlots, VisibleInstanceOnly, BlueprintReadOnly, Category="Quick Slot")
	TArray<FFrontierQuickSlotEntry> QuickSlots;
};
