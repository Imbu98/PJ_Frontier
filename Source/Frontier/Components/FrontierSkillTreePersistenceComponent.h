#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkillTree/FrontierSkillTreeTypes.h"
#include "FrontierSkillTreePersistenceComponent.generated.h"

class UFrontierProfileSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FFrontierSkillTreeInitialSyncCompletedSignature,
	bool,
	bSucceeded);

/**
 * Temporary local-profile transport for permanent skill-tree progression.
 *
 * The runtime skill tree deliberately does not know where progression is stored. When backend
 * progression is available, replace the local load/save boundary in this component and make the
 * server load the authoritative snapshot; node unlock and GAS application code can remain unchanged.
 */
UCLASS(ClassGroup=(Frontier), meta=(BlueprintSpawnableComponent))
class FRONTIER_API UFrontierSkillTreePersistenceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFrontierSkillTreePersistenceComponent();

	/** Starts one local-profile restore and binds subsequent replicated changes for saving. */
	UFUNCTION(BlueprintCallable, Category="Frontier|Skill Tree|Persistence")
	void StartSkillTreeSync();

	/** Starts or refreshes account-isolated reconciliation after every verified Backend level GET. */
	UFUNCTION(BlueprintCallable, Category="Frontier|Skill Tree|Persistence")
	void StartSkillTreeSyncForAccount(const FString& AccountId);

	/** Immediately snapshots the locally observed owner state into the queued local profile save. */
	UFUNCTION(BlueprintCallable, Category="Frontier|Skill Tree|Persistence")
	bool SaveCurrentSkillTree();

	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree|Persistence")
	bool IsInitialSyncComplete() const { return bInitialSyncComplete; }

	UPROPERTY(BlueprintAssignable, Category="Frontier|Skill Tree|Persistence")
	FFrontierSkillTreeInitialSyncCompletedSignature OnInitialSyncCompleted;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * The local node allocation is untrusted. The server validates it against its own cached
	 * Backend level and current DA costs; client-provided point totals are ignored.
	 */
	UFUNCTION(Server, Reliable)
	void ServerApplyLocalSkillTreeSnapshot(
		const FString& AccountId,
		const FFrontierSkillTreeProgressionSnapshot& Snapshot);

	UFUNCTION(Client, Reliable)
	void ClientCompleteInitialSkillTreeSync(
		bool bSucceeded,
		const FFrontierSkillTreeProgressionSnapshot& AuthoritativeSnapshot);

	UFUNCTION()
	void HandleSkillTreeChanged();

private:
	void AttemptInitializeSkillTreeSync();
	void BindSkillTreeDelegate();
	bool SaveSnapshotToLocalProfile(const FFrontierSkillTreeProgressionSnapshot& Snapshot) const;
	UFrontierProfileSaveGame* LoadOrCreateLocalProfile() const;
	FString ResolveProfileSaveSlotName() const;
	FString ResolveProgressionAccountId() const;

	FTimerHandle InitialSyncRetryTimerHandle;
	bool bSyncStarted = false;
	bool bSkillTreeDelegateBound = false;
	bool bInitialSyncComplete = false;
	bool bSyncRequestPending = false;
	bool bReconcileAgainAfterPending = false;
	FString ProgressionAccountId;
};
