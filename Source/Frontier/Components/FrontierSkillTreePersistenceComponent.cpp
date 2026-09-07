#include "Components/FrontierSkillTreePersistenceComponent.h"

#include "Components/FrontierSkillTreeComponent.h"
#include "Components/FrontierBackendProtocolComponent.h"
#include "Engine/GameInstance.h"
#include "Frontier.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierLobbyPlayerController.h"
#include "Game/FrontierPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Save/FrontierLocalProfileSubsystem.h"
#include "Save/FrontierProfileSaveGame.h"
#include "Progression/FrontierTemporarySkillPointSubsystem.h"
#include "TimerManager.h"

namespace
{
constexpr TCHAR FrontierSkillTreeProfileSaveSlotName[] = TEXT("FrontierLocalProfile");
constexpr int32 FrontierSkillTreeProfileSaveUserIndex = 0;
constexpr float InitialSyncRetryDelaySeconds = 0.2f;

FString BuildSkillTreeProfileSaveSlotName(const UWorld* World)
{
	FString SlotName = FrontierSkillTreeProfileSaveSlotName;
	if (!World)
	{
		return SlotName;
	}

	const FString WorldPackageName = World->GetOutermost()->GetName();
	const int32 PieIndex = WorldPackageName.Find(TEXT("UEDPIE_"), ESearchCase::IgnoreCase);
	if (PieIndex == INDEX_NONE)
	{
		return SlotName;
	}

	TArray<FString> Parts;
	WorldPackageName.Mid(PieIndex).ParseIntoArray(Parts, TEXT("_"), true);
	if (Parts.Num() >= 2)
	{
		SlotName += FString::Printf(TEXT("_PIE_%s"), *Parts[1]);
	}
	return SlotName;
}

UFrontierBackendProtocolComponent* ResolveBackendProtocolComponent(const APlayerController* Controller)
{
	if (const AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(Controller))
	{
		return LobbyController->GetBackendProtocolComponent();
	}
	if (const AFrontierPlayerController* FrontierController = Cast<AFrontierPlayerController>(Controller))
	{
		return FrontierController->GetBackendProtocolComponent();
	}
	return nullptr;
}

FString ResolveVerifiedBackendAccountId(const UFrontierBackendProtocolComponent* BackendProtocol)
{
	if (!BackendProtocol)
	{
		return FString();
	}
	const FFrontierBackendSteamLoginResult& Login = BackendProtocol->GetLastSteamLoginResult();
	return !Login.Player.PlayerIdString.IsEmpty() ? Login.Player.PlayerIdString : Login.Player.SteamId;
}

bool AreSnapshotsEquivalent(
	const FFrontierSkillTreeProgressionSnapshot& Left,
	const FFrontierSkillTreeProgressionSnapshot& Right)
{
	if (Left.AvailableSkillPoints != Right.AvailableSkillPoints
		|| Left.TotalSkillPointsEarned != Right.TotalSkillPointsEarned
		|| Left.Nodes.Num() != Right.Nodes.Num())
	{
		return false;
	}

	TMap<FGameplayTag, int32> LeftRanks;
	for (const FFrontierSavedSkillTreeNode& Node : Left.Nodes)
	{
		LeftRanks.FindOrAdd(Node.NodeTag) = Node.Rank;
	}
	for (const FFrontierSavedSkillTreeNode& Node : Right.Nodes)
	{
		if (LeftRanks.FindRef(Node.NodeTag) != Node.Rank)
		{
			return false;
		}
	}
	return true;
}
}

UFrontierSkillTreePersistenceComponent::UFrontierSkillTreePersistenceComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UFrontierSkillTreePersistenceComponent::StartSkillTreeSync()
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFrontierTemporarySkillPointSubsystem* ProgressionSubsystem = GameInstance
		? GameInstance->GetSubsystem<UFrontierTemporarySkillPointSubsystem>()
		: nullptr;
	StartSkillTreeSyncForAccount(ProgressionSubsystem ? ProgressionSubsystem->GetActiveAccountId() : FString());
}

void UFrontierSkillTreePersistenceComponent::StartSkillTreeSyncForAccount(const FString& AccountId)
{
	APlayerController* OwnerController = Cast<APlayerController>(GetOwner());
	if (!OwnerController || !OwnerController->IsLocalController())
	{
		return;
	}
	if (AccountId.IsEmpty())
	{
		FRONTIER_LOG(Verbose, TEXT("Skill-tree sync deferred until a verified account is active. Controller=%s"), *GetNameSafe(OwnerController));
		return;
	}
	if (bSyncStarted && !ProgressionAccountId.Equals(AccountId, ESearchCase::CaseSensitive))
	{
		FRONTIER_LOG(Error, TEXT("Skill-tree sync rejected an account switch on the same controller."));
		return;
	}
	if (bSyncRequestPending)
	{
		bReconcileAgainAfterPending = true;
		FRONTIER_LOG(Verbose, TEXT("Skill-tree reconciliation is already pending. AccountId=%s"), *AccountId);
		return;
	}

	ProgressionAccountId = AccountId;
	bSyncStarted = true;
	AttemptInitializeSkillTreeSync();
}

bool UFrontierSkillTreePersistenceComponent::SaveCurrentSkillTree()
{
	const APlayerController* OwnerController = Cast<APlayerController>(GetOwner());
	const AFrontierPlayerState* PlayerState = OwnerController
		? OwnerController->GetPlayerState<AFrontierPlayerState>()
		: nullptr;
	const UFrontierSkillTreeComponent* SkillTree = PlayerState
		? PlayerState->GetSkillTreeComponent()
		: nullptr;
	if (!OwnerController || !OwnerController->IsLocalController() || !SkillTree || !bInitialSyncComplete)
	{
		return false;
	}

	FFrontierSkillTreeProgressionSnapshot Snapshot;
	SkillTree->BuildPersistentSnapshot(Snapshot);
	return SaveSnapshotToLocalProfile(Snapshot);
}

void UFrontierSkillTreePersistenceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InitialSyncRetryTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UFrontierSkillTreePersistenceComponent::AttemptInitializeSkillTreeSync()
{
	APlayerController* OwnerController = Cast<APlayerController>(GetOwner());
	if (!OwnerController || !OwnerController->IsLocalController() || bSyncRequestPending)
	{
		return;
	}

	AFrontierPlayerState* PlayerState = OwnerController->GetPlayerState<AFrontierPlayerState>();
	if (!PlayerState || !PlayerState->GetSkillTreeComponent())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				InitialSyncRetryTimerHandle,
				this,
				&UFrontierSkillTreePersistenceComponent::AttemptInitializeSkillTreeSync,
				InitialSyncRetryDelaySeconds,
				false);
		}
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InitialSyncRetryTimerHandle);
	}

	UFrontierProfileSaveGame* Profile = LoadOrCreateLocalProfile();
	if (!Profile)
	{
		FRONTIER_LOG(Error, TEXT("Skill-tree local profile could not be loaded. Controller=%s"), *GetNameSafe(OwnerController));
		OnInitialSyncCompleted.Broadcast(false);
		return;
	}

	const FFrontierLocalAccountProgression* Account = Profile->FindAccountProgression(ResolveProgressionAccountId());
	if (!Account)
	{
		FRONTIER_LOG(Error, TEXT("Skill-tree account progression is unavailable. AccountId=%s"), *ResolveProgressionAccountId());
		OnInitialSyncCompleted.Broadcast(false);
		return;
	}

	BindSkillTreeDelegate();
	bSyncRequestPending = true;
	ServerApplyLocalSkillTreeSnapshot(ResolveProgressionAccountId(), Account->SkillTreeProgression);
}

void UFrontierSkillTreePersistenceComponent::BindSkillTreeDelegate()
{
	if (bSkillTreeDelegateBound)
	{
		return;
	}

	const APlayerController* OwnerController = Cast<APlayerController>(GetOwner());
	AFrontierPlayerState* PlayerState = OwnerController
		? OwnerController->GetPlayerState<AFrontierPlayerState>()
		: nullptr;
	UFrontierSkillTreeComponent* SkillTree = PlayerState
		? PlayerState->GetSkillTreeComponent()
		: nullptr;
	if (!SkillTree)
	{
		return;
	}

	SkillTree->OnSkillTreeChanged.AddDynamic(this, &UFrontierSkillTreePersistenceComponent::HandleSkillTreeChanged);
	bSkillTreeDelegateBound = true;
}

void UFrontierSkillTreePersistenceComponent::HandleSkillTreeChanged()
{
	SaveCurrentSkillTree();
}

void UFrontierSkillTreePersistenceComponent::ServerApplyLocalSkillTreeSnapshot_Implementation(
	const FString& AccountId,
	const FFrontierSkillTreeProgressionSnapshot& Snapshot)
{
	APlayerController* OwnerController = Cast<APlayerController>(GetOwner());
	AFrontierPlayerState* PlayerState = OwnerController
		? OwnerController->GetPlayerState<AFrontierPlayerState>()
		: nullptr;
	UFrontierSkillTreeComponent* SkillTree = PlayerState
		? PlayerState->GetSkillTreeComponent()
		: nullptr;

	const UFrontierBackendProtocolComponent* BackendProtocol = ResolveBackendProtocolComponent(OwnerController);
	const FString VerifiedAccountId = ResolveVerifiedBackendAccountId(BackendProtocol);
	const int32 VerifiedBackendLevel = BackendProtocol ? BackendProtocol->GetLastPlayerLevel().Level : 0;
	const bool bIdentityMatches = !AccountId.IsEmpty()
		&& AccountId.Equals(VerifiedAccountId, ESearchCase::CaseSensitive);
	bool bAllocationReset = false;
	const bool bApplied = bIdentityMatches
		&& VerifiedBackendLevel > 0
		&& SkillTree
		&& SkillTree->ApplyPersistentSnapshotWithPointBudget(
			Snapshot,
			UFrontierTemporarySkillPointSubsystem::CalculateTotalSkillPointsForLevel(VerifiedBackendLevel),
			bAllocationReset);
	if (bAllocationReset)
	{
		FRONTIER_LOG(Warning, TEXT("Skill-tree allocation exceeded the verified point budget and was reset. AccountId=%s Level=%d"), *AccountId, VerifiedBackendLevel);
	}
	FFrontierSkillTreeProgressionSnapshot SanitizedSnapshot;
	if (bApplied)
	{
		SkillTree->BuildPersistentSnapshot(SanitizedSnapshot);
	}
	ClientCompleteInitialSkillTreeSync(bApplied, SanitizedSnapshot);
}

void UFrontierSkillTreePersistenceComponent::ClientCompleteInitialSkillTreeSync_Implementation(
	const bool bSucceeded,
	const FFrontierSkillTreeProgressionSnapshot& AuthoritativeSnapshot)
{
	bSyncRequestPending = false;
	if (bSucceeded)
	{
		bInitialSyncComplete = true;
		// Persist the server-sanitized form so removed nodes or invalid dependencies are not kept on disk.
		SaveSnapshotToLocalProfile(AuthoritativeSnapshot);
	}
	OnInitialSyncCompleted.Broadcast(bSucceeded);
	if (bReconcileAgainAfterPending)
	{
		bReconcileAgainAfterPending = false;
		AttemptInitializeSkillTreeSync();
	}
}

bool UFrontierSkillTreePersistenceComponent::SaveSnapshotToLocalProfile(
	const FFrontierSkillTreeProgressionSnapshot& Snapshot) const
{
	UFrontierProfileSaveGame* Profile = LoadOrCreateLocalProfile();
	if (!Profile)
	{
		return false;
	}

	FFrontierLocalAccountProgression* Account = Profile->FindAccountProgression(ResolveProgressionAccountId());
	if (!Account)
	{
		return false;
	}
	const int32 CachedSpentPoints = FMath::Max(
		0,
		Snapshot.TotalSkillPointsEarned - Snapshot.AvailableSkillPoints);
	if (AreSnapshotsEquivalent(Account->SkillTreeProgression, Snapshot)
		&& Account->TemporarySkillPoints == Snapshot.AvailableSkillPoints
		&& Account->CachedSpentSkillPoints == CachedSpentPoints)
	{
		return true;
	}
	Account->SkillTreeProgression = Snapshot;
	Account->TemporarySkillPoints = Snapshot.AvailableSkillPoints;
	Account->CachedSpentSkillPoints = CachedSpentPoints;
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UFrontierLocalProfileSubsystem* ProfileSubsystem = GameInstance
		? GameInstance->GetSubsystem<UFrontierLocalProfileSubsystem>()
		: nullptr;
	return ProfileSubsystem
		&& ProfileSubsystem->QueueProfileSave(Profile, ResolveProfileSaveSlotName(), FrontierSkillTreeProfileSaveUserIndex);
}

UFrontierProfileSaveGame* UFrontierSkillTreePersistenceComponent::LoadOrCreateLocalProfile() const
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UFrontierLocalProfileSubsystem* ProfileSubsystem = GameInstance
		? GameInstance->GetSubsystem<UFrontierLocalProfileSubsystem>()
		: nullptr;
	return ProfileSubsystem
		? ProfileSubsystem->LoadOrCreateProfile(ResolveProfileSaveSlotName(), FrontierSkillTreeProfileSaveUserIndex)
		: nullptr;
}

FString UFrontierSkillTreePersistenceComponent::ResolveProfileSaveSlotName() const
{
	return BuildSkillTreeProfileSaveSlotName(GetWorld());
}

FString UFrontierSkillTreePersistenceComponent::ResolveProgressionAccountId() const
{
	if (!ProgressionAccountId.IsEmpty())
	{
		return ProgressionAccountId;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFrontierTemporarySkillPointSubsystem* ProgressionSubsystem = GameInstance
		? GameInstance->GetSubsystem<UFrontierTemporarySkillPointSubsystem>()
		: nullptr;
	return ProgressionSubsystem ? ProgressionSubsystem->GetActiveAccountId() : FString();
}
