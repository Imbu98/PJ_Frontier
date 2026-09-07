#pragma once

#include "CoreMinimal.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Progression/FrontierRaidLootTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "FrontierRaidLootPoolSubsystem.generated.h"

class UFrontierLootComponent;
class UFrontierRaidLootPoolPolicyDataAsset;
struct FFrontierRaidLootBatchResponse;

UENUM()
enum class EFrontierRaidLootInitializationState : uint8
{
	NotStarted,
	RequestingInitialBatch,
	PreparingComponents,
	NotifyingReady,
	Ready,
	Failed
};

/** World-scoped authority pool for backend-issued raid loot identities. */
UCLASS()
class FRONTIER_API UFrontierRaidLootPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void StartDedicatedServerInitialization();
	void RegisterLootComponent(UFrontierLootComponent* LootComponent);
	void UnregisterLootComponent(UFrontierLootComponent* LootComponent);

	EFrontierRaidLootInitializationState GetInitializationState() const { return InitializationState; }
	bool IsServerReady() const { return InitializationState == EFrontierRaidLootInitializationState::Ready; }

private:
	struct FPreparedEntry
	{
		FString LootEntryId;
		TArray<FFrontierInventorySlot> Slots;
	};

	struct FPoolState
	{
		TArray<FPreparedEntry> Entries;
		int32 NextEntryIndex = 0;
		int32 RefillThreshold = 0;
		int32 RefillRequestCount = 0;

		int32 Remaining() const { return Entries.Num() - NextEntryIndex; }
	};

	bool LoadPolicyData(FString& OutError);
	bool ApplyPolicyToPool(FName LootTableId, FString& OutError);
	bool CollectWorldLootComponents(FString& OutError);
	void RequestInitialBatch();
	void HandleInitialBatch(const FFrontierRaidLootBatchResponse& Response);
	bool AppendBatchAtomically(
		const FFrontierRaidLootBatchResponse& Response,
		const TSet<FName>& RequiredTables,
		FString& OutError);
	void PrepareAllPendingComponents();
	bool TryPrepareComponent(UFrontierLootComponent* LootComponent);
	void QueueRefill(FName LootTableId);
	void FlushRefillRequests();
	void SendActiveRefillRequest();
	void HandleRefillBatch(const FFrontierRaidLootBatchResponse& Response);
	void NotifyBackendReady();
	void FailInitialization(const FString& Error);

	TSet<TWeakObjectPtr<UFrontierLootComponent>> RegisteredComponents;
	TSet<TWeakObjectPtr<UFrontierLootComponent>> PendingComponents;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierRaidLootPoolPolicyDataAsset> PolicyData;

	TMap<FName, FPoolState> Pools;
	TSet<FGuid> SeenRaidItemIds;
	TSet<FString> SeenLootSourceIds;
	TSet<FString> SeenLootEntryIds;
	TSet<FName> PendingRefillTables;
	TSet<FName> ActiveRefillTables;
	FFrontierRaidLootBatchRequest ActiveRefillRequest;
	FString InitialBatchIdempotencyKey;
	FString ActiveRefillIdempotencyKey;
	FString ReadyIdempotencyKey;
	FString FailureIdempotencyKey;
	FTimerHandle RefillRetryTimerHandle;
	int32 RefillRetryAttempt = 0;
	bool bRefillRequestInFlight = false;
	bool bRefillFlushScheduled = false;
	EFrontierRaidLootInitializationState InitializationState = EFrontierRaidLootInitializationState::NotStarted;
};
