#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "FrontierLoadoutComponent.generated.h"

class UFrontierInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFrontierLoadoutChangedSignature, const TArray<FFrontierLoadoutSlot>&, Slots);

UCLASS(ClassGroup=(Frontier), meta=(BlueprintSpawnableComponent))
class FRONTIER_API UFrontierLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFrontierLoadoutComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category="Loadout")
	bool EquipItem(EFrontierEquipmentSlot SlotType, const FFrontierItemInstance& ItemInstance);

	UFUNCTION(BlueprintCallable, Category="Loadout")
	bool EquipItemFromInventory(UFrontierInventoryComponent* SourceInventory, int32 SourceSlotIndex, EFrontierEquipmentSlot RequestedSlot = EFrontierEquipmentSlot::None);

	UFUNCTION(BlueprintCallable, Category="Loadout")
	bool UnequipItem(EFrontierEquipmentSlot SlotType);

	UFUNCTION(BlueprintCallable, Category="Loadout")
	bool UnequipItemToInventory(UFrontierInventoryComponent* TargetInventory, EFrontierEquipmentSlot SlotType);

	UFUNCTION(BlueprintCallable, Category="Loadout")
	bool UnequipItemToInventorySlot(UFrontierInventoryComponent* TargetInventory, EFrontierEquipmentSlot SlotType, int32 TargetSlotIndex);

	UFUNCTION(BlueprintPure, Category="Loadout")
	const TArray<FFrontierLoadoutSlot>& GetLoadoutSlots() const;

	UFUNCTION(BlueprintPure, Category="Loadout")
	bool FindLoadoutSlot(EFrontierEquipmentSlot SlotType, FFrontierLoadoutSlot& OutSlot) const;

	UFUNCTION(BlueprintPure, Category="Loadout")
	bool CanEquipItemToSlot(EFrontierEquipmentSlot SlotType, const FFrontierItemInstance& ItemInstance) const;

	UFUNCTION(BlueprintPure, Category="Loadout")
	bool ResolveEquipSlotForItem(const FFrontierItemInstance& ItemInstance, EFrontierEquipmentSlot& OutSlotType) const;

	UFUNCTION(BlueprintCallable, Category="Loadout")
	void SetLoadoutSlotsFromSnapshot(const TArray<FFrontierLoadoutSlot>& InSlots);

	UFUNCTION(BlueprintPure, Category="Loadout|Score")
	float GetTotalEquipmentScore() const;

	const FFrontierLoadoutSlot* FindLoadoutSlotPtr(EFrontierEquipmentSlot SlotType) const;
	FFrontierLoadoutSlot* FindMutableLoadoutSlotPtr(EFrontierEquipmentSlot SlotType);

	UPROPERTY(BlueprintAssignable, Category="Loadout")
	FFrontierLoadoutChangedSignature OnLoadoutChanged;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_LoadoutSlots();

	void InitializeDefaultSlots();
	void BroadcastLoadoutChanged();
	void RecalculateTotalEquipmentScore();

	// Full instance data is owner-only; public visuals are represented by replicated equipment actors.
	UPROPERTY(ReplicatedUsing=OnRep_LoadoutSlots, VisibleInstanceOnly, BlueprintReadOnly, Category="Loadout")
	TArray<FFrontierLoadoutSlot> LoadoutSlots;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Loadout|Score")
	float TotalEquipmentScore = 0.0f;
};
