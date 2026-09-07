#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Loot/FrontierLootDropDataAsset.h"
#include "FrontierLootComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FFrontierBackendLootPreparedNative);

UCLASS(ClassGroup=(Frontier), meta=(BlueprintSpawnableComponent))
class FRONTIER_API UFrontierLootComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFrontierLootComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="Loot")
	TArray<FFrontierInventorySlot> GenerateLootSlots() const;

	UFUNCTION(BlueprintPure, Category="Loot|Backend")
	bool IsBackendLootManaged() const { return !LootTableId.IsNone(); }

	UFUNCTION(BlueprintPure, Category="Loot|Backend")
	bool IsBackendLootPrepared() const { return bBackendLootPrepared; }

	FName GetLootTableId() const { return LootTableId; }
	bool HasLegacyLocalLootDefinition() const { return LootDropDataAsset != nullptr || !LootTable.IsEmpty(); }
	FFrontierBackendLootPreparedNative& OnBackendLootPrepared() { return BackendLootPrepared; }
	void AssignBackendLootSlots(TArray<FFrontierInventorySlot> InLootSlots);

protected:
	TArray<FFrontierInventorySlot> GenerateLootSlotsFromTable(const TArray<FFrontierLootTableEntry>& SourceLootTable, int32 MinRolls, int32 MaxRolls) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot")
	TObjectPtr<UFrontierLootDropDataAsset> LootDropDataAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta=(ClampMin="0"))
	int32 MinLootRolls = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta=(ClampMin="0"))
	int32 MaxLootRolls = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot")
	TArray<FFrontierLootTableEntry> LootTable;

	/** Backend catalog loot table ID. None keeps the legacy local table for editor migration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot|Backend")
	FName LootTableId;

private:
	UPROPERTY(Transient)
	TArray<FFrontierInventorySlot> PreparedBackendLootSlots;

	bool bBackendLootPrepared = false;
	FFrontierBackendLootPreparedNative BackendLootPrepared;
};
