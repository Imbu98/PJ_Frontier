#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FrontierOnlineHttpClient.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Progression/FrontierLevelProgressionTypes.h"
#include "FrontierBackendProtocolComponent.generated.h"

USTRUCT(BlueprintType)
struct FFrontierBackendSessionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FString SessionId;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FString PlayerIdString;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	int64 PlayerId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FString Platform;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FString ExpiresAt;
};

USTRUCT(BlueprintType)
struct FFrontierBackendTokenInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FString TokenType;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FString AccessToken;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FString AccessTokenExpiresAt;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FString RefreshToken;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FString RefreshTokenExpiresAt;
};

USTRUCT(BlueprintType)
struct FFrontierBackendPlayerInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FString PlayerIdString;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	int64 PlayerId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FString SteamId;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FString Nickname;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FString Locale;
};

USTRUCT(BlueprintType)
struct FFrontierBackendSteamLoginResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FFrontierBackendSessionInfo Session;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FFrontierBackendTokenInfo Tokens;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FFrontierBackendPlayerInfo Player;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	bool bIsNewPlayer = false;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FString RequestId;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FString ServerTime;

	UPROPERTY(BlueprintReadOnly, Category="Backend")
	FString Message;
};

USTRUCT(BlueprintType)
struct FFrontierBackendCurrencyInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Backend|Currency")
	FString CurrencyCode;

	UPROPERTY(BlueprintReadOnly, Category="Backend|Currency")
	int64 Balance = 0;

};

USTRUCT(BlueprintType)
struct FFrontierItemUpgradeResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Backend|Upgrade")
	bool bRequestSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category="Backend|Upgrade")
	bool bUpgradeSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category="Backend|Upgrade")
	bool bRetryable = false;

	UPROPERTY(BlueprintReadOnly, Category="Backend|Upgrade")
	int32 PreviousEnhancementLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category="Backend|Upgrade")
	int32 CurrentEnhancementLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category="Backend|Upgrade")
	FString FailureReason;

	UPROPERTY(BlueprintReadOnly, Category="Backend|Upgrade")
	FString ErrorCode;

	UPROPERTY(BlueprintReadOnly, Category="Backend|Upgrade")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category="Backend|Upgrade")
	FFrontierItemInstance Item;

	UPROPERTY(BlueprintReadOnly, Category="Backend|Upgrade")
	TArray<FFrontierBackendCurrencyInfo> Currencies;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFrontierBackendSteamLoginSucceeded, const FFrontierBackendSteamLoginResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFrontierLobbyBackendDataReady, const FFrontierBackendSteamLoginResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFrontierBackendRequestFailed, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFrontierNicknameUpdateSucceeded, const FString&, Nickname);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFrontierNicknameUpdateFailed, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFrontierItemUpgradeCompleted, const FFrontierItemUpgradeResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFrontierBackendCurrenciesChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFrontierMatchmakingStatusChanged, const FString&, Status);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FFrontierLobbyLevelReady,
	const FFrontierPlayerLevelSnapshot&,
	Level,
	const FFrontierTemporarySkillPointResult&,
	SkillPointResult);

UCLASS(ClassGroup=(Frontier), meta=(BlueprintSpawnableComponent))
class FRONTIER_API UFrontierBackendProtocolComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFrontierBackendProtocolComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="Backend|Auth")
	void LoginWithSteamAuthTicket(const FString& AuthTicket);

	UFUNCTION(BlueprintCallable, Category="Backend|Auth")
	void ResumeAuthenticatedLobbySession();

	UFUNCTION(BlueprintPure, Category="Backend")
	bool IsRequestInProgress() const { return bRequestInProgress; }

	UFUNCTION(BlueprintPure, Category="Backend")
	const FFrontierBackendSteamLoginResult& GetLastSteamLoginResult() const { return LastSteamLoginResult; }

	const TArray<FFrontierBackendCurrencyInfo>& GetLastCurrencies() const { return LastCurrencies; }

	UFUNCTION(BlueprintPure, Category="Backend|Progression")
	const FFrontierPlayerLevelSnapshot& GetLastPlayerLevel() const { return LastPlayerLevel; }

	UFUNCTION(BlueprintPure, Category="Backend|Progression")
	const FFrontierTemporarySkillPointResult& GetLastTemporarySkillPointResult() const { return LastTemporarySkillPointResult; }

	UFUNCTION(BlueprintCallable, Category="Backend|Profile")
	void RequestUpdateNickname(const FString& Nickname);

	bool RequestUpgradeItem(
		const FGuid& ItemInstanceId,
		int32 ExpectedEnhancementLevel,
		const FString& IdempotencyKey);

	bool RequestMoveInventoryItem(const FGuid& ItemInstanceId, int32 TargetSlotIndex, TOptional<int32> Quantity = TOptional<int32>());
	bool RequestSwapInventoryItems(int32 SourceSlotIndex, int32 TargetSlotIndex, const FGuid& SourceItemInstanceId, const FGuid& TargetItemInstanceId);
	bool RequestDepositStorageItem(const FGuid& ItemInstanceId, TOptional<int32> TargetSlotIndex = TOptional<int32>(), TOptional<int32> Quantity = TOptional<int32>());
	bool RequestWithdrawStorageItem(const FGuid& ItemInstanceId, TOptional<int32> TargetSlotIndex = TOptional<int32>(), TOptional<int32> Quantity = TOptional<int32>());
	bool RequestMoveStorageItem(const FGuid& ItemInstanceId, int32 TargetSlotIndex, TOptional<int32> Quantity = TOptional<int32>());
	bool RequestSwapStorageItems(int32 SourceSlotIndex, int32 TargetSlotIndex, const FGuid& SourceItemInstanceId, const FGuid& TargetItemInstanceId);
	bool RequestSwapRaidInventoryWithStorage(int32 SourceRaidSlotIndex, int32 TargetStorageSlotIndex, const FGuid& SourceItemInstanceId, const FGuid& TargetItemInstanceId);
	bool RequestSwapStorageWithRaidInventory(int32 SourceStorageSlotIndex, int32 TargetRaidSlotIndex, const FGuid& SourceItemInstanceId, const FGuid& TargetItemInstanceId);
	bool RequestEquipInventoryItem(const FGuid& ItemInstanceId, EFrontierEquipmentSlot SlotType, bool bReplaceExisting, TOptional<int32> ReplacementTargetInventorySlot = TOptional<int32>());
	bool RequestUnequipItem(EFrontierEquipmentSlot SlotType, const FGuid& ItemInstanceId, TOptional<int32> TargetInventorySlotIndex = TOptional<int32>());
	void RequestCreateRaidEntry(const FString& MapId, const FString& PartyId = FString());
	void RequestStartMatchmaking(const FString& MapId, const FString& PartyId = FString());

	UFUNCTION(BlueprintCallable, Category="Backend|Matchmaking")
	void RequestStartMatchmakingWithMode(
		const FString& MapId,
		const FString& MatchMode,
		const FString& PartyId);

	UFUNCTION(BlueprintCallable, Category="Backend|Matchmaking")
	void CancelMatchmaking();
	/** Shared authority gate for HTTP recovery and RAID_SERVER_READY WebSocket events. */
	void HandleRaidServerReadyNotification(const FString& TicketId, const FString& MatchId);
	void RetryRaidServerReadyNotification();
	void RefreshLobbyAfterRaid(const FString& RaidSessionId);
	static bool AreRaidRefreshInputsReady(
		bool bLevelPending,
		bool bResultPending,
		bool bLevelSucceeded,
		bool bResultSucceeded);

	UPROPERTY(BlueprintAssignable, Category="Backend|Auth")
	FFrontierBackendSteamLoginSucceeded OnSteamLoginSucceeded;

	UPROPERTY(BlueprintAssignable, Category="Backend|Inventory")
	FFrontierLobbyBackendDataReady OnLobbyBackendDataReady;

	UPROPERTY(BlueprintAssignable, Category="Backend|Auth")
	FFrontierBackendRequestFailed OnBackendRequestFailed;

	UPROPERTY(BlueprintAssignable, Category="Backend|Profile")
	FFrontierNicknameUpdateSucceeded OnNicknameUpdateSucceeded;

	UPROPERTY(BlueprintAssignable, Category="Backend|Profile")
	FFrontierNicknameUpdateFailed OnNicknameUpdateFailed;

	UPROPERTY(BlueprintAssignable, Category="Backend|Upgrade")
	FFrontierItemUpgradeCompleted OnItemUpgradeCompleted;

	UPROPERTY(BlueprintAssignable, Category="Backend|Currency")
	FFrontierBackendCurrenciesChanged OnCurrenciesChanged;

	UPROPERTY(BlueprintAssignable, Category="Backend|Matchmaking")
	FFrontierMatchmakingStatusChanged OnMatchmakingStatusChanged;

	UPROPERTY(BlueprintAssignable, Category="Backend|Progression")
	FFrontierLobbyLevelReady OnLobbyLevelReady;

private:
	UFUNCTION(Server, Reliable)
	void ServerLoginWithSteamAuthTicket(const FString& AuthTicket);

	UFUNCTION(Server, Reliable)
	void ServerRequestUpdateNickname(const FString& Nickname);

	UFUNCTION(Server, Reliable)
	void ServerRequestCreateRaidEntry(const FString& MapId, const FString& PartyId);

	UFUNCTION(Server, Reliable)
	void ServerRequestStartMatchmaking(const FString& MapId, const FString& PartyId);

	UFUNCTION(Server, Reliable)
	void ServerRequestStartMatchmakingWithMode(
		const FString& MapId,
		const FString& MatchMode,
		const FString& PartyId);

	UFUNCTION(Server, Reliable)
	void ServerCancelMatchmaking();

	UFUNCTION(Server, Reliable)
	void ServerAdoptPartyMatchmakingContext(
		const FString& TicketId,
		const FString& MapId,
		const FString& PartyId);

	UFUNCTION(Server, Reliable)
	void ServerRelayRaidServerReady(const FString& TicketId, const FString& MatchId);

	UFUNCTION(Server, Reliable)
	void ServerRefreshLobbyAfterRaid(const FString& RaidSessionId);

	UFUNCTION(Server, Reliable)
	void ServerAcknowledgeRaidLobbyRefresh(const FString& RaidSessionId, bool bSucceeded);

	UFUNCTION(Client, Reliable)
	void ClientReceiveBackendLoginSucceeded(const FFrontierBackendSteamLoginResult& Result);

	UFUNCTION(Client, Reliable)
	void ClientReceiveBackendRequestFailed(const FString& ErrorMessage);

	UFUNCTION(Client, Reliable)
	void ClientReceiveNicknameUpdateSucceeded(const FString& Nickname);

	UFUNCTION(Client, Reliable)
	void ClientReceiveNicknameUpdateFailed(const FString& ErrorMessage);

	UFUNCTION(Client, Reliable)
	void ClientReceiveItemUpgradeResult(const FFrontierItemUpgradeResult& Result);

	UFUNCTION(Client, Reliable)
	void ClientSetMatchmakingRequestInProgress(bool bInProgress);

	UFUNCTION(Client, Reliable)
	void ClientSetMatchmakingOperationInProgress(bool bInProgress);

	UFUNCTION(Client, Reliable)
	void ClientReceiveMatchmakingCancelled(const FString& TicketId);

	UFUNCTION(Client, Reliable)
	void ClientReceiveMatchmakingStatus(const FString& Status);

	UFUNCTION(Client, Reliable)
	void ClientReceiveMatchmakingContext(
		const FString& TicketId,
		const FString& MapId,
		const FString& PartyId);

	UFUNCTION(Client, Reliable)
	void ClientReceiveLobbyProgressionReady(
		const FFrontierBackendSteamLoginResult& LoginResult,
		const FFrontierPlayerLevelSnapshot& Level);

	UFUNCTION(Client, Reliable)
	void ClientReceiveRaidRefreshReady(
		const FFrontierPlayerLevelSnapshot& Level,
		const FString& RaidSessionId,
		const FString& Outcome,
		bool bHasReasonCode,
		const FString& ReasonCode,
		bool bHasCommittedAt,
		const FString& CommittedAt);

	void HandleSteamAuthResponse(const FFrontierOnlineSteamLoginResponse& Response);
	void StartNicknameUpdateRequest(const FString& Nickname);
	void HandleNicknameUpdateResponse(const FFrontierOnlineProfileResponse& Response, const FString& RequestedNickname);
	void BroadcastNicknameUpdateFailed(const FString& ErrorMessage);
	void RequestProfileAfterLogin(
		FFrontierBackendSteamLoginResult LoginResult,
		bool bAfterAccessTokenRefresh = false);
	void HandleProfileAfterLogin(
		const FFrontierOnlineProfileResponse& Response,
		FFrontierBackendSteamLoginResult LoginResult,
		bool bAfterAccessTokenRefresh);
	void RequestLobbyBootstrapAfterLogin(
		FFrontierBackendSteamLoginResult LoginResult,
		bool bAfterAccessTokenRefresh = false);
	void HandleLobbyBootstrapResponse(
		const FFrontierOnlineLobbyBootstrapResponse& Response,
		FFrontierBackendSteamLoginResult LoginResult,
		bool bAfterAccessTokenRefresh);
	void RequestPlayerLevelAfterBootstrap(
		FFrontierBackendSteamLoginResult LoginResult,
		bool bAfterAccessTokenRefresh = false);
	void HandlePlayerLevelResponse(
		const FFrontierOnlinePlayerLevelResponse& Response,
		FFrontierBackendSteamLoginResult LoginResult,
		bool bAfterAccessTokenRefresh);
	void CompleteLocalLobbyProgression(
		const FFrontierBackendSteamLoginResult& LoginResult,
		const FFrontierPlayerLevelSnapshot& Level);
	void StartMatchmakingRequest(const FString& MapId, const FString& MatchMode, const FString& PartyId);
	void SendCreateMatchmakingTicketAttempt(bool bAfterAccessTokenRefresh);
	void HandleCreateMatchmakingTicketResponse(
		bool bAfterAccessTokenRefresh,
		const FFrontierOnlineMatchmakingTicketResponse& Response);
	void ScheduleMatchmakingTicketPoll();
	void PollMatchmakingTicket(bool bAfterAccessTokenRefresh = false);
	void HandleMatchmakingTicketPollResponse(
		bool bAfterAccessTokenRefresh,
		const FFrontierOnlineMatchmakingTicketResponse& Response);
	void SendCancelMatchmakingTicketAttempt(bool bAfterAccessTokenRefresh);
	void HandleCancelMatchmakingTicketResponse(
		bool bAfterAccessTokenRefresh,
		const FFrontierOnlineMatchmakingTicketResponse& Response);
	void FailMatchmaking(const FString& Error, bool bRetryable);
	void HandleBackendWebSocketConnected();
	void HandleBackendWebSocketDisconnected();
	void HandleBackendMatchmakingStatus(
		const FFrontierOnlineMatchmakingStatusEvent& Event);
	void HandleBackendMatchmakingFailed(
		const FFrontierOnlineMatchmakingFailedEvent& Event);
	void HandleBackendRaidServerReady(
		const FFrontierOnlineRaidServerReadyEvent& Event);
	void StartRaidEntryAfterServerReady();
	void HandleSteamMatchmakingContextChanged(
		const FString& TicketId,
		const FString& MapId,
		const FString& PartyId);
	void StartRaidEntryRequest(const FString& MapId, const FString& PartyId);
	void SendRaidEntryAttempt(FFrontierOnlineCreateRaidEntryRequest Request, bool bAfterAccessTokenRefresh);
	void HandleRaidEntryResponse(
		const FFrontierOnlineCreateRaidEntryRequest& Request,
		bool bAfterAccessTokenRefresh,
		const FFrontierOnlineRaidEntryResponse& Response);
	void StartRaidLobbyRefresh(const FString& RaidSessionId);
	void RequestRaidRefreshLevel(bool bAfterAccessTokenRefresh);
	void RequestRaidRefreshResult(bool bAfterAccessTokenRefresh);
	void TryCompleteRaidLobbyRefresh();
	void CompleteLocalRaidRefresh(
		const FFrontierPlayerLevelSnapshot& Level,
		const FFrontierOnlineRaidResultDTO& Result);
	bool ApplyInventoryResponseToPlayerState(const FFrontierOnlineInventoryResponse& Response, FString& OutError);
	bool ApplyInventoryDataToPlayerState(const FFrontierOnlineInventoryData& InventoryData, FString& OutError);
	bool ApplyStorageResponseToPlayerState(const FFrontierOnlineStorageResponse& Response, FString& OutError);
	bool ApplyStorageDataToPlayerState(const FFrontierOnlineStorageData& StorageData, FString& OutError);
	bool ApplyEquipmentResponseToPlayerState(const FFrontierOnlineEquipmentResponse& Response, FString& OutError);
	bool ApplyEquipmentDataToPlayerState(const FFrontierOnlineEquipmentData& EquipmentData, FString& OutError);
	bool ApplyLobbyBootstrapToPlayerState(const FFrontierOnlineLobbyBootstrapResponse& Response, FString& OutError);
	void EnterActiveRaidSession(const FFrontierOnlineRaidSessionDTO& ActiveRaid);
	void HandleInventoryMutationResponse(const FFrontierOnlineInventoryResponse& Response);
	void HandleStorageTransferResponse(const FFrontierOnlineStorageTransferResponse& Response);
	void HandleStorageMutationResponse(const FFrontierOnlineStorageResponse& Response);
	bool ContinuePendingInventorySwap();
	void ClearPendingInventorySwap();
	bool RequestStorageTransfer(
		bool bDeposit,
		const FGuid& ItemInstanceId,
		TOptional<int32> TargetSlotIndex,
		TOptional<int32> Quantity);
	void HandleEquipmentMutationResponse(const FFrontierOnlineEquipmentChangeResponse& Response);
	void HandleItemUpgradeResponse(const FFrontierOnlineItemUpgradeResponse& Response);
	bool ApplyItemUpgradeResponseToPlayerState(
		const FFrontierOnlineItemUpgradeResponse& Response,
		FFrontierItemInstance& OutUpdatedItem,
		FString& OutError);
	void BroadcastItemUpgradeResult(const FFrontierItemUpgradeResult& Result);
	void BroadcastFailure(const FString& ErrorMessage);
	void BroadcastFailureLocal(const FString& ErrorMessage);
	void BroadcastMatchmakingStatus(const FString& Status);
	FFrontierBackendSteamLoginResult BuildSanitizedLoginResult(const FFrontierOnlineSteamLoginResponse& Response) const;

	UPROPERTY()
	FFrontierBackendSteamLoginResult LastSteamLoginResult;

	UPROPERTY()
	TArray<FFrontierBackendCurrencyInfo> LastCurrencies;

	UPROPERTY()
	FFrontierPlayerLevelSnapshot LastPlayerLevel;

	UPROPERTY()
	FFrontierTemporarySkillPointResult LastTemporarySkillPointResult;

	TSharedPtr<FFrontierOnlineHttpClient> OnlineHttpClient;
	TSharedPtr<FFrontierOnlineHttpClient> RaidEntryHttpClient;
	TSharedPtr<FFrontierOnlineHttpClient> RaidRefreshLevelHttpClient;
	TSharedPtr<FFrontierOnlineHttpClient> RaidRefreshResultHttpClient;
	TSharedPtr<FFrontierOnlineHttpClient> MatchmakingHttpClient;
	bool bRequestInProgress = false;

	bool bInventoryMutationInProgress = false;
	bool bEquipmentMutationInProgress = false;

	enum class EPendingInventorySwapKind : uint8
	{
		None,
		RaidInventory,
		Storage,
		RaidToStorage,
		StorageToRaid
	};

	struct FPendingInventorySwap
	{
		EPendingInventorySwapKind Kind = EPendingInventorySwapKind::None;
		int32 SourceSlotIndex = INDEX_NONE;
		int32 TargetSlotIndex = INDEX_NONE;
		int32 SourceTempSlotIndex = INDEX_NONE;
		int32 TargetTempSlotIndex = INDEX_NONE;
		FGuid SourceItemInstanceId;
		FGuid TargetItemInstanceId;
		int32 Step = 0;

		void Reset()
		{
			*this = FPendingInventorySwap();
		}
	};

	FPendingInventorySwap PendingInventorySwap;

	FString PendingRaidRefreshSessionId;
	bool bRaidRefreshLevelPending = false;
	bool bRaidRefreshResultPending = false;
	bool bRaidRefreshLevelSucceeded = false;
	bool bRaidRefreshResultSucceeded = false;
	FString RaidRefreshFailure;
	FFrontierOnlinePlayerLevelResponse PendingRaidRefreshLevel;
	FFrontierOnlineRaidResultResponse PendingRaidRefreshResult;

	FString PendingMatchmakingMapId;
	FString PendingMatchmakingMode;
	FString PendingMatchmakingPartyId;
	FString PendingMatchmakingTicketId;
	FString PendingMatchmakingMatchId;
	bool bMatchmakingOperationInProgress = false;
	bool bRaidServerReadyReceived = false;
	bool bRaidEntryStartedForTicket = false;
	FTimerHandle MatchmakingPollTimerHandle;
	FTimerHandle RaidEntryAfterServerReadyTimerHandle;
	float MatchmakingPollIntervalSeconds = 2.0f;
};
