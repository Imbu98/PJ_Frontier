#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tasks/Task.h"
#include "FrontierLocalProfileSubsystem.generated.h"

class UFrontierProfileSaveGame;

/**
 * Owns the local profile cache across map travel and serializes disk writes one at a time.
 */
UCLASS()
class FRONTIER_API UFrontierLocalProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	using FSaveCompletion = TFunction<void(bool)>;

	UFrontierProfileSaveGame* LoadOrCreateProfile(const FString& SlotName, int32 UserIndex);
	bool QueueProfileSave(UFrontierProfileSaveGame* Profile, const FString& SlotName, int32 UserIndex);
	bool QueueProfileSave(
		UFrontierProfileSaveGame* Profile,
		const FString& SlotName,
		int32 UserIndex,
		FSaveCompletion Completion);

	bool IsSaveInProgress() const { return bSaveInProgress; }
	bool HasPendingSave() const { return bSaveDirty; }
	bool IsWriteBlocked() const { return bWriteBlocked; }

	virtual void Deinitialize() override;

private:
	struct FPendingSaveCompletion
	{
		uint64 Revision = 0;
		FSaveCompletion Completion;
	};

	void StartAsyncSaveIfNeeded();
	void HandleAsyncSaveCompleted(const FString& SlotName, int32 UserIndex, uint64 SavedRevision, bool bSuccess);
	void ResolveSaveCompletions(uint64 ThroughRevision, bool bSuccess);
	void BlockWrites(const FString& Reason);

	UPROPERTY(Transient)
	TObjectPtr<UFrontierProfileSaveGame> CachedProfile;

	FString CachedSlotName;
	int32 CachedUserIndex = INDEX_NONE;
	bool bSaveInProgress = false;
	bool bSaveDirty = false;
	bool bWriteBlocked = false;
	bool bShuttingDown = false;
	int32 ConsecutiveSaveFailures = 0;
	uint64 LatestQueuedSaveRevision = 0;
	uint64 ActiveSaveRevision = 0;
	TArray<FPendingSaveCompletion> PendingSaveCompletions;
	UE::Tasks::FTask ActiveSaveTask;
};
