#pragma once

#include "CoreMinimal.h"
#include "Progression/FrontierLevelProgressionTypes.h"
#include "Progression/FrontierRaidExperienceTypes.h"
#include "Raid/FrontierOnlineRaidTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FrontierRaidSessionSubsystem.generated.h"

class AFrontierLobbyPlayerController;
class APlayerController;
class AFrontierPlayerState;
class FFrontierOnlineHttpClient;
class UFrontierInternalApiSubsystem;

UENUM(BlueprintType)
enum class EFrontierRaidFlowState : uint8
{
	Idle,
	CreatingEntry,
	AuthorizingJoin,
	RaidActiveSimulated,
	FinalizingLocalResult,
	CommittingRaidResult,
	GrantingExperience,
	ReturningToLobby,
	RefreshingLobby,
	Completed,
	RetryableFailure
};

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierRaidResultSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid")
	FString RaidSessionId;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid")
	FString Outcome;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid")
	FString ReasonCode;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid")
	FString CommittedAt;

};

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierRaidSettlementPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid")
	FString RaidSessionId;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid")
	EFrontierRaidOutcome Outcome = EFrontierRaidOutcome::Dead;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid")
	int64 AwardedExperience = 0;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid")
	FFrontierPlayerLevelSnapshot BeforeLevel;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid")
	FFrontierPlayerLevelSnapshot AfterLevel;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FFrontierRaidFlowStateChangedSignature,
	EFrontierRaidFlowState,
	State,
	const FString&,
	Message);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FFrontierRaidResultSnapshotChangedSignature,
	const FFrontierRaidResultSnapshot&,
	Result);

/** Central owner of client entry context and dedicated-server settlement ordering. */
UCLASS()
class FRONTIER_API UFrontierRaidSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool BeginClientRaidEntry(FString& OutError);
	void AcceptClientRaidEntry(const FFrontierOnlineRaidEntryDTO& Entry);
	void AcceptClientRaidEntryContext(const FString& RaidSessionId);
	void FailClientFlow(const FString& Error, bool bRetryable);
	void MarkClientRaidActive(const FString& RaidSessionId);
	void MarkClientSettlementState(EFrontierRaidFlowState NewState, const FString& Message = FString());
	void BeginClientLobbyRefresh();
	void CompleteClientLobbyRefresh(const FFrontierOnlineRaidResultDTO& Result);

	bool BeginServerJoinAuthorization(
		APlayerController* Controller,
		const FFrontierOnlineRaidEntryDTO& Entry,
		FString& OutError);
	void RemoveServerRaidContext(APlayerController* Controller);
	bool HasServerRaidContext(APlayerController* Controller) const;
	bool IsServerRaidActive(APlayerController* Controller) const;
	bool TryAwardDebugExperience(
		APlayerController* Controller,
		const FString& EventId,
		FString& OutError);
	bool BeginServerSettlement(
		APlayerController* Controller,
		EFrontierRaidOutcome Outcome,
		FString& OutError);
	bool BeginServerLobbyRefresh(
		APlayerController* Controller,
		const FString& RaidSessionId,
		FString& OutError);
	void CompleteServerLobbyRefresh(
		APlayerController* Controller,
		const FString& RaidSessionId);
	void MarkServerLobbyRefreshFailed(
		APlayerController* Controller,
		const FString& RaidSessionId);

	void CachePreRaidLevel(const FFrontierPlayerLevelSnapshot& Level);
	void MarkClientSettlementCommitted(
		const FString& RaidSessionId,
		EFrontierRaidOutcome Outcome,
		int64 AwardedExperience);
	bool CompletePendingSettlementPresentation(
		const FFrontierPlayerLevelSnapshot& AuthoritativeAfterLevel,
		FFrontierRaidSettlementPresentation& OutPresentation);
	bool ConsumeSettlementPresentation(FFrontierRaidSettlementPresentation& OutPresentation);
	bool IsAwaitingSettlementLevelRefresh() const { return bAwaitingSettlementLevelRefresh; }

	UFUNCTION(BlueprintPure, Category="Frontier|Raid")
	EFrontierRaidFlowState GetClientState() const { return ClientState; }

	UFUNCTION(BlueprintPure, Category="Frontier|Raid")
	bool IsClientRaidActive() const { return ClientState == EFrontierRaidFlowState::RaidActiveSimulated; }

	const FString& GetLastRaidSessionId() const { return ClientRaidSessionId; }
	const FFrontierRaidResultSnapshot& GetLastResult() const { return LastResult; }

	UPROPERTY(BlueprintAssignable, Category="Frontier|Raid")
	FFrontierRaidFlowStateChangedSignature OnRaidFlowStateChanged;

	UPROPERTY(BlueprintAssignable, Category="Frontier|Raid")
	FFrontierRaidResultSnapshotChangedSignature OnRaidResultChanged;

	static bool CanTransition(EFrontierRaidFlowState From, EFrontierRaidFlowState To);
	static bool IsDebugExperienceAllowed(EFrontierRaidFlowState State);
	static bool DoesRaidResultConfirmCommit(
		const FFrontierOnlineRaidResultDTO& Result,
		const FString& ExpectedRaidSessionId,
		int64 ExpectedPlayerId,
		EFrontierRaidOutcome ExpectedOutcome);

private:
	struct FServerRaidContext
	{
		TWeakObjectPtr<APlayerController> Controller;
		TWeakObjectPtr<AFrontierPlayerState> PlayerState;
		EFrontierRaidFlowState State = EFrontierRaidFlowState::AuthorizingJoin;
		FString RaidSessionId;
		FString ServerId;
		FString JoinToken;
		int64 PlayerId = 0;
		FString SteamId;
		FString PlayerConnectionId;
		FFrontierOnlineRaidLoadoutManifestDTO LoadoutManifest;
		FString ServerResultId;
		int32 ResultSequence = 1;
		FString ResultOccurredAt;
		FString ResultEventDigest;
		FString FrozenResultBody;
		FString FrozenGrantBody;
		EFrontierRaidOutcome Outcome = EFrontierRaidOutcome::Dead;
		int64 FinalExperience = 0;
		bool bHasGrantedLevel = false;
		FFrontierOnlinePlayerLevelDTO GrantedLevel;
	};

	void TransitionClient(EFrontierRaidFlowState NewState, const FString& Message);
	void HandleJoinAuthorizationCompleted(
		const FString& RaidSessionId,
		const struct FFrontierInternalApiResponse& TransportResponse);
	void SubmitServerResultCommit(const FString& RaidSessionId);
	void HandleServerResultCommitCompleted(
		const FString& RaidSessionId,
		const struct FFrontierInternalApiResponse& TransportResponse);
	void ConfirmServerResultAfterConflict(
		const FString& RaidSessionId,
		bool bAfterAccessTokenRefresh = false);
	void HandleServerResultConflictConfirmation(
		const FString& RaidSessionId,
		bool bAfterAccessTokenRefresh,
		const FFrontierOnlineRaidResultResponse& Response);
	void ContinueAfterServerResultCommit(const FString& RaidSessionId);
	void SubmitExperienceGrant(const FString& RaidSessionId);
	void HandleExperienceGrantCompleted(
		const FString& RaidSessionId,
		const struct FFrontierInternalApiResponse& TransportResponse);
	void FailServerFlow(const FString& RaidSessionId, const FString& Error, bool bRetryable);

	bool BuildFrozenResultBody(FServerRaidContext& Context, FString& OutError) const;
	bool BuildFrozenGrantBody(FServerRaidContext& Context, FString& OutError) const;

	EFrontierRaidFlowState ClientState = EFrontierRaidFlowState::Idle;
	FString ClientRaidSessionId;
	FFrontierRaidResultSnapshot LastResult;
	FFrontierPlayerLevelSnapshot PreRaidLevel;
	FFrontierRaidSettlementPresentation PendingSettlementPresentation;
	FString PendingSettlementRaidSessionId;
	EFrontierRaidOutcome PendingSettlementOutcome = EFrontierRaidOutcome::Dead;
	int64 PendingSettlementExperience = 0;
	bool bHasPreRaidLevel = false;
	bool bAwaitingSettlementLevelRefresh = false;

	TMap<FString, FServerRaidContext> ServerContexts;
	TMap<TWeakObjectPtr<APlayerController>, FString> RaidIdByController;
	TMap<FString, TSharedPtr<FFrontierOnlineHttpClient>> ConflictConfirmationClients;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierInternalApiSubsystem> InternalApi;
};
