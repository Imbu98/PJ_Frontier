#pragma once

#include "CoreMinimal.h"
#include "Interaction/FrontierInteractableActor.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Loot/FrontierLootDropDataAsset.h"
#include "FrontierLootContainerActor.generated.h"

class AFrontierPlayerController;
class UFrontierItemDataAsset;
class UFrontierLootComponent;
class UFrontierLootInventoryComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFrontierDeadPlayerInventoryChangedSignature, const TArray<FFrontierInventorySlot>&, Slots);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFrontierDeadPlayerLoadoutChangedSignature, const TArray<FFrontierLoadoutSlot>&, Slots);

UENUM(BlueprintType)
enum class EFrontierLootContainerSourceType : uint8
{
	WorldLoot,
	EnemyDeath,
	BossDeath,
	PlayerDeath
};

UCLASS()
class FRONTIER_API AFrontierLootContainerActor : public AFrontierInteractableActor
{
	GENERATED_BODY()

public:
	AFrontierLootContainerActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool CanInteract(const AFrontierPlayerController* InteractingController) const override;
	virtual void Interacted(AFrontierPlayerController* InteractingController) override;
	virtual FText GetInteractionDisplayName(const AFrontierPlayerController* InteractingController) const override;
	virtual FText GetInteractionActionText(const AFrontierPlayerController* InteractingController) const override;
	virtual FText GetInteractionPromptText(const AFrontierPlayerController* InteractingController) const override;
	virtual FVector GetInteractionWorldLocation() const override;
	virtual void GetInteractionHighlightComponents(TArray<UPrimitiveComponent*>& OutComponents) const override;

	UFUNCTION(BlueprintPure, Category="Loot")
	UFrontierLootInventoryComponent* GetLootInventoryComponent() const;

	UFUNCTION(BlueprintPure, Category="Loot")
	EFrontierLootContainerSourceType GetSourceType() const;

	UFUNCTION(BlueprintPure, Category="Loot")
	FText GetLootActorDisplayName() const;

	UFUNCTION(BlueprintPure, Category="Loot")
	const TArray<FFrontierInventorySlot>& GetDeadPlayerInventory() const;

	UFUNCTION(BlueprintPure, Category="Loot")
	const TArray<FFrontierLoadoutSlot>& GetDeadPlayerLoadoutItems() const;

	UFUNCTION(BlueprintCallable, Category="Loot")
	void InitializeFromLootSlots(const TArray<FFrontierInventorySlot>& InitialLootSlots, EFrontierLootContainerSourceType InSourceType);

	UFUNCTION(BlueprintCallable, Category="Loot")
	void InitializeDeadPlayerLootData(const TArray<FFrontierInventorySlot>& InDeadPlayerInventory, const TArray<FFrontierLoadoutSlot>& InDeadPlayerLoadoutItems);

	UFUNCTION(BlueprintCallable, Category="Loot")
	bool LootItemToPlayer(AFrontierPlayerController* InteractingController, int32 LootSlotIndex);

	UFUNCTION(BlueprintCallable, Category="Loot")
	bool LootItemToPlayerSlot(AFrontierPlayerController* InteractingController, int32 LootSlotIndex, int32 TargetRaidSlotIndex);

	UFUNCTION(BlueprintCallable, Category="Loot")
	bool LootDeadPlayerLoadoutItemToPlayer(AFrontierPlayerController* InteractingController, EFrontierEquipmentSlot SlotType);

	UFUNCTION(BlueprintCallable, Category="Loot")
	bool StorePlayerItem(AFrontierPlayerController* InteractingController, int32 RaidSlotIndex);

	UFUNCTION(BlueprintCallable, Category="Loot")
	bool StorePlayerItemAtSlot(AFrontierPlayerController* InteractingController, int32 RaidSlotIndex, int32 TargetLootSlotIndex);

	UFUNCTION(BlueprintCallable, Category="Loot")
	bool SwapLootSlots(int32 SourceLootSlotIndex, int32 TargetLootSlotIndex);

	/** Player-facing swap path. Validates authority and interaction range before mutating loot. */
	UFUNCTION(BlueprintCallable, Category="Loot")
	bool SwapLootSlotsForPlayer(AFrontierPlayerController* InteractingController, int32 SourceLootSlotIndex, int32 TargetLootSlotIndex);

	UPROPERTY(BlueprintAssignable, Category="Loot|DeadPlayer")
	FFrontierDeadPlayerInventoryChangedSignature OnDeadPlayerInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category="Loot|DeadPlayer")
	FFrontierDeadPlayerLoadoutChangedSignature OnDeadPlayerLoadoutChanged;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_SourceType();

	UFUNCTION()
	void OnRep_DeadPlayerLoadoutItems();

	UFUNCTION()
	void HandleLootInventoryChanged(const TArray<FFrontierInventorySlot>& UpdatedSlots);
	void HandleBackendLootPrepared();

	void BroadcastDeadPlayerInventoryChanged();
	void BroadcastDeadPlayerLoadoutChanged();
	void GenerateInitialLoot();
	bool HasAnyLoot() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Loot")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Loot")
	TObjectPtr<UFrontierLootInventoryComponent> LootInventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Loot")
	TObjectPtr<UFrontierLootComponent> LootComponent;

	UPROPERTY(ReplicatedUsing=OnRep_SourceType, BlueprintReadOnly, Category="Loot")
	EFrontierLootContainerSourceType SourceType = EFrontierLootContainerSourceType::WorldLoot;

	UPROPERTY(EditInstanceOnly, Replicated, BlueprintReadOnly, Category="Loot")
	FText LootActorDisplayName = FText::FromString(TEXT("Loot"));

	// Compatibility mirror of LootInventoryComponent for existing Blueprint and UI consumers.
	UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category="Loot|DeadPlayer")
	TArray<FFrontierInventorySlot> DeadPlayerInventory;

	UPROPERTY(ReplicatedUsing=OnRep_DeadPlayerLoadoutItems, BlueprintReadOnly, Category="Loot|DeadPlayer")
	TArray<FFrontierLoadoutSlot> DeadPlayerLoadoutItems;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot")
	bool bGenerateLootOnBeginPlay = true;

	UPROPERTY(Transient)
	bool bInitialLootGenerated = false;

};
