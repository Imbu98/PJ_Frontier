#include "Progression/FrontierRaidLootPoolSubsystem.h"

#include "Components/FrontierLootComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Frontier.h"
#include "Inventory/FrontierRaidRuntimeItemMapper.h"
#include "Inventory/Items/FrontierItemCatalogSubsystem.h"
#include "Loot/FrontierRaidLootPoolPolicyDataAsset.h"
#include "Loot/FrontierRaidLootPoolSettings.h"
#include "Progression/FrontierInternalApiSubsystem.h"
#include "Progression/FrontierRaidLootTypes.h"
#include "TimerManager.h"

void UFrontierRaidLootPoolSubsystem::StartDedicatedServerInitialization()
{
	UWorld* World = GetWorld();
	FRONTIER_LOG(
		Log,
		TEXT("[RaidStartup] Loot pool initialization entered. State=%d World=%s NetMode=%d"),
		static_cast<int32>(InitializationState),
		World ? *World->GetOutermost()->GetName() : TEXT("<null>"),
		World ? static_cast<int32>(World->GetNetMode()) : -1);
	if (InitializationState != EFrontierRaidLootInitializationState::NotStarted)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("[RaidStartup] Loot pool initialization skipped because it was already started. State=%d"),
			static_cast<int32>(InitializationState));
		return;
	}
	if (!World)
	{
		FRONTIER_LOG(Error, TEXT("[RaidStartup] Loot pool initialization stopped because the world is unavailable."));
		return;
	}
	if (World->GetNetMode() != NM_DedicatedServer)
	{
		FRONTIER_LOG(Error, TEXT("[RaidStartup] Loot pool initialization stopped because the world is not a dedicated server."));
		return;
	}

	FRONTIER_LOG(Log, TEXT("[RaidStartup] Collecting backend-managed LootComponents from the raid world."));
	FString CollectionError;
	if (!CollectWorldLootComponents(CollectionError))
	{
		FailInitialization(CollectionError);
		return;
	}
	FRONTIER_LOG(
		Log,
		TEXT("[RaidStartup] LootComponent collection completed. RegisteredComponentCount=%d"),
		RegisteredComponents.Num());
	RequestInitialBatch();
}

void UFrontierRaidLootPoolSubsystem::RegisterLootComponent(UFrontierLootComponent* LootComponent)
{
	if (!IsValid(LootComponent) || !LootComponent->IsBackendLootManaged())
	{
		return;
	}
	RegisteredComponents.Add(LootComponent);

	if (InitializationState == EFrontierRaidLootInitializationState::Ready)
	{
		FString PolicyError;
		if (!ApplyPolicyToPool(LootComponent->GetLootTableId(), PolicyError))
		{
			RegisteredComponents.Remove(LootComponent);
			FRONTIER_LOG(Error, TEXT("[RaidLootPool] Runtime LootComponent rejected: %s"), *PolicyError);
			return;
		}
		if (!TryPrepareComponent(LootComponent))
		{
			PendingComponents.Add(LootComponent);
			QueueRefill(LootComponent->GetLootTableId());
		}
	}
}

void UFrontierRaidLootPoolSubsystem::UnregisterLootComponent(UFrontierLootComponent* LootComponent)
{
	RegisteredComponents.Remove(LootComponent);
	PendingComponents.Remove(LootComponent);
}

bool UFrontierRaidLootPoolSubsystem::LoadPolicyData(FString& OutError)
{
	OutError.Reset();
	FRONTIER_LOG(Log, TEXT("[RaidStartup] Loading raid loot pool policy data from Project Settings."));
	const UFrontierRaidLootPoolSettings* Settings = GetDefault<UFrontierRaidLootPoolSettings>();
	if (!Settings || Settings->PolicyData.IsNull())
	{
		OutError = TEXT("Raid loot pool PolicyData is not configured in Project Settings > Game > Frontier Raid Loot Pool.");
		FRONTIER_LOG(Error, TEXT("[RaidStartup] Loot policy load failed: %s"), *OutError);
		return false;
	}

	FRONTIER_LOG(
		Log,
		TEXT("[RaidStartup] Loading loot policy asset. Path=%s"),
		*Settings->PolicyData.ToSoftObjectPath().ToString());
	PolicyData = Settings->PolicyData.LoadSynchronous();
	if (!PolicyData)
	{
		OutError = FString::Printf(
			TEXT("Raid loot pool PolicyData could not be loaded: %s"),
			*Settings->PolicyData.ToSoftObjectPath().ToString());
		FRONTIER_LOG(Error, TEXT("[RaidStartup] Loot policy load failed: %s"), *OutError);
		return false;
	}
	const bool bValid = PolicyData->ValidatePolicies(OutError);
	if (bValid)
	{
		FRONTIER_LOG(
			Log,
			TEXT("[RaidStartup] Loot policy validation succeeded. PolicyCount=%d"),
			PolicyData->Policies.Num());
	}
	else
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidStartup] Loot policy validation failed. PolicyCount=%d Error=%s"),
			PolicyData->Policies.Num(),
			OutError.IsEmpty() ? TEXT("<empty>") : *OutError);
	}
	return bValid;
}

bool UFrontierRaidLootPoolSubsystem::ApplyPolicyToPool(const FName LootTableId, FString& OutError)
{
	OutError.Reset();
	if (!PolicyData)
	{
		OutError = TEXT("Raid loot pool policy data is not loaded.");
		return false;
	}
	const FFrontierRaidLootPoolPolicy* Policy = PolicyData->FindPolicy(LootTableId);
	if (!Policy)
	{
		OutError = FString::Printf(
			TEXT("No raid loot pool policy exists for LootTableId=%s."),
			*LootTableId.ToString());
		return false;
	}
	FPoolState& Pool = Pools.FindOrAdd(LootTableId);
	Pool.RefillThreshold = Policy->RefillThreshold;
	Pool.RefillRequestCount = Policy->RefillRequestCount;
	return true;
}

bool UFrontierRaidLootPoolSubsystem::CollectWorldLootComponents(FString& OutError)
{
	OutError.Reset();
	int32 BackendManagedComponentCount = 0;
	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		TInlineComponentArray<UFrontierLootComponent*> LootComponents(*ActorIt);
		for (UFrontierLootComponent* Component : LootComponents)
		{
			if (IsValid(Component) && Component->IsBackendLootManaged())
			{
				RegisterLootComponent(Component);
				++BackendManagedComponentCount;
			}
			else if (IsValid(Component) && Component->HasLegacyLocalLootDefinition())
			{
				OutError = FString::Printf(
					TEXT("Map loot component has a local loot definition but no LootTableId. Owner=%s"),
					*GetPathNameSafe(Component->GetOwner()));
				FRONTIER_LOG(Error, TEXT("[RaidStartup] LootComponent collection failed: %s"), *OutError);
				return false;
			}
		}
	}
	FRONTIER_LOG(
		Log,
		TEXT("[RaidStartup] World LootComponent scan finished. BackendManagedComponentCount=%d"),
		BackendManagedComponentCount);
	return true;
}

void UFrontierRaidLootPoolSubsystem::RequestInitialBatch()
{
	FRONTIER_LOG(Log, TEXT("[RaidStartup] Preparing initial loot batch request."));
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UFrontierInternalApiSubsystem* InternalApi = GameInstance
		? GameInstance->GetSubsystem<UFrontierInternalApiSubsystem>()
		: nullptr;
	if (!InternalApi)
	{
		FRONTIER_LOG(Error, TEXT("[RaidStartup] Initial loot batch stopped: InternalApiSubsystem is unavailable."));
		FailInitialization(TEXT("Internal API subsystem is unavailable."));
		return;
	}
	const FFrontierInternalApiConfig& Config = InternalApi->GetConfig();
	FRONTIER_LOG(
		Log,
		TEXT("[RaidStartup] Internal API identity loaded. RaidServerId=%s MatchId=%s MapId=%s PublicAddress=%s PublicPort=%d"),
		Config.RaidServerId.IsEmpty() ? TEXT("<empty>") : *Config.RaidServerId,
		Config.MatchId.IsEmpty() ? TEXT("<empty>") : *Config.MatchId,
		Config.MapId.IsEmpty() ? TEXT("<empty>") : *Config.MapId,
		Config.PublicServerAddress.IsEmpty() ? TEXT("<empty>") : *Config.PublicServerAddress,
		Config.PublicServerPort);
	if (Config.RaidServerId.IsEmpty() || Config.MatchId.IsEmpty() || Config.MapId.IsEmpty())
	{
		FailInitialization(TEXT("RaidServerId, MatchId, and MapId must be supplied to the dedicated server."));
		return;
	}
	FString PolicyError;
	if (!LoadPolicyData(PolicyError))
	{
		FailInitialization(PolicyError);
		return;
	}
	FRONTIER_LOG(Log, TEXT("[RaidStartup] Loot policy is ready; counting components by LootTableId."));

	TMap<FName, int32> ComponentCounts;
	for (const TWeakObjectPtr<UFrontierLootComponent>& WeakComponent : RegisteredComponents)
	{
		if (const UFrontierLootComponent* Component = WeakComponent.Get())
		{
			const FName TableId = Component->GetLootTableId();
			++ComponentCounts.FindOrAdd(TableId);
		}
	}

	if (ComponentCounts.IsEmpty())
	{
		FRONTIER_LOG(Log, TEXT("[RaidLootPool] No backend-managed LootComponents were found. Continuing ready gate with an empty pool."));
		NotifyBackendReady();
		return;
	}
	FRONTIER_LOG(Log, TEXT("[RaidStartup] Initial loot request will contain %d LootTableId entries."), ComponentCounts.Num());

	FFrontierRaidLootBatchRequest Request;
	Request.RaidServerId = Config.RaidServerId;
	Request.MapId = Config.MapId;
	Request.GenerationReason = TEXT("RAID_INITIALIZE");
	TArray<FName> SortedTableIds;
	ComponentCounts.GetKeys(SortedTableIds);
	SortedTableIds.Sort(FNameLexicalLess());
	for (const FName TableId : SortedTableIds)
	{
		const int32 ComponentCount = ComponentCounts.FindRef(TableId);
		if (!ApplyPolicyToPool(TableId, PolicyError))
		{
			FailInitialization(PolicyError);
			return;
		}
		const FFrontierRaidLootPoolPolicy* Policy = PolicyData->FindPolicy(TableId);
		int32 RequestedCount = 0;
		if (!Policy || !Policy->TryCalculateInitialRequestCount(ComponentCount, RequestedCount, PolicyError))
		{
			FailInitialization(PolicyError);
			return;
		}
		FFrontierRaidLootRequestEntry& Entry = Request.LootRequests.AddDefaulted_GetRef();
		Entry.LootTableId = TableId.ToString();
		Entry.RequestedCount = RequestedCount;
		FRONTIER_LOG(
			Log,
			TEXT("[RaidLootPool] Initial request. LootTableId=%s ComponentCount=%d InitialSpawnWaves=%d RequestedCount=%d"),
			*Entry.LootTableId,
			ComponentCount,
			Policy->InitialSpawnWaves,
			Entry.RequestedCount);
	}

	InitializationState = EFrontierRaidLootInitializationState::RequestingInitialBatch;
	if (InitialBatchIdempotencyKey.IsEmpty())
	{
		InitialBatchIdempotencyKey = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	}
	FRONTIER_LOG(
		Log,
		TEXT("[RaidStartup] Sending initial loot batch. MatchId=%s RaidServerId=%s RequestEntryCount=%d"),
		*Config.MatchId,
		*Request.RaidServerId,
		Request.LootRequests.Num());
	FString Error;
	const TWeakObjectPtr<UFrontierRaidLootPoolSubsystem> WeakThis(this);
	if (!InternalApi->RequestRaidLootBatch(
		Request,
		InitialBatchIdempotencyKey,
		[WeakThis](const FFrontierRaidLootBatchResponse& Response)
		{
			if (UFrontierRaidLootPoolSubsystem* This = WeakThis.Get())
			{
				This->HandleInitialBatch(Response);
			}
		},
		Error))
	{
		FailInitialization(Error);
		return;
	}
	FRONTIER_LOG(Log, TEXT("[RaidStartup] Initial loot batch request queued successfully."));
}

void UFrontierRaidLootPoolSubsystem::HandleInitialBatch(const FFrontierRaidLootBatchResponse& Response)
{
	FRONTIER_LOG(
		Log,
		TEXT("[RaidStartup] Initial loot batch response received. Transport=%d HttpStatus=%d Success=%d Retryable=%d ErrorCode=%s Message=%s MatchId=%s PoolCount=%d"),
		Response.bTransportSucceeded ? 1 : 0,
		Response.HttpStatus,
		Response.bSuccess ? 1 : 0,
		Response.bRetryable ? 1 : 0,
		Response.ErrorCode.IsEmpty() ? TEXT("<empty>") : *Response.ErrorCode,
		Response.Message.IsEmpty() ? TEXT("<empty>") : *Response.Message,
		Response.Data.MatchId.IsEmpty() ? TEXT("<empty>") : *Response.Data.MatchId,
		Response.Data.LootPools.Num());
	if (!Response.bTransportSucceeded || !Response.bSuccess)
	{
		FailInitialization(FString::Printf(
			TEXT("Initial loot batch failed. HttpStatus=%d ErrorCode=%s Message=%s"),
			Response.HttpStatus,
			*Response.ErrorCode,
			*Response.Message));
		return;
	}

	TSet<FName> RequiredTables;
	for (const TWeakObjectPtr<UFrontierLootComponent>& WeakComponent : RegisteredComponents)
	{
		if (const UFrontierLootComponent* Component = WeakComponent.Get())
		{
			RequiredTables.Add(Component->GetLootTableId());
		}
	}
	FString Error;
	FRONTIER_LOG(Log, TEXT("[RaidStartup] Validating and appending initial loot batch atomically. RequiredTableCount=%d"), RequiredTables.Num());
	if (!AppendBatchAtomically(Response, RequiredTables, Error))
	{
		FailInitialization(Error);
		return;
	}

	InitializationState = EFrontierRaidLootInitializationState::PreparingComponents;
	FRONTIER_LOG(Log, TEXT("[RaidStartup] Initial loot batch accepted. Preparing map LootComponents from the pool."));
	PrepareAllPendingComponents();
	FRONTIER_LOG(Log, TEXT("[RaidStartup] LootComponent preparation finished. PendingComponentCount=%d"), PendingComponents.Num());
	if (!PendingComponents.IsEmpty())
	{
		FailInitialization(TEXT("Initial loot batch did not contain enough entries for every map LootComponent."));
		return;
	}
	FRONTIER_LOG(Log, TEXT("[RaidStartup] All map LootComponents prepared. Proceeding to backend SERVER_READY notification."));
	NotifyBackendReady();
}

bool UFrontierRaidLootPoolSubsystem::AppendBatchAtomically(
	const FFrontierRaidLootBatchResponse& Response,
	const TSet<FName>& RequiredTables,
	FString& OutError)
{
	UFrontierItemCatalogSubsystem* Catalog = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierItemCatalogSubsystem>()
		: nullptr;
	const UFrontierInternalApiSubsystem* InternalApi = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierInternalApiSubsystem>()
		: nullptr;
	if (!Catalog || !InternalApi || Response.Data.MatchId != InternalApi->GetConfig().MatchId)
	{
		OutError = TEXT("Loot response has no item catalog or its matchId does not match this server process.");
		FRONTIER_LOG(
			Error,
			TEXT("[RaidStartup] Loot batch validation failed at catalog/match check. Catalog=%d InternalApi=%d ResponseMatchId=%s ConfigMatchId=%s"),
			Catalog ? 1 : 0,
			InternalApi ? 1 : 0,
			Response.Data.MatchId.IsEmpty() ? TEXT("<empty>") : *Response.Data.MatchId,
			InternalApi && !InternalApi->GetConfig().MatchId.IsEmpty() ? *InternalApi->GetConfig().MatchId : TEXT("<empty>"));
		return false;
	}

	TMap<FName, TArray<FPreparedEntry>> ConvertedPools;
	TSet<FGuid> CandidateRaidIds = SeenRaidItemIds;
	TSet<FString> CandidateSourceIds = SeenLootSourceIds;
	TSet<FString> CandidateEntryIds = SeenLootEntryIds;
	TSet<FName> ReturnedTables;
	for (const FFrontierRaidLootPoolDTO& SourcePool : Response.Data.LootPools)
	{
		const FName TableId(*SourcePool.LootTableId);
		ReturnedTables.Add(TableId);
		TArray<FPreparedEntry>& ConvertedEntries = ConvertedPools.FindOrAdd(TableId);
		for (const FFrontierRaidLootEntryDTO& SourceEntry : SourcePool.Entries)
		{
			if (CandidateEntryIds.Contains(SourceEntry.LootEntryId))
			{
				OutError = FString::Printf(TEXT("Duplicate lootEntryId received: %s"), *SourceEntry.LootEntryId);
				return false;
			}
			CandidateEntryIds.Add(SourceEntry.LootEntryId);
			FPreparedEntry& Entry = ConvertedEntries.AddDefaulted_GetRef();
			Entry.LootEntryId = SourceEntry.LootEntryId;
			for (const FFrontierOnlineRaidRuntimeItemDTO& SourceItem : SourceEntry.Items)
			{
				FGuid RaidItemId;
				if (!FGuid::Parse(SourceItem.RaidItemId, RaidItemId) || !RaidItemId.IsValid()
					|| CandidateRaidIds.Contains(RaidItemId)
					|| CandidateSourceIds.Contains(SourceItem.LootSourceId))
				{
					OutError = FString::Printf(
						TEXT("Loot batch contains invalid or duplicate identity. RaidItemId=%s LootSourceId=%s"),
						*SourceItem.RaidItemId,
						*SourceItem.LootSourceId);
					return false;
				}

				FFrontierItemInstance RuntimeItem;
				if (!FFrontierRaidRuntimeItemMapper::TryBuildRuntimeItem(
					SourceItem,
					*Catalog,
					true,
					RuntimeItem,
					OutError))
				{
					FRONTIER_LOG(
						Error,
						TEXT("[RaidStartup] Runtime loot item conversion failed. LootTableId=%s LootEntryId=%s RaidItemId=%s LootSourceId=%s Error=%s"),
						*SourcePool.LootTableId,
						*SourceEntry.LootEntryId,
						*SourceItem.RaidItemId,
						*SourceItem.LootSourceId,
						*OutError);
					return false;
				}
				CandidateRaidIds.Add(RaidItemId);
				CandidateSourceIds.Add(SourceItem.LootSourceId);
				FFrontierInventorySlot& Slot = Entry.Slots.AddDefaulted_GetRef();
				Slot.SlotIndex = Entry.Slots.Num() - 1;
				Slot.bOccupied = true;
				Slot.ItemInstance = MoveTemp(RuntimeItem);
			}
		}
	}

	for (const FName RequiredTable : RequiredTables)
	{
		if (!ReturnedTables.Contains(RequiredTable))
		{
			OutError = FString::Printf(TEXT("Loot response omitted requested pool %s."), *RequiredTable.ToString());
			return false;
		}
	}

	for (TPair<FName, TArray<FPreparedEntry>>& Pair : ConvertedPools)
	{
		Pools.FindOrAdd(Pair.Key).Entries.Append(MoveTemp(Pair.Value));
	}
	SeenRaidItemIds = MoveTemp(CandidateRaidIds);
	SeenLootSourceIds = MoveTemp(CandidateSourceIds);
	SeenLootEntryIds = MoveTemp(CandidateEntryIds);
	return true;
}

void UFrontierRaidLootPoolSubsystem::PrepareAllPendingComponents()
{
	FRONTIER_LOG(Log, TEXT("[RaidStartup] Preparing pending LootComponents. RegisteredComponentCount=%d"), RegisteredComponents.Num());
	TArray<TWeakObjectPtr<UFrontierLootComponent>> Components = RegisteredComponents.Array();
	Components.Sort([](
		const TWeakObjectPtr<UFrontierLootComponent>& Left,
		const TWeakObjectPtr<UFrontierLootComponent>& Right)
	{
		return GetPathNameSafe(Left.Get()) < GetPathNameSafe(Right.Get());
	});
	PendingComponents.Reset();
	for (const TWeakObjectPtr<UFrontierLootComponent>& WeakComponent : Components)
	{
		if (UFrontierLootComponent* Component = WeakComponent.Get())
		{
			if (!Component->IsBackendLootPrepared() && !TryPrepareComponent(Component))
			{
				PendingComponents.Add(Component);
			}
		}
	}
}

bool UFrontierRaidLootPoolSubsystem::TryPrepareComponent(UFrontierLootComponent* LootComponent)
{
	if (!IsValid(LootComponent) || LootComponent->IsBackendLootPrepared())
	{
		return IsValid(LootComponent);
	}
	FPoolState* Pool = Pools.Find(LootComponent->GetLootTableId());
	if (!Pool || Pool->Remaining() <= 0)
	{
		return false;
	}

	FPreparedEntry& Entry = Pool->Entries[Pool->NextEntryIndex++];
	LootComponent->AssignBackendLootSlots(MoveTemp(Entry.Slots));
	PendingComponents.Remove(LootComponent);
	FRONTIER_LOG(
		Log,
		TEXT("[RaidLootPool] Assigned entry. Owner=%s LootTableId=%s LootEntryId=%s Remaining=%d"),
		*GetNameSafe(LootComponent->GetOwner()),
		*LootComponent->GetLootTableId().ToString(),
		*Entry.LootEntryId,
		Pool->Remaining());
	if (InitializationState == EFrontierRaidLootInitializationState::Ready
		&& Pool->RefillRequestCount > 0
		&& Pool->Remaining() <= Pool->RefillThreshold)
	{
		QueueRefill(LootComponent->GetLootTableId());
	}
	return true;
}

void UFrontierRaidLootPoolSubsystem::QueueRefill(const FName LootTableId)
{
	if (LootTableId.IsNone() || InitializationState != EFrontierRaidLootInitializationState::Ready)
	{
		return;
	}
	const FPoolState* Pool = Pools.Find(LootTableId);
	if (!Pool || Pool->RefillRequestCount <= 0)
	{
		return;
	}
	PendingRefillTables.Add(LootTableId);
	if (bRefillRequestInFlight || bRefillFlushScheduled || !GetWorld())
	{
		return;
	}
	bRefillFlushScheduled = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(
		this,
		&UFrontierRaidLootPoolSubsystem::FlushRefillRequests));
}

void UFrontierRaidLootPoolSubsystem::FlushRefillRequests()
{
	bRefillFlushScheduled = false;
	if (bRefillRequestInFlight || PendingRefillTables.IsEmpty())
	{
		return;
	}
	UFrontierInternalApiSubsystem* InternalApi = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierInternalApiSubsystem>()
		: nullptr;
	if (!InternalApi)
	{
		return;
	}

	ActiveRefillTables = MoveTemp(PendingRefillTables);
	PendingRefillTables.Reset();
	ActiveRefillRequest = FFrontierRaidLootBatchRequest();
	ActiveRefillRequest.RaidServerId = InternalApi->GetConfig().RaidServerId;
	ActiveRefillRequest.GenerationReason = TEXT("MONSTER_RESPAWN");
	TArray<FName> SortedTables = ActiveRefillTables.Array();
	SortedTables.Sort(FNameLexicalLess());
	for (const FName TableId : SortedTables)
	{
		const FPoolState* Pool = Pools.Find(TableId);
		if (!Pool || Pool->RefillRequestCount <= 0)
		{
			continue;
		}
		FFrontierRaidLootRequestEntry& Entry = ActiveRefillRequest.LootRequests.AddDefaulted_GetRef();
		Entry.LootTableId = TableId.ToString();
		Entry.RequestedCount = Pool->RefillRequestCount;
	}
	if (ActiveRefillRequest.LootRequests.IsEmpty())
	{
		ActiveRefillTables.Reset();
		return;
	}
	ActiveRefillIdempotencyKey = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	RefillRetryAttempt = 0;
	SendActiveRefillRequest();
}

void UFrontierRaidLootPoolSubsystem::SendActiveRefillRequest()
{
	UFrontierInternalApiSubsystem* InternalApi = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierInternalApiSubsystem>()
		: nullptr;
	if (!InternalApi || ActiveRefillTables.IsEmpty() || ActiveRefillIdempotencyKey.IsEmpty())
	{
		bRefillRequestInFlight = false;
		return;
	}
	bRefillRequestInFlight = true;
	FString Error;
	const TWeakObjectPtr<UFrontierRaidLootPoolSubsystem> WeakThis(this);
	if (!InternalApi->RequestRaidLootRefill(
		ActiveRefillRequest,
		ActiveRefillIdempotencyKey,
		[WeakThis](const FFrontierRaidLootBatchResponse& Response)
		{
			if (UFrontierRaidLootPoolSubsystem* This = WeakThis.Get())
			{
				This->HandleRefillBatch(Response);
			}
		},
		Error))
	{
		bRefillRequestInFlight = false;
		FRONTIER_LOG(Error, TEXT("[RaidLootPool] Could not queue refill: %s"), *Error);
	}
}

void UFrontierRaidLootPoolSubsystem::HandleRefillBatch(
	const FFrontierRaidLootBatchResponse& Response)
{
	bRefillRequestInFlight = false;
	FString Error;
	if (!Response.bTransportSucceeded || !Response.bSuccess
		|| !AppendBatchAtomically(Response, ActiveRefillTables, Error))
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidLootPool] Refill failed. HttpStatus=%d ErrorCode=%s Error=%s Message=%s RetryAttempt=%d"),
			Response.HttpStatus,
			*Response.ErrorCode,
			*Error,
			*Response.Message,
			RefillRetryAttempt);
		const bool bRetryable = !Response.bTransportSucceeded || Response.bRetryable
			|| Response.HttpStatus == 429 || Response.HttpStatus >= 500;
		if (bRetryable && RefillRetryAttempt < 3 && GetWorld())
		{
			const float Delay = FMath::Pow(2.0f, static_cast<float>(RefillRetryAttempt++));
			bRefillRequestInFlight = true;
			GetWorld()->GetTimerManager().SetTimer(
				RefillRetryTimerHandle,
				this,
				&UFrontierRaidLootPoolSubsystem::SendActiveRefillRequest,
				Delay,
				false);
			return;
		}
		ActiveRefillTables.Reset();
		ActiveRefillRequest = FFrontierRaidLootBatchRequest();
		ActiveRefillIdempotencyKey.Reset();
		RefillRetryAttempt = 0;
		return;
	}
	ActiveRefillTables.Reset();
	ActiveRefillRequest = FFrontierRaidLootBatchRequest();
	ActiveRefillIdempotencyKey.Reset();
	RefillRetryAttempt = 0;
	PrepareAllPendingComponents();
	if (!PendingRefillTables.IsEmpty())
	{
		QueueRefill(*PendingRefillTables.CreateConstIterator());
	}
}

void UFrontierRaidLootPoolSubsystem::NotifyBackendReady()
{
	FRONTIER_LOG(Log, TEXT("[RaidStartup] Preparing SERVER_READY notification."));
	UFrontierInternalApiSubsystem* InternalApi = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierInternalApiSubsystem>()
		: nullptr;
	if (!InternalApi)
	{
		FRONTIER_LOG(Error, TEXT("[RaidStartup] SERVER_READY stopped: InternalApiSubsystem is unavailable."));
		FailInitialization(TEXT("Internal API subsystem is unavailable while notifying ready."));
		return;
	}
	const FFrontierInternalApiConfig& Config = InternalApi->GetConfig();
	FFrontierRaidServerReadyRequest Request;
	Request.RaidServerId = Config.RaidServerId;
	Request.MatchId = Config.MatchId;
	Request.MapId = Config.MapId;
	Request.ServerAddress = Config.PublicServerAddress;
	Request.Port = Config.PublicServerPort;
	Request.bLootReady = true;
	FRONTIER_LOG(
		Log,
		TEXT("[RaidStartup] SERVER_READY request built. RaidServerId=%s MatchId=%s MapId=%s Address=%s Port=%d LootReady=%d"),
		*Request.RaidServerId,
		*Request.MatchId,
		*Request.MapId,
		Request.ServerAddress.IsEmpty() ? TEXT("<empty>") : *Request.ServerAddress,
		Request.Port,
		Request.bLootReady ? 1 : 0);
	InitializationState = EFrontierRaidLootInitializationState::NotifyingReady;
	if (ReadyIdempotencyKey.IsEmpty())
	{
		ReadyIdempotencyKey = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	}

	FString Error;
	const TWeakObjectPtr<UFrontierRaidLootPoolSubsystem> WeakThis(this);
	if (!InternalApi->NotifyRaidServerReady(
		Request,
		ReadyIdempotencyKey,
		[WeakThis, Request](const FFrontierRaidServerReadyResponse& Response)
		{
			UFrontierRaidLootPoolSubsystem* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			FRONTIER_LOG(
				Log,
				TEXT("[RaidStartup] SERVER_READY response received. Transport=%d HttpStatus=%d Success=%d Accepted=%d Status=%s ErrorCode=%s Message=%s RaidServerId=%s MatchId=%s"),
				Response.bTransportSucceeded ? 1 : 0,
				Response.HttpStatus,
				Response.bSuccess ? 1 : 0,
				Response.bAccepted ? 1 : 0,
				Response.Status.IsEmpty() ? TEXT("<empty>") : *Response.Status,
				Response.ErrorCode.IsEmpty() ? TEXT("<empty>") : *Response.ErrorCode,
				Response.Message.IsEmpty() ? TEXT("<empty>") : *Response.Message,
				Response.RaidServerId.IsEmpty() ? TEXT("<empty>") : *Response.RaidServerId,
				Response.MatchId.IsEmpty() ? TEXT("<empty>") : *Response.MatchId);
			if (!Response.bTransportSucceeded || !Response.bSuccess || !Response.bAccepted
				|| Response.Status != TEXT("SERVER_READY")
				|| Response.RaidServerId != Request.RaidServerId
				|| Response.MatchId != Request.MatchId)
			{
				This->FailInitialization(FString::Printf(
					TEXT("Backend rejected server ready. HttpStatus=%d ErrorCode=%s Status=%s Message=%s"),
					Response.HttpStatus,
					*Response.ErrorCode,
					*Response.Status,
					*Response.Message));
				return;
			}
			This->InitializationState = EFrontierRaidLootInitializationState::Ready;
			FRONTIER_LOG(
				Log,
				TEXT("[RaidLootPool] Dedicated server accepted as ready. RaidServerId=%s MatchId=%s"),
				*Response.RaidServerId,
				*Response.MatchId);
		},
		Error))
	{
		FRONTIER_LOG(Error, TEXT("[RaidStartup] SERVER_READY request could not be queued: %s"), *Error);
		FailInitialization(Error);
		return;
	}
	FRONTIER_LOG(Log, TEXT("[RaidStartup] SERVER_READY request queued successfully."));
}

void UFrontierRaidLootPoolSubsystem::FailInitialization(const FString& Error)
{
	if (InitializationState == EFrontierRaidLootInitializationState::Failed)
	{
		FRONTIER_LOG(Warning, TEXT("[RaidStartup] Ignoring duplicate initialization failure after the subsystem is already failed."));
		return;
	}

	InitializationState = EFrontierRaidLootInitializationState::Failed;
	FRONTIER_LOG(Error, TEXT("[RaidStartup] Initialization state changed to Failed. Reason=%s"), *Error);
	FRONTIER_LOG(Error, TEXT("[RaidLootPool] Dedicated server readiness failed: %s"), *Error);

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UFrontierInternalApiSubsystem* InternalApi = GameInstance
		? GameInstance->GetSubsystem<UFrontierInternalApiSubsystem>()
		: nullptr;
	if (!InternalApi)
	{
		FRONTIER_LOG(Error, TEXT("[RaidLootPool] Could not report readiness failure because InternalApiSubsystem is unavailable."));
		return;
	}

	const FFrontierInternalApiConfig& Config = InternalApi->GetConfig();
	if (Config.RaidServerId.IsEmpty() || Config.MatchId.IsEmpty() || Config.MapId.IsEmpty())
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidLootPool] Could not report readiness failure because server identity is incomplete. RaidServerId=%s MatchId=%s MapId=%s"),
			Config.RaidServerId.IsEmpty() ? TEXT("<empty>") : *Config.RaidServerId,
			Config.MatchId.IsEmpty() ? TEXT("<empty>") : *Config.MatchId,
			Config.MapId.IsEmpty() ? TEXT("<empty>") : *Config.MapId);
		return;
	}

	FFrontierRaidServerFailureRequest Request;
	Request.RaidServerId = Config.RaidServerId;
	Request.MatchId = Config.MatchId;
	Request.MapId = Config.MapId;
	Request.ErrorCode = TEXT("DEDICATED_SERVER_INITIALIZATION_FAILED");
	Request.Message = Error;
	FRONTIER_LOG(
		Log,
		TEXT("[RaidStartup] Reporting initialization failure to backend. RaidServerId=%s MatchId=%s MapId=%s ErrorCode=%s"),
		*Request.RaidServerId,
		*Request.MatchId,
		*Request.MapId,
		*Request.ErrorCode);
	if (FailureIdempotencyKey.IsEmpty())
	{
		FailureIdempotencyKey = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	}

	FString StartError;
	const TWeakObjectPtr<UFrontierRaidLootPoolSubsystem> WeakThis(this);
	if (!InternalApi->NotifyRaidServerFailure(
		Request,
		FailureIdempotencyKey,
		[WeakThis, Request](const FFrontierInternalApiResponse& Response)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			if (!Response.bTransportSucceeded || !Response.bSucceeded)
			{
				FRONTIER_LOG(
					Error,
					TEXT("[RaidLootPool] Backend readiness failure report failed. HttpStatus=%d ErrorCode=%s Message=%s RaidServerId=%s MatchId=%s"),
					Response.HttpStatus,
					Response.ErrorCode.IsEmpty() ? TEXT("<empty>") : *Response.ErrorCode,
					Response.Message.IsEmpty() ? TEXT("<empty>") : *Response.Message,
					*Request.RaidServerId,
					*Request.MatchId);
				return;
			}
			FRONTIER_LOG(
				Log,
				TEXT("[RaidLootPool] Backend readiness failure reported. RaidServerId=%s MatchId=%s ErrorCode=%s"),
				*Request.RaidServerId,
				*Request.MatchId,
				*Request.ErrorCode);
		},
		StartError))
	{
		FRONTIER_LOG(Error, TEXT("[RaidStartup] Initialization failure report could not be queued: %s"), *StartError);
		FRONTIER_LOG(
			Error,
			TEXT("[RaidLootPool] Could not start backend readiness failure report. Error=%s"),
			StartError.IsEmpty() ? TEXT("<empty>") : *StartError);
	}
	else
	{
		FRONTIER_LOG(Log, TEXT("[RaidStartup] Initialization failure report queued successfully."));
	}
}
