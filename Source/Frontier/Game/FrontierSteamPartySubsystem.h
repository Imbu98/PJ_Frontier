#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FrontierSteamPartySubsystem.generated.h"

class FFrontierSteamPartyCallbackBridge;
class FFrontierOnlineHttpClient;
struct FFrontierOnlinePartyResponse;
struct FFrontierOnlineLeavePartyResponse;
class UTexture2D;

USTRUCT(BlueprintType)
struct FFrontierSteamPartyMember
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Steam Party")
	FString SteamId;

	UPROPERTY(BlueprintReadOnly, Category="Steam Party")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Steam Party")
	TObjectPtr<UTexture2D> AvatarTexture = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Steam Party")
	bool bIsLocalPlayer = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FFrontierSteamPartyMembersChangedSignature,
	const TArray<FFrontierSteamPartyMember>&,
	Members);

DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FFrontierSteamMatchmakingContextChangedNative,
	const FString& /* TicketId */,
	const FString& /* MapId */,
	const FString& /* PartyId */);

/**
 * Owns the client-side Steam Lobby used to assemble a party before backend
 * matchmaking. Backend party synchronization will consume GetSteamLobbyId().
 */
UCLASS()
class FRONTIER_API UFrontierSteamPartySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Pre-creates only the Steam Lobby. Backend Party creation remains lazy. */
	UFUNCTION(BlueprintCallable, Category="Frontier|Steam Party")
	bool PrepareSteamPartyLobby();

	/** Creates the private Steam Lobby when needed, then opens Steam's invite overlay. */
	UFUNCTION(BlueprintCallable, Category="Frontier|Steam Party")
	bool OpenSteamInviteOverlay();

	UFUNCTION(BlueprintCallable, Category="Frontier|Steam Party")
	void RefreshPartyMembers();

	UFUNCTION(BlueprintPure, Category="Frontier|Steam Party")
	const TArray<FFrontierSteamPartyMember>& GetPartyMembers() const { return PartyMembers; }

	/** Empty until a Steam Lobby has been created or joined. */
	UFUNCTION(BlueprintPure, Category="Frontier|Steam Party")
	FString GetSteamLobbyId() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Steam Party")
	bool HasSteamPartyLobby() const;

	/** Returns true only when another Steam player is currently in the Lobby. */
	UFUNCTION(BlueprintPure, Category="Frontier|Steam Party")
	bool HasOtherPartyMembers() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Steam Party")
	bool IsLocalPartyLeader() const { return IsLocalSteamLobbyOwner(); }

	/** Leaves as a member, or disbands the backend party when called by its leader. */
	UFUNCTION(BlueprintCallable, Category="Frontier|Steam Party")
	bool LeaveCurrentParty();

	/** Backend party identity corresponding to the current Steam Lobby. */
	UFUNCTION(BlueprintPure, Category="Frontier|Steam Party")
	const FString& GetBackendPartyId() const { return BackendPartyId; }

	/** Publishes the leader-created ticket so every Steam Lobby member can subscribe with its own JWT. */
	bool PublishMatchmakingContext(
		const FString& TicketId,
		const FString& MapId,
		const FString& PartyId);

	FFrontierSteamMatchmakingContextChangedNative OnMatchmakingContextChanged;

	UPROPERTY(BlueprintAssignable, Category="Frontier|Steam Party")
	FFrontierSteamPartyMembersChangedSignature OnPartyMembersChanged;

private:
	friend class FFrontierSteamPartyCallbackBridge;

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleSessionUserInviteAccepted(
		bool bWasSuccessful,
		int32 ControllerId,
		FUniqueNetIdPtr UserId,
		const FOnlineSessionSearchResult& InviteResult);
	void HandleExistingSessionDestroyedForInvite(FName SessionName, bool bWasSuccessful);
	void JoinPendingInvite();
	bool CreateSteamPartyLobby(bool bOpenInviteOverlayWhenReady);
	bool ShowInviteOverlay() const;
	void RequestCreateBackendParty(bool bForceTokenRefresh = false);
	void StartCreateBackendPartyRequest(const FString& AccessToken);
	void HandleCreateBackendPartyResponse(const FFrontierOnlinePartyResponse& Response);
	void TryRequestJoinBackendParty();
	void RequestJoinBackendParty(bool bForceTokenRefresh = false);
	void StartJoinBackendPartyRequest(const FString& AccessToken);
	void HandleJoinBackendPartyResponse(const FFrontierOnlinePartyResponse& Response);
	void RequestExitBackendParty(bool bForceTokenRefresh = false);
	void StartExitBackendPartyRequest(const FString& AccessToken);
	void HandleLeaveBackendPartyResponse(const FFrontierOnlineLeavePartyResponse& Response);
	void HandleDisbandBackendPartyResponse(const FFrontierOnlinePartyResponse& Response);
	void CompleteBackendPartyExit();
	void FailBackendPartyExit(const FString& Error);
	bool DestroySteamLobbyAfterBackendExit();
	bool IsLocalSteamLobbyOwner() const;
	FString ReadBackendPartyIdFromSteamLobby() const;
	bool WriteBackendPartyIdToSteamLobby(const FString& PartyId) const;
	FString ReadSteamLobbyData(const char* Key) const;
	void RefreshMatchmakingContextFromSteamLobby();
	void ResetBackendPartyState();
	UTexture2D* ResolveAvatarTexture(uint64 SteamId);
	void HandleSteamLobbyChanged(uint64 SteamLobbyId);
	void HandleSteamAvatarLoaded(uint64 SteamId);

	static const FName PartySessionName;
	static constexpr int32 MaximumPartyMembers = 3;

	IOnlineSessionPtr SessionInterface;
	FDelegateHandle CreateSessionCompleteHandle;
	FDelegateHandle JoinSessionCompleteHandle;
	FDelegateHandle SessionUserInviteAcceptedHandle;

	FOnlineSessionSearchResult PendingInviteResult;
	bool bHasPendingInvite = false;
	bool bSteamLobbyCreationInProgress = false;
	bool bOpenInviteOverlayAfterCreate = false;
	bool bBackendPartyRequestInProgress = false;
	bool bBackendPartyJoined = false;
	bool bBackendAuthRetryAttempted = false;
	bool bAwaitingBackendPartyJoinMetadata = false;
	bool bBackendPartyExitInProgress = false;
	bool bExitRequestWasDisband = false;
	int32 PendingInviteControllerId = 0;
	FString BackendPartyId;
	FString LastMatchmakingTicketId;
	FString LastMatchmakingMapId;
	FString LastMatchmakingPartyId;
	FString BackendMutationIdempotencyKey;
	TSharedPtr<FFrontierOnlineHttpClient> PartyHttpClient;

	UPROPERTY(Transient)
	TArray<FFrontierSteamPartyMember> PartyMembers;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTexture2D>> AvatarTextureCache;

	FFrontierSteamPartyCallbackBridge* SteamCallbackBridge = nullptr;
};
