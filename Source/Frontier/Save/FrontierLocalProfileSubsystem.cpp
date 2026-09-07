#include "Save/FrontierLocalProfileSubsystem.h"

#include "Async/Async.h"
#include "Frontier.h"
#include "Kismet/GameplayStatics.h"
#include "PlatformFeatures.h"
#include "Save/FrontierProfileSaveGame.h"
#include "SaveGameSystem.h"

namespace
{
constexpr int32 MaxAutomaticSaveRetries = 2;
}

UFrontierProfileSaveGame* UFrontierLocalProfileSubsystem::LoadOrCreateProfile(
	const FString& SlotName,
	const int32 UserIndex)
{
	if (SlotName.IsEmpty() || UserIndex < 0 || bWriteBlocked)
	{
		return nullptr;
	}

	if (CachedProfile)
	{
		if (CachedSlotName == SlotName && CachedUserIndex == UserIndex)
		{
			return CachedProfile;
		}

		BlockWrites(TEXT("Attempted to switch local profile slots while a cached profile is active."));
		return nullptr;
	}

	const bool bSaveExists = UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
	USaveGame* LoadedObject = bSaveExists
		? UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex)
		: nullptr;

	if (bSaveExists && !LoadedObject)
	{
		BlockWrites(TEXT("The existing local profile could not be loaded. It will not be overwritten."));
		return nullptr;
	}

	UFrontierProfileSaveGame* Profile = Cast<UFrontierProfileSaveGame>(LoadedObject);
	if (LoadedObject && !Profile)
	{
		BlockWrites(TEXT("The existing save slot contains an incompatible SaveGame class."));
		return nullptr;
	}

	if (!Profile)
	{
		Profile = Cast<UFrontierProfileSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UFrontierProfileSaveGame::StaticClass()));
		if (!Profile)
		{
			BlockWrites(TEXT("Failed to create the local profile SaveGame object."));
			return nullptr;
		}
		Profile->InitializeNewProfile();
	}

	bool bWasMigrated = false;
	FString MigrationError;
	if (!Profile->MigrateToLatest(bWasMigrated, MigrationError))
	{
		BlockWrites(MigrationError);
		return nullptr;
	}

	CachedProfile = Profile;
	CachedSlotName = SlotName;
	CachedUserIndex = UserIndex;

	if (bWasMigrated)
	{
		FRONTIER_LOG(Log, TEXT("Migrated local profile '%s' to save version %d."), *SlotName, Profile->SaveVersion);
		QueueProfileSave(Profile, SlotName, UserIndex);
	}

	return CachedProfile;
}

bool UFrontierLocalProfileSubsystem::QueueProfileSave(
	UFrontierProfileSaveGame* Profile,
	const FString& SlotName,
	const int32 UserIndex)

{
	return QueueProfileSave(Profile, SlotName, UserIndex, FSaveCompletion());
}

bool UFrontierLocalProfileSubsystem::QueueProfileSave(
	UFrontierProfileSaveGame* Profile,
	const FString& SlotName,
	const int32 UserIndex,
	FSaveCompletion Completion)
{
	if (bWriteBlocked
		|| !Profile
		|| Profile != CachedProfile
		|| SlotName != CachedSlotName
		|| UserIndex != CachedUserIndex)
	{
		return false;
	}

	bool bWasMigrated = false;
	FString MigrationError;
	if (!Profile->MigrateToLatest(bWasMigrated, MigrationError))
	{
		BlockWrites(MigrationError);
		return false;
	}

	bSaveDirty = true;
	const uint64 QueuedRevision = ++LatestQueuedSaveRevision;
	if (Completion)
	{
		FPendingSaveCompletion& PendingCompletion = PendingSaveCompletions.AddDefaulted_GetRef();
		PendingCompletion.Revision = QueuedRevision;
		PendingCompletion.Completion = MoveTemp(Completion);
	}
	if (!bSaveInProgress)
	{
		ConsecutiveSaveFailures = 0;
	}
	StartAsyncSaveIfNeeded();
	return true;
}

void UFrontierLocalProfileSubsystem::StartAsyncSaveIfNeeded()
{
	if (bWriteBlocked || bShuttingDown || bSaveInProgress || !bSaveDirty || !CachedProfile)
	{
		return;
	}

	TSharedRef<TArray<uint8>, ESPMode::ThreadSafe> SaveData = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>();
	ISaveGameSystem* SaveSystem = IPlatformFeaturesModule::Get().GetSaveGameSystem();
	if (!SaveSystem || !UGameplayStatics::SaveGameToMemory(CachedProfile, *SaveData) || SaveData->IsEmpty())
	{
		HandleAsyncSaveCompleted(CachedSlotName, CachedUserIndex, LatestQueuedSaveRevision, false);
		return;
	}

	bSaveDirty = false;
	bSaveInProgress = true;
	ActiveSaveRevision = LatestQueuedSaveRevision;

	const FString SlotName = CachedSlotName;
	const int32 UserIndex = CachedUserIndex;
	const uint64 SavedRevision = ActiveSaveRevision;
	const TWeakObjectPtr<UFrontierLocalProfileSubsystem> WeakThis(this);
	ActiveSaveTask = UE::Tasks::Launch(
		UE_SOURCE_LOCATION,
		[WeakThis, SaveSystem, SaveData, SlotName, UserIndex, SavedRevision]()
		{
			const bool bSuccess = SaveSystem->SaveGame(false, *SlotName, UserIndex, *SaveData);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, SlotName, UserIndex, SavedRevision, bSuccess]()
			{
				if (UFrontierLocalProfileSubsystem* Subsystem = WeakThis.Get())
				{
					Subsystem->HandleAsyncSaveCompleted(SlotName, UserIndex, SavedRevision, bSuccess);
				}
			});
		},
		UE::Tasks::ETaskPriority::Normal,
		UE::Tasks::EExtendedTaskPriority::None,
		UE::Tasks::ETaskFlags::DoNotRunInsideBusyWait);
}

void UFrontierLocalProfileSubsystem::HandleAsyncSaveCompleted(
	const FString& SlotName,
	const int32 UserIndex,
	const uint64 SavedRevision,
	const bool bSuccess)
{
	if (bShuttingDown)
	{
		return;
	}

	if (bWriteBlocked && !CachedProfile)
	{
		return;
	}

	if (SlotName != CachedSlotName || UserIndex != CachedUserIndex)
	{
		BlockWrites(TEXT("Received an async save callback for an unexpected profile slot."));
		return;
	}

	bSaveInProgress = false;
	if (bSuccess)
	{
		ConsecutiveSaveFailures = 0;
		ResolveSaveCompletions(SavedRevision, true);
		StartAsyncSaveIfNeeded();
		return;
	}

	bSaveDirty = true;
	++ConsecutiveSaveFailures;
	FRONTIER_LOG(Warning, TEXT("Async local profile save failed. Slot=%s Attempt=%d"), *SlotName, ConsecutiveSaveFailures);
	if (ConsecutiveSaveFailures <= MaxAutomaticSaveRetries)
	{
		StartAsyncSaveIfNeeded();
		return;
	}

	ResolveSaveCompletions(SavedRevision, false);
}

void UFrontierLocalProfileSubsystem::ResolveSaveCompletions(const uint64 ThroughRevision, const bool bSuccess)
{
	TArray<FSaveCompletion> Completions;
	for (int32 Index = PendingSaveCompletions.Num() - 1; Index >= 0; --Index)
	{
		if (PendingSaveCompletions[Index].Revision <= ThroughRevision)
		{
			Completions.Add(MoveTemp(PendingSaveCompletions[Index].Completion));
			PendingSaveCompletions.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}

	for (FSaveCompletion& Completion : Completions)
	{
		if (Completion)
		{
			Completion(bSuccess);
		}
	}
}

void UFrontierLocalProfileSubsystem::BlockWrites(const FString& Reason)
{
	bWriteBlocked = true;
	bSaveInProgress = false;
	bSaveDirty = false;
	ResolveSaveCompletions(TNumericLimits<uint64>::Max(), false);
	FRONTIER_LOG(Error, TEXT("Local profile writes blocked to protect existing data. Reason=%s"), *Reason);
}

void UFrontierLocalProfileSubsystem::Deinitialize()
{
	bShuttingDown = true;
	if (ActiveSaveTask.IsValid() && !ActiveSaveTask.IsCompleted())
	{
		ActiveSaveTask.Wait();
	}

	if ((bSaveDirty || bSaveInProgress) && CachedProfile && !bWriteBlocked)
	{
		const bool bSaved = UGameplayStatics::SaveGameToSlot(CachedProfile, CachedSlotName, CachedUserIndex);
		ResolveSaveCompletions(LatestQueuedSaveRevision, bSaved);
		if (!bSaved)
		{
			FRONTIER_LOG(Error, TEXT("Failed to flush pending local profile data during GameInstance shutdown. Slot=%s"), *CachedSlotName);
		}
	}
	CachedProfile = nullptr;
	CachedSlotName.Reset();
	CachedUserIndex = INDEX_NONE;
	bWriteBlocked = true;
	ResolveSaveCompletions(TNumericLimits<uint64>::Max(), false);
	Super::Deinitialize();
}
