#include "Game/FrontierSteamPartySubsystem.h"

#include "Async/Async.h"
#include "Engine/Texture2D.h"
#include "Frontier.h"
#include "FrontierOnlineHttpClient.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Online/FrontierPlayerSessionSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "steam/steam_api.h"

const FName UFrontierSteamPartySubsystem::PartySessionName(TEXT("FrontierPartySession"));

namespace
{
constexpr char BackendPartyIdLobbyDataKey[] = "FRONTIER_BACKEND_PARTY_ID_s";
constexpr char MatchmakingMapIdLobbyDataKey[] = "FRONTIER_MATCH_MAP_ID_s";
constexpr char MatchmakingPartyIdLobbyDataKey[] = "FRONTIER_MATCH_PARTY_ID_s";
// Written last and treated as the commit marker for the other matchmaking fields.
constexpr char MatchmakingTicketIdLobbyDataKey[] = "FRONTIER_MATCH_TICKET_ID_s";
}

class FFrontierSteamPartyCallbackBridge
{
public:
	explicit FFrontierSteamPartyCallbackBridge(UFrontierSteamPartySubsystem* InOwner)
		: Owner(InOwner)
	{
		LobbyChatUpdateCallback.Register(this, &FFrontierSteamPartyCallbackBridge::OnLobbyChatUpdate);
		LobbyDataUpdateCallback.Register(this, &FFrontierSteamPartyCallbackBridge::OnLobbyDataUpdate);
		AvatarImageLoadedCallback.Register(this, &FFrontierSteamPartyCallbackBridge::OnAvatarImageLoaded);
	}

	~FFrontierSteamPartyCallbackBridge()
	{
		LobbyChatUpdateCallback.Unregister();
		LobbyDataUpdateCallback.Unregister();
		AvatarImageLoadedCallback.Unregister();
	}

private:
	void OnLobbyChatUpdate(LobbyChatUpdate_t* Callback)
	{
		if (!Callback)
		{
			return;
		}

		const TWeakObjectPtr<UFrontierSteamPartySubsystem> WeakOwner = Owner;
		const uint64 LobbyId = Callback->m_ulSteamIDLobby;
		AsyncTask(ENamedThreads::GameThread, [WeakOwner, LobbyId]()
		{
			if (UFrontierSteamPartySubsystem* StrongOwner = WeakOwner.Get())
			{
				StrongOwner->HandleSteamLobbyChanged(LobbyId);
			}
		});
	}

	void OnLobbyDataUpdate(LobbyDataUpdate_t* Callback)
	{
		if (!Callback || !Callback->m_bSuccess)
		{
			return;
		}

		const TWeakObjectPtr<UFrontierSteamPartySubsystem> WeakOwner = Owner;
		const uint64 LobbyId = Callback->m_ulSteamIDLobby;
		AsyncTask(ENamedThreads::GameThread, [WeakOwner, LobbyId]()
		{
			if (UFrontierSteamPartySubsystem* StrongOwner = WeakOwner.Get())
			{
				StrongOwner->HandleSteamLobbyChanged(LobbyId);
			}
		});
	}

	void OnAvatarImageLoaded(AvatarImageLoaded_t* Callback)
	{
		if (!Callback)
		{
			return;
		}

		const TWeakObjectPtr<UFrontierSteamPartySubsystem> WeakOwner = Owner;
		const uint64 SteamId = Callback->m_steamID.ConvertToUint64();
		AsyncTask(ENamedThreads::GameThread, [WeakOwner, SteamId]()
		{
			if (UFrontierSteamPartySubsystem* StrongOwner = WeakOwner.Get())
			{
				StrongOwner->HandleSteamAvatarLoaded(SteamId);
			}
		});
	}

	TWeakObjectPtr<UFrontierSteamPartySubsystem> Owner;
	CCallbackManual<FFrontierSteamPartyCallbackBridge, LobbyChatUpdate_t> LobbyChatUpdateCallback;
	CCallbackManual<FFrontierSteamPartyCallbackBridge, LobbyDataUpdate_t> LobbyDataUpdateCallback;
	CCallbackManual<FFrontierSteamPartyCallbackBridge, AvatarImageLoaded_t> AvatarImageLoadedCallback;
};

void UFrontierSteamPartySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PartyHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());

	IOnlineSubsystem* SteamSubsystem = IOnlineSubsystem::Get(TEXT("STEAM"));
	if (!SteamSubsystem)
	{
		FRONTIER_LOG(Warning, TEXT("Steam party initialization skipped because Steam OSS is unavailable."));
		return;
	}

	SessionInterface = SteamSubsystem->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		FRONTIER_LOG(Warning, TEXT("Steam party initialization skipped because the session interface is unavailable."));
		return;
	}

	CreateSessionCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(
			this,
			&ThisClass::HandleCreateSessionComplete));
	JoinSessionCompleteHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(
			this,
			&ThisClass::HandleJoinSessionComplete));
	SessionUserInviteAcceptedHandle = SessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(
		FOnSessionUserInviteAcceptedDelegate::CreateUObject(
			this,
			&ThisClass::HandleSessionUserInviteAccepted));

	if (SteamUser() && SteamFriends() && SteamUtils() && SteamMatchmaking())
	{
		SteamCallbackBridge = new FFrontierSteamPartyCallbackBridge(this);
	}

	RefreshPartyMembers();
}

void UFrontierSteamPartySubsystem::Deinitialize()
{
	delete SteamCallbackBridge;
	SteamCallbackBridge = nullptr;

	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		SessionInterface->ClearOnSessionUserInviteAcceptedDelegate_Handle(SessionUserInviteAcceptedHandle);
	}

	SessionInterface.Reset();
	PartyHttpClient.Reset();
	PartyMembers.Reset();
	AvatarTextureCache.Reset();
	OnMatchmakingContextChanged.Clear();
	bHasPendingInvite = false;
	bSteamLobbyCreationInProgress = false;
	bOpenInviteOverlayAfterCreate = false;
	ResetBackendPartyState();

	Super::Deinitialize();
}

bool UFrontierSteamPartySubsystem::PrepareSteamPartyLobby()
{
	if (!SessionInterface.IsValid())
	{
		FRONTIER_LOG(Warning, TEXT("Steam party Lobby could not be prepared because the session interface is unavailable."));
		return false;
	}
	if (bSteamLobbyCreationInProgress)
	{
		return true;
	}
	if (SessionInterface->GetNamedSession(PartySessionName))
	{
		return true;
	}
	return CreateSteamPartyLobby(false);
}

bool UFrontierSteamPartySubsystem::OpenSteamInviteOverlay()
{
	if (!SessionInterface.IsValid())
	{
		FRONTIER_LOG(Warning, TEXT("Steam invite UI could not open because the session interface is unavailable."));
		return false;
	}
	if (bSteamLobbyCreationInProgress)
	{
		bOpenInviteOverlayAfterCreate = true;
		return true;
	}

	if (SessionInterface->GetNamedSession(PartySessionName))
	{
		if (bSteamLobbyCreationInProgress || GetSteamLobbyId().IsEmpty())
		{
			bOpenInviteOverlayAfterCreate = true;
			return true;
		}

		const FString LobbyBackendPartyId = ReadBackendPartyIdFromSteamLobby();
		if (!LobbyBackendPartyId.IsEmpty())
		{
			BackendPartyId = LobbyBackendPartyId;
		}
		if (!BackendPartyId.IsEmpty())
		{
			if (!IsLocalSteamLobbyOwner() && !bBackendPartyJoined)
			{
				TryRequestJoinBackendParty();
				FRONTIER_LOG(Warning, TEXT("Steam invite UI is waiting for backend party join completion."));
				return false;
			}
			if (IsLocalSteamLobbyOwner() && LobbyBackendPartyId.IsEmpty()
				&& !WriteBackendPartyIdToSteamLobby(BackendPartyId))
			{
				FRONTIER_LOG(Warning, TEXT("Steam invite UI could not open because backend party metadata was not published."));
				return false;
			}
			return ShowInviteOverlay();
		}
		if (!IsLocalSteamLobbyOwner())
		{
			FRONTIER_LOG(Warning, TEXT("Steam invite UI is waiting for backend party metadata from the lobby owner."));
			return false;
		}

		bOpenInviteOverlayAfterCreate = true;
		RequestCreateBackendParty();
		return true;
	}

	return CreateSteamPartyLobby(true);
}

bool UFrontierSteamPartySubsystem::CreateSteamPartyLobby(const bool bOpenInviteOverlayWhenReady)
{
	IOnlineSubsystem* SteamSubsystem = IOnlineSubsystem::Get(TEXT("STEAM"));
	const IOnlineIdentityPtr Identity = SteamSubsystem ? SteamSubsystem->GetIdentityInterface() : nullptr;
	if (!Identity.IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn)
	{
		FRONTIER_LOG(Warning, TEXT("Steam invite UI requires a logged-in Steam user."));
		return false;
	}

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = MaximumPartyMembers;
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = false;
	Settings.bAllowJoinInProgress = true;
	Settings.bIsLANMatch = false;
	Settings.bIsDedicated = false;
	Settings.bUsesStats = false;
	Settings.bAllowInvites = true;
	Settings.bUsesPresence = true;
	Settings.bAllowJoinViaPresence = false;
	Settings.bAllowJoinViaPresenceFriendsOnly = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bUseLobbiesVoiceChatIfAvailable = false;
	Settings.Set(
		FName(TEXT("FRONTIER_PARTY")),
		true,
		EOnlineDataAdvertisementType::ViaOnlineService);

	ResetBackendPartyState();
	bSteamLobbyCreationInProgress = true;
	bOpenInviteOverlayAfterCreate = bOpenInviteOverlayWhenReady;
	if (!SessionInterface->CreateSession(0, PartySessionName, Settings))
	{
		bSteamLobbyCreationInProgress = false;
		bOpenInviteOverlayAfterCreate = false;
		FRONTIER_LOG(Warning, TEXT("Steam party lobby creation could not be started."));
		return false;
	}

	return true;
}

void UFrontierSteamPartySubsystem::RefreshPartyMembers()
{
	TArray<FFrontierSteamPartyMember> NewMembers;
	if (!SteamUser() || !SteamFriends())
	{
		PartyMembers = MoveTemp(NewMembers);
		OnPartyMembersChanged.Broadcast(PartyMembers);
		return;
	}

	const CSteamID LocalSteamId = SteamUser()->GetSteamID();
	TArray<CSteamID> OrderedSteamIds;
	if (LocalSteamId.IsValid())
	{
		OrderedSteamIds.Add(LocalSteamId);
	}

	const FString LobbyIdString = GetSteamLobbyId();
	const uint64 LobbyIdValue = LobbyIdString.IsEmpty()
		? 0
		: FCString::Strtoui64(*LobbyIdString, nullptr, 10);
	if (LobbyIdValue != 0 && SteamMatchmaking())
	{
		const CSteamID LobbyId(LobbyIdValue);
		const int32 LobbyMemberCount = SteamMatchmaking()->GetNumLobbyMembers(LobbyId);
		for (int32 MemberIndex = 0;
			MemberIndex < LobbyMemberCount && OrderedSteamIds.Num() < MaximumPartyMembers;
			++MemberIndex)
		{
			const CSteamID MemberSteamId = SteamMatchmaking()->GetLobbyMemberByIndex(LobbyId, MemberIndex);
			if (MemberSteamId.IsValid() && MemberSteamId != LocalSteamId)
			{
				OrderedSteamIds.AddUnique(MemberSteamId);
			}
		}
	}

	NewMembers.Reserve(OrderedSteamIds.Num());
	for (const CSteamID MemberSteamId : OrderedSteamIds)
	{
		FFrontierSteamPartyMember& Member = NewMembers.AddDefaulted_GetRef();
		Member.SteamId = LexToString(MemberSteamId.ConvertToUint64());
		Member.bIsLocalPlayer = MemberSteamId == LocalSteamId;
		const char* PersonaName = SteamFriends()->GetFriendPersonaName(MemberSteamId);
		Member.DisplayName = PersonaName ? UTF8_TO_TCHAR(PersonaName) : FString();
		Member.AvatarTexture = ResolveAvatarTexture(MemberSteamId.ConvertToUint64());
	}

	PartyMembers = MoveTemp(NewMembers);
	OnPartyMembersChanged.Broadcast(PartyMembers);
}

FString UFrontierSteamPartySubsystem::GetSteamLobbyId() const
{
	if (!SessionInterface.IsValid())
	{
		return FString();
	}

	const FNamedOnlineSession* PartySession = SessionInterface->GetNamedSession(PartySessionName);
	return PartySession ? PartySession->GetSessionIdStr() : FString();
}

bool UFrontierSteamPartySubsystem::HasSteamPartyLobby() const
{
	return !GetSteamLobbyId().IsEmpty();
}

bool UFrontierSteamPartySubsystem::HasOtherPartyMembers() const
{
	return PartyMembers.ContainsByPredicate(
		[](const FFrontierSteamPartyMember& Member)
		{
			return !Member.bIsLocalPlayer;
		});
}

bool UFrontierSteamPartySubsystem::LeaveCurrentParty()
{
	if (bBackendPartyRequestInProgress || bBackendPartyExitInProgress)
	{
		return false;
	}
	bHasPendingInvite = false;
	if (bBackendPartyJoined && !BackendPartyId.IsEmpty())
	{
		RequestExitBackendParty();
		return true;
	}
	return DestroySteamLobbyAfterBackendExit();
}

bool UFrontierSteamPartySubsystem::PublishMatchmakingContext(
	const FString& TicketId,
	const FString& MapId,
	const FString& PartyId)
{
	FString NormalizedTicketId = TicketId;
	NormalizedTicketId.TrimStartAndEndInline();
	FString NormalizedMapId = MapId;
	NormalizedMapId.TrimStartAndEndInline();
	FString NormalizedPartyId = PartyId;
	NormalizedPartyId.TrimStartAndEndInline();
	if (NormalizedTicketId.IsEmpty() || NormalizedMapId.IsEmpty()
		|| NormalizedPartyId.IsEmpty() || NormalizedPartyId != BackendPartyId
		|| !SteamMatchmaking() || !IsLocalSteamLobbyOwner())
	{
		return false;
	}

	const uint64 LobbyIdValue = FCString::Strtoui64(*GetSteamLobbyId(), nullptr, 10);
	if (LobbyIdValue == 0)
	{
		return false;
	}
	const CSteamID LobbyId(LobbyIdValue);
	const bool bMapWritten = SteamMatchmaking()->SetLobbyData(
		LobbyId,
		MatchmakingMapIdLobbyDataKey,
		TCHAR_TO_UTF8(*NormalizedMapId));
	const bool bPartyWritten = SteamMatchmaking()->SetLobbyData(
		LobbyId,
		MatchmakingPartyIdLobbyDataKey,
		TCHAR_TO_UTF8(*NormalizedPartyId));
	const bool bTicketWritten = bMapWritten && bPartyWritten
		&& SteamMatchmaking()->SetLobbyData(
			LobbyId,
			MatchmakingTicketIdLobbyDataKey,
			TCHAR_TO_UTF8(*NormalizedTicketId));
	if (!bTicketWritten)
	{
		FRONTIER_LOG(Warning, TEXT("Matchmaking context could not be published to the Steam Lobby."));
		return false;
	}

	LastMatchmakingTicketId = NormalizedTicketId;
	LastMatchmakingMapId = NormalizedMapId;
	LastMatchmakingPartyId = NormalizedPartyId;
	FRONTIER_LOG(
		Log,
		TEXT("[Matchmaking] Published party matchmaking context to Steam Lobby. LobbyId=%s TicketId=%s MapId=%s PartyId=%s"),
		*GetSteamLobbyId(),
		*LastMatchmakingTicketId,
		*LastMatchmakingMapId,
		*LastMatchmakingPartyId);
	OnMatchmakingContextChanged.Broadcast(
		LastMatchmakingTicketId,
		LastMatchmakingMapId,
		LastMatchmakingPartyId);
	return true;
}

void UFrontierSteamPartySubsystem::HandleCreateSessionComplete(
	const FName SessionName,
	const bool bWasSuccessful)
{
	if (SessionName != PartySessionName)
	{
		return;
	}
	bSteamLobbyCreationInProgress = false;

	if (bWasSuccessful)
	{
		FRONTIER_LOG(Log, TEXT("Steam party lobby creation completed. LobbyId=%s"), *GetSteamLobbyId());
		if (bOpenInviteOverlayAfterCreate)
		{
			RequestCreateBackendParty();
		}
	}
	else
	{
		bOpenInviteOverlayAfterCreate = false;
		FRONTIER_LOG(Warning, TEXT("Steam party lobby creation failed."));
	}

	RefreshPartyMembers();
}

void UFrontierSteamPartySubsystem::HandleJoinSessionComplete(
	const FName SessionName,
	const EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionName != PartySessionName)
	{
		return;
	}

	const bool bJoined = Result == EOnJoinSessionCompleteResult::Success
		|| Result == EOnJoinSessionCompleteResult::AlreadyInSession;
	if (bJoined)
	{
		FRONTIER_LOG(
			Log,
			TEXT("Steam party lobby join completed. Result=%s LobbyId=%s"),
			LexToString(Result),
			*GetSteamLobbyId());
		bAwaitingBackendPartyJoinMetadata = true;
		TryRequestJoinBackendParty();
	}
	else
	{
		ResetBackendPartyState();
		FRONTIER_LOG(Warning, TEXT("Steam party lobby join failed. Result=%s"), LexToString(Result));
	}
	RefreshPartyMembers();
}

void UFrontierSteamPartySubsystem::HandleSessionUserInviteAccepted(
	const bool bWasSuccessful,
	const int32 ControllerId,
	FUniqueNetIdPtr UserId,
	const FOnlineSessionSearchResult& InviteResult)
{
	if (!bWasSuccessful || !UserId.IsValid() || !InviteResult.IsValid() || !SessionInterface.IsValid())
	{
		FRONTIER_LOG(Warning, TEXT("Steam party invite acceptance did not contain a valid lobby result."));
		return;
	}

	if (const FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(PartySessionName))
	{
		if (ExistingSession->GetSessionIdStr() == InviteResult.GetSessionIdStr())
		{
			RefreshPartyMembers();
			TryRequestJoinBackendParty();
			return;
		}

		PendingInviteResult = InviteResult;
		PendingInviteControllerId = ControllerId;
		bHasPendingInvite = true;
		if (bBackendPartyJoined && !BackendPartyId.IsEmpty())
		{
			RequestExitBackendParty();
		}
		else if (!DestroySteamLobbyAfterBackendExit())
		{
			bHasPendingInvite = false;
			FRONTIER_LOG(Warning, TEXT("Existing Steam party lobby could not be left before accepting an invite."));
		}
		return;
	}

	PendingInviteResult = InviteResult;
	PendingInviteControllerId = ControllerId;
	bHasPendingInvite = true;
	ResetBackendPartyState();
	JoinPendingInvite();
}

void UFrontierSteamPartySubsystem::HandleExistingSessionDestroyedForInvite(
	const FName SessionName,
	const bool bWasSuccessful)
{
	if (SessionName != PartySessionName)
	{
		return;
	}

	if (!bWasSuccessful)
	{
		bHasPendingInvite = false;
		ResetBackendPartyState();
		FRONTIER_LOG(Warning, TEXT("Steam party invite was not joined because the previous lobby could not be left."));
		return;
	}

	ResetBackendPartyState();
	if (bHasPendingInvite)
	{
		JoinPendingInvite();
	}
}

void UFrontierSteamPartySubsystem::JoinPendingInvite()
{
	if (!bHasPendingInvite || !SessionInterface.IsValid())
	{
		return;
	}

	const FOnlineSessionSearchResult InviteResult = PendingInviteResult;
	const int32 ControllerId = PendingInviteControllerId;
	bHasPendingInvite = false;
	if (!SessionInterface->JoinSession(ControllerId, PartySessionName, InviteResult))
	{
		FRONTIER_LOG(Warning, TEXT("Steam party lobby join could not be started."));
	}
}

void UFrontierSteamPartySubsystem::RequestCreateBackendParty(const bool bForceTokenRefresh)
{
	if (bBackendPartyRequestInProgress || bBackendPartyJoined)
	{
		return;
	}

	const FString SteamLobbyId = GetSteamLobbyId();
	if (SteamLobbyId.IsEmpty() || !IsLocalSteamLobbyOwner())
	{
		bOpenInviteOverlayAfterCreate = false;
		FRONTIER_LOG(Warning, TEXT("Backend party creation requires ownership of a valid Steam Lobby."));
		return;
	}

	UFrontierPlayerSessionSubsystem* PlayerSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession || !PlayerSession->HasAuthenticatedSession() || !PartyHttpClient.IsValid())
	{
		bOpenInviteOverlayAfterCreate = false;
		FRONTIER_LOG(Warning, TEXT("Backend party creation requires an authenticated player session."));
		return;
	}

	if (!bForceTokenRefresh)
	{
		bBackendAuthRetryAttempted = false;
	}
	if (BackendMutationIdempotencyKey.IsEmpty())
	{
		BackendMutationIdempotencyKey = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	}

	bBackendPartyRequestInProgress = true;
	const TWeakObjectPtr<UFrontierSteamPartySubsystem> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis](const bool bSucceeded, const FString& AccessToken, const FString& Error)
		{
			UFrontierSteamPartySubsystem* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded || AccessToken.IsEmpty())
			{
				This->bBackendPartyRequestInProgress = false;
				This->bOpenInviteOverlayAfterCreate = false;
				FRONTIER_LOG(Warning, TEXT("Backend party creation could not acquire an access token. Error=%s"), *Error);
				return;
			}
			This->StartCreateBackendPartyRequest(AccessToken);
		},
		bForceTokenRefresh);
}

void UFrontierSteamPartySubsystem::StartCreateBackendPartyRequest(const FString& AccessToken)
{
	const FString SteamLobbyId = GetSteamLobbyId();
	FString StartError;
	const TWeakObjectPtr<UFrontierSteamPartySubsystem> WeakThis(this);
	if (!PartyHttpClient.IsValid()
		|| !PartyHttpClient->CreateParty(
			AccessToken,
			SteamLobbyId,
			BackendMutationIdempotencyKey,
			[WeakThis](const FFrontierOnlinePartyResponse& Response)
			{
				if (UFrontierSteamPartySubsystem* This = WeakThis.Get())
				{
					This->HandleCreateBackendPartyResponse(Response);
				}
			},
			StartError))
	{
		bBackendPartyRequestInProgress = false;
		bOpenInviteOverlayAfterCreate = false;
		FRONTIER_LOG(Warning, TEXT("Backend party creation request could not start. Error=%s"), *StartError);
	}
}

void UFrontierSteamPartySubsystem::HandleCreateBackendPartyResponse(
	const FFrontierOnlinePartyResponse& Response)
{
	if (!Response.bSuccess
		&& Response.HttpStatus == 401
		&& Response.ErrorCode == TEXT("ACCESS_TOKEN_EXPIRED")
		&& !bBackendAuthRetryAttempted)
	{
		bBackendPartyRequestInProgress = false;
		bBackendAuthRetryAttempted = true;
		RequestCreateBackendParty(true);
		return;
	}

	bBackendPartyRequestInProgress = false;
	if (!Response.bSuccess)
	{
		bOpenInviteOverlayAfterCreate = false;
		FRONTIER_LOG(
			Warning,
			TEXT("Backend party creation failed. HttpStatus=%d ErrorCode=%s RequestId=%s Message=%s"),
			Response.HttpStatus,
			*Response.ErrorCode,
			*Response.RequestId,
			*Response.Message);
		return;
	}

	const FString SteamLobbyId = GetSteamLobbyId();
	if (Response.Data.PartyId.IsEmpty()
		|| Response.Data.SteamLobbyId.IsEmpty()
		|| Response.Data.SteamLobbyId != SteamLobbyId)
	{
		bOpenInviteOverlayAfterCreate = false;
		FRONTIER_LOG(
			Warning,
			TEXT("Backend party creation returned mismatched identity data. ExpectedLobbyId=%s ActualLobbyId=%s PartyId=%s"),
			*SteamLobbyId,
			*Response.Data.SteamLobbyId,
			*Response.Data.PartyId);
		return;
	}

	BackendPartyId = Response.Data.PartyId;
	bBackendPartyJoined = true;
	BackendMutationIdempotencyKey.Reset();
	if (!WriteBackendPartyIdToSteamLobby(BackendPartyId))
	{
		bOpenInviteOverlayAfterCreate = false;
		FRONTIER_LOG(Warning, TEXT("Backend party was created, but its ID could not be published to Steam Lobby metadata. PartyId=%s"), *BackendPartyId);
		return;
	}

	FRONTIER_LOG(
		Log,
		TEXT("Backend party creation completed. PartyId=%s LobbyId=%s Members=%d"),
		*BackendPartyId,
		*SteamLobbyId,
		Response.Data.Members.Num());
	const bool bShouldOpenInviteOverlay = bOpenInviteOverlayAfterCreate;
	bOpenInviteOverlayAfterCreate = false;
	if (bShouldOpenInviteOverlay)
	{
		ShowInviteOverlay();
	}
}

void UFrontierSteamPartySubsystem::TryRequestJoinBackendParty()
{
	if (bBackendPartyJoined || bBackendPartyRequestInProgress || IsLocalSteamLobbyOwner())
	{
		return;
	}

	const FString LobbyPartyId = ReadBackendPartyIdFromSteamLobby();
	if (LobbyPartyId.IsEmpty())
	{
		bAwaitingBackendPartyJoinMetadata = true;
		return;
	}

	BackendPartyId = LobbyPartyId;
	bAwaitingBackendPartyJoinMetadata = false;
	RequestJoinBackendParty();
}

void UFrontierSteamPartySubsystem::RequestJoinBackendParty(const bool bForceTokenRefresh)
{
	if (bBackendPartyRequestInProgress || bBackendPartyJoined)
	{
		return;
	}

	const FString SteamLobbyId = GetSteamLobbyId();
	if (SteamLobbyId.IsEmpty() || BackendPartyId.IsEmpty() || IsLocalSteamLobbyOwner())
	{
		FRONTIER_LOG(Warning, TEXT("Backend party join requires a member Steam Lobby and backend party ID."));
		return;
	}

	UFrontierPlayerSessionSubsystem* PlayerSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession || !PlayerSession->HasAuthenticatedSession() || !PartyHttpClient.IsValid())
	{
		FRONTIER_LOG(Warning, TEXT("Backend party join requires an authenticated player session."));
		return;
	}

	if (!bForceTokenRefresh)
	{
		bBackendAuthRetryAttempted = false;
	}
	if (BackendMutationIdempotencyKey.IsEmpty())
	{
		BackendMutationIdempotencyKey = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	}

	bBackendPartyRequestInProgress = true;
	const TWeakObjectPtr<UFrontierSteamPartySubsystem> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis](const bool bSucceeded, const FString& AccessToken, const FString& Error)
		{
			UFrontierSteamPartySubsystem* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded || AccessToken.IsEmpty())
			{
				This->bBackendPartyRequestInProgress = false;
				FRONTIER_LOG(Warning, TEXT("Backend party join could not acquire an access token. Error=%s"), *Error);
				return;
			}
			This->StartJoinBackendPartyRequest(AccessToken);
		},
		bForceTokenRefresh);
}

void UFrontierSteamPartySubsystem::StartJoinBackendPartyRequest(const FString& AccessToken)
{
	FString StartError;
	const TWeakObjectPtr<UFrontierSteamPartySubsystem> WeakThis(this);
	if (!PartyHttpClient.IsValid()
		|| !PartyHttpClient->JoinParty(
			AccessToken,
			BackendPartyId,
			GetSteamLobbyId(),
			BackendMutationIdempotencyKey,
			[WeakThis](const FFrontierOnlinePartyResponse& Response)
			{
				if (UFrontierSteamPartySubsystem* This = WeakThis.Get())
				{
					This->HandleJoinBackendPartyResponse(Response);
				}
			},
			StartError))
	{
		bBackendPartyRequestInProgress = false;
		FRONTIER_LOG(Warning, TEXT("Backend party join request could not start. Error=%s"), *StartError);
	}
}

void UFrontierSteamPartySubsystem::HandleJoinBackendPartyResponse(
	const FFrontierOnlinePartyResponse& Response)
{
	if (!Response.bSuccess
		&& Response.HttpStatus == 401
		&& Response.ErrorCode == TEXT("ACCESS_TOKEN_EXPIRED")
		&& !bBackendAuthRetryAttempted)
	{
		bBackendPartyRequestInProgress = false;
		bBackendAuthRetryAttempted = true;
		RequestJoinBackendParty(true);
		return;
	}

	bBackendPartyRequestInProgress = false;
	if (!Response.bSuccess)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Backend party join failed. PartyId=%s HttpStatus=%d ErrorCode=%s RequestId=%s Message=%s"),
			*BackendPartyId,
			Response.HttpStatus,
			*Response.ErrorCode,
			*Response.RequestId,
			*Response.Message);
		return;
	}

	if ((!Response.Data.PartyId.IsEmpty() && Response.Data.PartyId != BackendPartyId)
		|| (!Response.Data.SteamLobbyId.IsEmpty() && Response.Data.SteamLobbyId != GetSteamLobbyId()))
	{
		FRONTIER_LOG(Warning, TEXT("Backend party join returned identity data that does not match the current Steam Lobby."));
		return;
	}

	bBackendPartyJoined = true;
	bAwaitingBackendPartyJoinMetadata = false;
	BackendMutationIdempotencyKey.Reset();
	FRONTIER_LOG(
		Log,
		TEXT("Backend party join completed. PartyId=%s LobbyId=%s"),
		*BackendPartyId,
		*GetSteamLobbyId());
	RefreshMatchmakingContextFromSteamLobby();
}

void UFrontierSteamPartySubsystem::RequestExitBackendParty(const bool bForceTokenRefresh)
{
	if (bBackendPartyExitInProgress || BackendPartyId.IsEmpty())
	{
		return;
	}
	UFrontierPlayerSessionSubsystem* PlayerSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession || !PlayerSession->HasAuthenticatedSession() || !PartyHttpClient.IsValid())
	{
		FailBackendPartyExit(TEXT("Backend party exit requires an authenticated player session."));
		return;
	}
	if (!bForceTokenRefresh)
	{
		bBackendAuthRetryAttempted = false;
		bExitRequestWasDisband = IsLocalSteamLobbyOwner();
	}
	bBackendPartyExitInProgress = true;
	const TWeakObjectPtr<UFrontierSteamPartySubsystem> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis](const bool bSucceeded, const FString& AccessToken, const FString& Error)
		{
			UFrontierSteamPartySubsystem* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded || AccessToken.IsEmpty())
			{
				This->FailBackendPartyExit(Error);
				return;
			}
			This->StartExitBackendPartyRequest(AccessToken);
		},
		bForceTokenRefresh);
}

void UFrontierSteamPartySubsystem::StartExitBackendPartyRequest(const FString& AccessToken)
{
	FString StartError;
	const TWeakObjectPtr<UFrontierSteamPartySubsystem> WeakThis(this);
	const bool bStarted = bExitRequestWasDisband
		? PartyHttpClient->DisbandParty(
			AccessToken,
			BackendPartyId,
			[WeakThis](const FFrontierOnlinePartyResponse& Response)
			{
				if (UFrontierSteamPartySubsystem* This = WeakThis.Get())
				{
					This->HandleDisbandBackendPartyResponse(Response);
				}
			},
			StartError)
		: PartyHttpClient->LeaveParty(
			AccessToken,
			BackendPartyId,
			[WeakThis](const FFrontierOnlineLeavePartyResponse& Response)
			{
				if (UFrontierSteamPartySubsystem* This = WeakThis.Get())
				{
					This->HandleLeaveBackendPartyResponse(Response);
				}
			},
			StartError);
	if (!bStarted)
	{
		FailBackendPartyExit(StartError);
	}
}

void UFrontierSteamPartySubsystem::HandleLeaveBackendPartyResponse(
	const FFrontierOnlineLeavePartyResponse& Response)
{
	if (!Response.bSuccess && Response.HttpStatus == 401
		&& Response.ErrorCode == TEXT("ACCESS_TOKEN_EXPIRED") && !bBackendAuthRetryAttempted)
	{
		bBackendPartyExitInProgress = false;
		bBackendAuthRetryAttempted = true;
		RequestExitBackendParty(true);
		return;
	}
	if (!Response.bSuccess || Response.Data.PartyId != BackendPartyId)
	{
		FailBackendPartyExit(Response.Message.IsEmpty() ? TEXT("Backend party leave failed.") : Response.Message);
		return;
	}
	CompleteBackendPartyExit();
}

void UFrontierSteamPartySubsystem::HandleDisbandBackendPartyResponse(
	const FFrontierOnlinePartyResponse& Response)
{
	if (!Response.bSuccess && Response.HttpStatus == 401
		&& Response.ErrorCode == TEXT("ACCESS_TOKEN_EXPIRED") && !bBackendAuthRetryAttempted)
	{
		bBackendPartyExitInProgress = false;
		bBackendAuthRetryAttempted = true;
		RequestExitBackendParty(true);
		return;
	}
	if (!Response.bSuccess || !Response.bHasData || Response.Data.PartyId != BackendPartyId
		|| Response.Data.Status != TEXT("DISBANDED"))
	{
		FailBackendPartyExit(Response.Message.IsEmpty() ? TEXT("Backend party disband failed.") : Response.Message);
		return;
	}
	CompleteBackendPartyExit();
}

void UFrontierSteamPartySubsystem::CompleteBackendPartyExit()
{
	bBackendPartyExitInProgress = false;
	bBackendPartyJoined = false;
	BackendMutationIdempotencyKey.Reset();
	if (!DestroySteamLobbyAfterBackendExit())
	{
		bHasPendingInvite = false;
		ResetBackendPartyState();
		FRONTIER_LOG(Warning, TEXT("Backend party exit succeeded, but the Steam Lobby could not be destroyed."));
	}
}

void UFrontierSteamPartySubsystem::FailBackendPartyExit(const FString& Error)
{
	bBackendPartyExitInProgress = false;
	bHasPendingInvite = false;
	FRONTIER_LOG(Warning, TEXT("Backend party exit failed. PartyId=%s Error=%s"), *BackendPartyId, *Error);
}

bool UFrontierSteamPartySubsystem::DestroySteamLobbyAfterBackendExit()
{
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(PartySessionName))
	{
		ResetBackendPartyState();
		if (bHasPendingInvite)
		{
			JoinPendingInvite();
		}
		return true;
	}
	return SessionInterface->DestroySession(
		PartySessionName,
		FOnDestroySessionCompleteDelegate::CreateUObject(
			this,
			&ThisClass::HandleExistingSessionDestroyedForInvite));
}

bool UFrontierSteamPartySubsystem::IsLocalSteamLobbyOwner() const
{
	if (!SteamUser() || !SteamMatchmaking())
	{
		return false;
	}

	const uint64 LobbyIdValue = FCString::Strtoui64(*GetSteamLobbyId(), nullptr, 10);
	return LobbyIdValue != 0
		&& SteamMatchmaking()->GetLobbyOwner(CSteamID(LobbyIdValue)) == SteamUser()->GetSteamID();
}

FString UFrontierSteamPartySubsystem::ReadBackendPartyIdFromSteamLobby() const
{
	if (!SteamMatchmaking())
	{
		return FString();
	}

	const uint64 LobbyIdValue = FCString::Strtoui64(*GetSteamLobbyId(), nullptr, 10);
	if (LobbyIdValue == 0)
	{
		return FString();
	}

	const char* PartyId = SteamMatchmaking()->GetLobbyData(
		CSteamID(LobbyIdValue),
		BackendPartyIdLobbyDataKey);
	return PartyId && PartyId[0] != '\0' ? UTF8_TO_TCHAR(PartyId) : FString();
}

bool UFrontierSteamPartySubsystem::WriteBackendPartyIdToSteamLobby(const FString& PartyId) const
{
	if (PartyId.IsEmpty() || !SteamMatchmaking() || !IsLocalSteamLobbyOwner())
	{
		return false;
	}

	const uint64 LobbyIdValue = FCString::Strtoui64(*GetSteamLobbyId(), nullptr, 10);
	return LobbyIdValue != 0
		&& SteamMatchmaking()->SetLobbyData(
			CSteamID(LobbyIdValue),
			BackendPartyIdLobbyDataKey,
			TCHAR_TO_UTF8(*PartyId));
}

FString UFrontierSteamPartySubsystem::ReadSteamLobbyData(const char* Key) const
{
	if (!SteamMatchmaking() || !Key)
	{
		return FString();
	}
	const uint64 LobbyIdValue = FCString::Strtoui64(*GetSteamLobbyId(), nullptr, 10);
	if (LobbyIdValue == 0)
	{
		return FString();
	}
	const char* Value = SteamMatchmaking()->GetLobbyData(CSteamID(LobbyIdValue), Key);
	return Value && Value[0] != '\0' ? UTF8_TO_TCHAR(Value) : FString();
}

void UFrontierSteamPartySubsystem::RefreshMatchmakingContextFromSteamLobby()
{
	// Members must complete the authenticated Backend party join before trusting
	// matchmaking metadata distributed by Steam Lobby.
	if ((!IsLocalSteamLobbyOwner() && !bBackendPartyJoined) || BackendPartyId.IsEmpty())
	{
		return;
	}
	const FString TicketId = ReadSteamLobbyData(MatchmakingTicketIdLobbyDataKey);
	const FString MapId = ReadSteamLobbyData(MatchmakingMapIdLobbyDataKey);
	const FString PartyId = ReadSteamLobbyData(MatchmakingPartyIdLobbyDataKey);
	if (TicketId.IsEmpty() || MapId.IsEmpty() || PartyId.IsEmpty()
		|| PartyId != BackendPartyId
		|| (TicketId == LastMatchmakingTicketId
			&& MapId == LastMatchmakingMapId
			&& PartyId == LastMatchmakingPartyId))
	{
		if (!TicketId.IsEmpty() || !MapId.IsEmpty() || !PartyId.IsEmpty())
		{
			FRONTIER_LOG(
				Warning,
				TEXT("[Matchmaking] Steam Lobby matchmaking metadata was incomplete or mismatched. LobbyId=%s TicketId=%s MapId=%s MetadataPartyId=%s BackendPartyId=%s"),
				*GetSteamLobbyId(),
				TicketId.IsEmpty() ? TEXT("<empty>") : *TicketId,
				MapId.IsEmpty() ? TEXT("<empty>") : *MapId,
				PartyId.IsEmpty() ? TEXT("<empty>") : *PartyId,
				BackendPartyId.IsEmpty() ? TEXT("<empty>") : *BackendPartyId);
		}
		return;
	}
	LastMatchmakingTicketId = TicketId;
	LastMatchmakingMapId = MapId;
	LastMatchmakingPartyId = PartyId;
	FRONTIER_LOG(
		Log,
		TEXT("[Matchmaking] Steam Lobby matchmaking metadata adopted. LobbyId=%s TicketId=%s MapId=%s PartyId=%s"),
		*GetSteamLobbyId(),
		*TicketId,
		*MapId,
		*PartyId);
	OnMatchmakingContextChanged.Broadcast(TicketId, MapId, PartyId);
}

void UFrontierSteamPartySubsystem::ResetBackendPartyState()
{
	BackendPartyId.Reset();
	BackendMutationIdempotencyKey.Reset();
	bBackendPartyRequestInProgress = false;
	bBackendPartyJoined = false;
	bBackendAuthRetryAttempted = false;
	bAwaitingBackendPartyJoinMetadata = false;
	bBackendPartyExitInProgress = false;
	bExitRequestWasDisband = false;
	LastMatchmakingTicketId.Reset();
	LastMatchmakingMapId.Reset();
	LastMatchmakingPartyId.Reset();
}

bool UFrontierSteamPartySubsystem::ShowInviteOverlay() const
{
	IOnlineSubsystem* SteamSubsystem = IOnlineSubsystem::Get(TEXT("STEAM"));
	const IOnlineExternalUIPtr ExternalUI = SteamSubsystem ? SteamSubsystem->GetExternalUIInterface() : nullptr;
	if (!ExternalUI.IsValid() || !ExternalUI->ShowInviteUI(0, PartySessionName))
	{
		FRONTIER_LOG(Warning, TEXT("Steam invite overlay could not open. Ensure the Steam overlay is enabled."));
		return false;
	}

	return true;
}

UTexture2D* UFrontierSteamPartySubsystem::ResolveAvatarTexture(const uint64 SteamId)
{
	const FString SteamIdString = LexToString(SteamId);
	if (const TObjectPtr<UTexture2D>* CachedTexture = AvatarTextureCache.Find(SteamIdString))
	{
		return CachedTexture->Get();
	}

	if (!SteamFriends() || !SteamUtils())
	{
		return nullptr;
	}

	const int32 AvatarHandle = SteamFriends()->GetLargeFriendAvatar(CSteamID(SteamId));
	if (AvatarHandle <= 0)
	{
		// -1 means Steam has started an asynchronous avatar download. The
		// AvatarImageLoaded_t callback refreshes the member list when it arrives.
		return nullptr;
	}

	uint32 Width = 0;
	uint32 Height = 0;
	if (!SteamUtils()->GetImageSize(AvatarHandle, &Width, &Height)
		|| Width == 0
		|| Height == 0
		|| Width > 2048
		|| Height > 2048)
	{
		return nullptr;
	}

	TArray<uint8> ImageBytes;
	ImageBytes.SetNumUninitialized(static_cast<int32>(Width * Height * 4));
	if (!SteamUtils()->GetImageRGBA(AvatarHandle, ImageBytes.GetData(), ImageBytes.Num()))
	{
		return nullptr;
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(
		static_cast<int32>(Width),
		static_cast<int32>(Height),
		PF_R8G8B8A8,
		FName(*FString::Printf(TEXT("SteamAvatar_%s"), *SteamIdString)));
	if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.IsEmpty())
	{
		return nullptr;
	}

	Texture->SRGB = true;
	Texture->NeverStream = true;
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* MipData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(MipData, ImageBytes.GetData(), ImageBytes.Num());
	Mip.BulkData.Unlock();
	Texture->UpdateResource();

	AvatarTextureCache.Add(SteamIdString, Texture);
	return Texture;
}

void UFrontierSteamPartySubsystem::HandleSteamLobbyChanged(const uint64 SteamLobbyId)
{
	const FString CurrentLobbyId = GetSteamLobbyId();
	if (!CurrentLobbyId.IsEmpty()
		&& CurrentLobbyId == LexToString(SteamLobbyId))
	{
		RefreshPartyMembers();
		TryRequestJoinBackendParty();
		RefreshMatchmakingContextFromSteamLobby();
	}
}

void UFrontierSteamPartySubsystem::HandleSteamAvatarLoaded(const uint64 SteamId)
{
	AvatarTextureCache.Remove(LexToString(SteamId));
	RefreshPartyMembers();
}
