#include "Components/FrontierBackendProtocolComponent.h"

#include "Components/FrontierSkillTreePersistenceComponent.h"
#include "Components/FrontierLoadoutComponent.h"
#include "Components/FrontierLobbyBootstrapJsonParser.h"
#include "Components/FrontierStorageComponent.h"
#include "Components/FrontierRaidInventoryComponent.h"
#include "Frontier.h"
#include "FrontierOnlineConfig.h"
#include "Game/FrontierPlayerState.h"
#include "Game/FrontierLobbyPlayerController.h"
#include "Game/FrontierSteamPartySubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/FrontierBackendInventoryMapper.h"
#include "Inventory/Items/FrontierItemCatalogSubsystem.h"
#include "Progression/FrontierTemporarySkillPointSubsystem.h"
#include "Online/FrontierPlayerSessionSubsystem.h"
#include "Online/FrontierBackendWebSocketSubsystem.h"
#include "Progression/FrontierRaidSessionSubsystem.h"
#include "TimerManager.h"

namespace
{
FFrontierPlayerLevelSnapshot BuildPlayerLevelSnapshot(const FFrontierOnlinePlayerLevelDTO& Source)
{
	FFrontierPlayerLevelSnapshot Result;
	Result.Level = Source.Level;
	Result.TotalExperience = Source.TotalExperience;
	Result.CurrentLevelExperience = Source.CurrentLevelExperience;
	Result.bHasNextLevelRequiredExperience = Source.bHasNextLevelRequiredExperience;
	Result.NextLevelRequiredExperience = Source.NextLevelRequiredExperience;
	Result.MaxLevel = Source.MaxLevel;
	Result.UpdatedAt = Source.UpdatedAt;
	return Result;
}

bool IsOlderPlayerLevelSnapshot(
	const FFrontierPlayerLevelSnapshot& Incoming,
	const FFrontierPlayerLevelSnapshot& Accepted)
{
	if (Incoming.UpdatedAt.IsEmpty() || Accepted.UpdatedAt.IsEmpty())
	{
		return false;
	}
	FDateTime IncomingTime;
	FDateTime AcceptedTime;
	return FDateTime::ParseIso8601(*Incoming.UpdatedAt, IncomingTime)
		&& FDateTime::ParseIso8601(*Accepted.UpdatedAt, AcceptedTime)
		&& IncomingTime < AcceptedTime;
}

FString ResolveLoginAccountId(const FFrontierBackendSteamLoginResult& LoginResult)
{
	return !LoginResult.Player.PlayerIdString.IsEmpty()
		? LoginResult.Player.PlayerIdString
		: LoginResult.Player.SteamId;
}

void ApplyProfileDisplayNameToOwner(UActorComponent* Component, const FString& DisplayName)
{
	if (!Component || DisplayName.IsEmpty())
	{
		return;
	}

	if (APlayerController* OwnerController = Cast<APlayerController>(Component->GetOwner()))
	{
		if (AFrontierPlayerState* PlayerState = OwnerController->GetPlayerState<AFrontierPlayerState>())
		{
			PlayerState->SetPlayerName(DisplayName);
		}
	}
}

bool IsTemporarilySkippableMissingItemDataAssetError(const FString& Error)
{
	return Error.StartsWith(TEXT("No Item DataAsset found for "));
}

FString RaidFlowStateToString(const UFrontierRaidSessionSubsystem* RaidSession)
{
	if (!RaidSession)
	{
		return TEXT("<subsystem unavailable>");
	}
	const EFrontierRaidFlowState State = RaidSession->GetClientState();
	return FString::Printf(
		TEXT("%s(%d)"),
		*StaticEnum<EFrontierRaidFlowState>()->GetNameStringByValue(static_cast<int64>(State)),
		static_cast<int32>(State));
}
}

UFrontierBackendProtocolComponent::UFrontierBackendProtocolComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
	OnlineHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
}

void UFrontierBackendProtocolComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UFrontierBackendWebSocketSubsystem* WebSocket = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierBackendWebSocketSubsystem>()
		: nullptr)
	{
		WebSocket->OnConnected.AddUObject(this, &UFrontierBackendProtocolComponent::HandleBackendWebSocketConnected);
		WebSocket->OnDisconnected.AddUObject(this, &UFrontierBackendProtocolComponent::HandleBackendWebSocketDisconnected);
		WebSocket->OnMatchmakingStatus.AddUObject(this, &UFrontierBackendProtocolComponent::HandleBackendMatchmakingStatus);
		WebSocket->OnRaidServerReady.AddUObject(this, &UFrontierBackendProtocolComponent::HandleBackendRaidServerReady);
		WebSocket->OnMatchmakingFailed.AddUObject(this, &UFrontierBackendProtocolComponent::HandleBackendMatchmakingFailed);
	}
	if (UFrontierSteamPartySubsystem* SteamParty = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierSteamPartySubsystem>()
		: nullptr)
	{
		SteamParty->OnMatchmakingContextChanged.AddUObject(
			this,
			&UFrontierBackendProtocolComponent::HandleSteamMatchmakingContextChanged);
	}
}

void UFrontierBackendProtocolComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(MatchmakingPollTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(RaidEntryAfterServerReadyTimerHandle);
		if (UFrontierBackendWebSocketSubsystem* WebSocket = GetWorld()->GetGameInstance()
			? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierBackendWebSocketSubsystem>()
			: nullptr)
		{
			WebSocket->OnConnected.RemoveAll(this);
			WebSocket->OnDisconnected.RemoveAll(this);
			WebSocket->OnMatchmakingStatus.RemoveAll(this);
			WebSocket->OnRaidServerReady.RemoveAll(this);
			WebSocket->OnMatchmakingFailed.RemoveAll(this);
		}
		if (UFrontierSteamPartySubsystem* SteamParty = GetWorld()->GetGameInstance()
			? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierSteamPartySubsystem>()
			: nullptr)
		{
			SteamParty->OnMatchmakingContextChanged.RemoveAll(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UFrontierBackendProtocolComponent::LoginWithSteamAuthTicket(const FString& AuthTicket)
{
	AActor* OwnerActor = GetOwner();
	if (OwnerActor && !OwnerActor->HasAuthority())
	{
		bRequestInProgress = true;
		ServerLoginWithSteamAuthTicket(AuthTicket);
		return;
	}

	if (bMatchmakingOperationInProgress)
	{
		FRONTIER_LOG(Warning, TEXT("Steam backend login ignored because a request is already in progress."));
		return;
	}

	if (!OnlineHttpClient)
	{
		OnlineHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
	}

	TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	FString StartError;
	bRequestInProgress = true;
	const bool bStarted = OnlineHttpClient->LoginWithSteamTicket(
		AuthTicket,
		[WeakThis](const FFrontierOnlineSteamLoginResponse& Response)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleSteamAuthResponse(Response);
			}
		},
		StartError);

	if (!bStarted)
	{
		BroadcastFailure(StartError);
		return;
	}

}

void UFrontierBackendProtocolComponent::ServerLoginWithSteamAuthTicket_Implementation(const FString& AuthTicket)
{
	LoginWithSteamAuthTicket(AuthTicket);
}

void UFrontierBackendProtocolComponent::RequestUpdateNickname(const FString& Nickname)
{
	FString NormalizedNickname = Nickname;
	NormalizedNickname.TrimStartAndEndInline();
	if (NormalizedNickname.IsEmpty())
	{
		OnNicknameUpdateFailed.Broadcast(TEXT("닉네임을 입력해 주세요."));
		return;
	}

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ServerRequestUpdateNickname(NormalizedNickname);
		return;
	}

	StartNicknameUpdateRequest(NormalizedNickname);
}

void UFrontierBackendProtocolComponent::ServerRequestUpdateNickname_Implementation(const FString& Nickname)
{
	RequestUpdateNickname(Nickname);
}

void UFrontierBackendProtocolComponent::StartNicknameUpdateRequest(const FString& Nickname)
{
	UE_LOG(LogFrontier, Log, TEXT("[Nickname] Starting profile update. NicknameLength=%d"), Nickname.Len());
	if (bRequestInProgress)
	{
		UE_LOG(LogFrontier, Warning, TEXT("[Nickname] Profile update rejected locally because another backend request is active."));
		BroadcastNicknameUpdateFailed(TEXT("다른 백엔드 요청이 처리 중입니다."));
		return;
	}

	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession || !PlayerSession->HasAuthenticatedSession())
	{
		UE_LOG(LogFrontier, Warning, TEXT("[Nickname] Profile update rejected because no authenticated session is available."));
		BroadcastNicknameUpdateFailed(TEXT("로그인 세션을 확인할 수 없습니다."));
		return;
	}

	if (!OnlineHttpClient)
	{
		OnlineHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
	}

	const FString Locale = PlayerSession->GetLocale();
	if (Locale.IsEmpty())
	{
		BroadcastNicknameUpdateFailed(TEXT("프로필 locale을 확인할 수 없습니다."));
		return;
	}

	bRequestInProgress = true;
	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis, Nickname, Locale](const bool bSucceeded, const FString& AccessToken, const FString& Error)
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded)
			{
				UE_LOG(LogFrontier, Warning, TEXT("[Nickname] Access token acquisition failed. Error=%s"), *Error);
				This->bRequestInProgress = false;
				This->BroadcastNicknameUpdateFailed(
					Error.IsEmpty() ? TEXT("인증 토큰을 확인할 수 없습니다.") : Error);
				return;
			}

			FString StartError;
			const FString IdempotencyKey = FString::Printf(
				TEXT("profile-display-name-%s"),
				*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
			if (!This->OnlineHttpClient->UpdateMyProfile(
				AccessToken,
				Nickname,
				Locale,
				IdempotencyKey,
				[WeakThis, Nickname](const FFrontierOnlineProfileResponse& Response)
				{
					if (UFrontierBackendProtocolComponent* InnerThis = WeakThis.Get())
					{
						InnerThis->HandleNicknameUpdateResponse(Response, Nickname);
					}
				},
				StartError))
			{
				UE_LOG(LogFrontier, Warning, TEXT("[Nickname] Profile HTTP request could not be started. Error=%s"), *StartError);
				This->bRequestInProgress = false;
				This->BroadcastNicknameUpdateFailed(StartError);
			}
		});
}

void UFrontierBackendProtocolComponent::HandleNicknameUpdateResponse(
	const FFrontierOnlineProfileResponse& Response,
	const FString& RequestedNickname)
{
	bRequestInProgress = false;
	UE_LOG(LogFrontier, Log, TEXT("[Nickname] Profile response received. HttpStatus=%d TransportSucceeded=%d Success=%d PlayerId=%lld DisplayNameLength=%d Locale=%s ErrorCode=%s Message=%s"),
		Response.HttpStatus,
		Response.bTransportSucceeded ? 1 : 0,
		Response.bSuccess ? 1 : 0,
		Response.PlayerId,
		Response.DisplayName.Len(),
		*Response.Locale,
		*Response.ErrorCode,
		*Response.Message);
	if (!Response.bTransportSucceeded || !Response.bSuccess)
	{
		const FString ErrorMessage = Response.Message.IsEmpty()
			? TEXT("닉네임 변경에 실패했습니다.")
			: Response.Message;
		BroadcastNicknameUpdateFailed(ErrorMessage);
		return;
	}

	const FString UpdatedNickname = Response.DisplayName.IsEmpty() ? RequestedNickname : Response.DisplayName;
	if (UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr)
	{
		PlayerSession->SetNickname(UpdatedNickname);
	}
	LastSteamLoginResult.Player.Nickname = UpdatedNickname;
	ApplyProfileDisplayNameToOwner(this, UpdatedNickname);
	if (!Response.Locale.IsEmpty())
	{
		if (UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
			? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
			: nullptr)
		{
			PlayerSession->SetLocale(Response.Locale);
		}
		LastSteamLoginResult.Player.Locale = Response.Locale;
	}

	if (GetOwner() && GetOwner()->HasAuthority() && GetNetMode() != NM_Standalone)
	{
		UE_LOG(LogFrontier, Log, TEXT("[Nickname] Broadcasting success through client RPC."));
		ClientReceiveNicknameUpdateSucceeded(UpdatedNickname);
	}
	else
	{
		UE_LOG(LogFrontier, Log, TEXT("[Nickname] Broadcasting local success delegate."));
		OnNicknameUpdateSucceeded.Broadcast(UpdatedNickname);
	}
}

void UFrontierBackendProtocolComponent::BroadcastNicknameUpdateFailed(const FString& ErrorMessage)
{
	if (GetOwner() && GetOwner()->HasAuthority() && GetNetMode() != NM_Standalone)
	{
		ClientReceiveNicknameUpdateFailed(ErrorMessage);
	}
	else
	{
		OnNicknameUpdateFailed.Broadcast(ErrorMessage);
	}
}

void UFrontierBackendProtocolComponent::ResumeAuthenticatedLobbySession()
{
	if (bRequestInProgress)
	{
		FRONTIER_LOG(Warning, TEXT("Authenticated lobby resume ignored because a request is already in progress."));
		return;
	}

	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession || !PlayerSession->HasAuthenticatedSession())
	{
		BroadcastFailureLocal(TEXT("저장된 로그인 세션을 확인할 수 없습니다."));
		return;
	}

	FFrontierBackendSteamLoginResult LoginResult;
	LoginResult.bSuccess = true;
	LoginResult.Session.SessionId = PlayerSession->GetSessionId();
	LoginResult.Session.PlayerId = PlayerSession->GetPlayerId();
	LoginResult.Session.PlayerIdString = LexToString(PlayerSession->GetPlayerId());
	LoginResult.Player.PlayerId = PlayerSession->GetPlayerId();
	LoginResult.Player.PlayerIdString = LexToString(PlayerSession->GetPlayerId());
	LoginResult.Player.SteamId = PlayerSession->GetSteamId();
	LoginResult.Player.Nickname = PlayerSession->GetNickname();
	LoginResult.Player.Locale = PlayerSession->GetLocale();
	LastSteamLoginResult = LoginResult;

	FRONTIER_LOG(
		Log,
		TEXT("Resuming authenticated lobby session without requesting a new Steam ticket. PlayerId=%lld SessionId=%s"),
		PlayerSession->GetPlayerId(),
		*PlayerSession->GetSessionId());
	RequestLobbyBootstrapAfterLogin(MoveTemp(LoginResult));
}

void UFrontierBackendProtocolComponent::RequestStartMatchmaking(
	const FString& MapId,
	const FString& PartyId)
{
	RequestStartMatchmakingWithMode(MapId, PartyId.IsEmpty() ? TEXT("SOLO") : TEXT("PARTY"), PartyId);
}

void UFrontierBackendProtocolComponent::RequestStartMatchmakingWithMode(
	const FString& MapId,
	const FString& MatchMode,
	const FString& PartyId)
{
	FString NormalizedMapId = MapId;
	NormalizedMapId.TrimStartAndEndInline();
	FString NormalizedMatchMode = MatchMode;
	NormalizedMatchMode.TrimStartAndEndInline();
	NormalizedMatchMode.ToUpperInline();
	FString NormalizedPartyId = PartyId;
	NormalizedPartyId.TrimStartAndEndInline();
	if (UFrontierSteamPartySubsystem* SteamParty = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierSteamPartySubsystem>()
		: nullptr)
	{
		// A solo player still owns an automatically-created Steam Lobby.
		// Require BackendPartyId only after another player has joined it.
		if (SteamParty->HasOtherPartyMembers())
		{
			if (!SteamParty->IsLocalPartyLeader())
			{
				FRONTIER_LOG(
					Warning,
					TEXT("[Matchmaking] Direct matchmaking request rejected for non-leader. Controller=%s SteamLobbyId=%s"),
					*GetNameSafe(GetOwner()),
					*SteamParty->GetSteamLobbyId());
				BroadcastFailureLocal(TEXT("Only the party leader can start matchmaking."));
				return;
			}
			if (NormalizedPartyId.IsEmpty())
			{
				NormalizedPartyId = SteamParty->GetBackendPartyId();
			}
			if (NormalizedPartyId.IsEmpty())
			{
				FRONTIER_LOG(Warning, TEXT("[Matchmaking] Party matchmaking request rejected because BackendPartyId is unavailable."));
				BroadcastFailureLocal(TEXT("Party initialization is incomplete."));
				return;
			}
		}
	}
	if (!NormalizedPartyId.IsEmpty() && NormalizedMatchMode == TEXT("SOLO"))
	{
		NormalizedMatchMode = TEXT("PARTY");
	}
	const bool bValidMode = NormalizedMatchMode == TEXT("SOLO") || NormalizedMatchMode == TEXT("PARTY");
	if (NormalizedMapId.IsEmpty() || !bValidMode
		|| (NormalizedMatchMode == TEXT("SOLO") && !NormalizedPartyId.IsEmpty())
		|| bRequestInProgress || !PendingMatchmakingTicketId.IsEmpty())
	{
		BroadcastFailureLocal(TEXT("Matchmaking could not start because its mode, map, party, or request state is invalid."));
		return;
	}
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		bRequestInProgress = true;
		ServerRequestStartMatchmakingWithMode(NormalizedMapId, NormalizedMatchMode, NormalizedPartyId);
		return;
	}
	StartMatchmakingRequest(NormalizedMapId, NormalizedMatchMode, NormalizedPartyId);
}

void UFrontierBackendProtocolComponent::ServerRequestStartMatchmaking_Implementation(
	const FString& MapId,
	const FString& PartyId)
{
	StartMatchmakingRequest(MapId, PartyId.IsEmpty() ? TEXT("SOLO") : TEXT("PARTY"), PartyId);
}

void UFrontierBackendProtocolComponent::ServerRequestStartMatchmakingWithMode_Implementation(
	const FString& MapId,
	const FString& MatchMode,
	const FString& PartyId)
{
	StartMatchmakingRequest(MapId, MatchMode, PartyId);
}

void UFrontierBackendProtocolComponent::ServerAdoptPartyMatchmakingContext_Implementation(
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
	if (NormalizedTicketId.IsEmpty() || NormalizedMapId.IsEmpty() || NormalizedPartyId.IsEmpty()
		|| !LastSteamLoginResult.bSuccess || LastPlayerLevel.Level <= 0)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("[Matchmaking] Party context adoption rejected. TicketId=%s MapId=%s PartyId=%s LoginSuccess=%d PlayerLevel=%d"),
			NormalizedTicketId.IsEmpty() ? TEXT("<empty>") : *NormalizedTicketId,
			NormalizedMapId.IsEmpty() ? TEXT("<empty>") : *NormalizedMapId,
			NormalizedPartyId.IsEmpty() ? TEXT("<empty>") : *NormalizedPartyId,
			LastSteamLoginResult.bSuccess ? 1 : 0,
			LastPlayerLevel.Level);
		FailMatchmaking(TEXT("Party matchmaking context is invalid or lobby initialization is incomplete."), false);
		return;
	}
	if (!PendingMatchmakingTicketId.IsEmpty())
	{
		if (PendingMatchmakingTicketId != NormalizedTicketId)
		{
			FRONTIER_LOG(Warning, TEXT("[Matchmaking] Rejected a second party ticket while another ticket is active."));
		}
		return;
	}

	PendingMatchmakingTicketId = NormalizedTicketId;
	PendingMatchmakingMapId = NormalizedMapId;
	PendingMatchmakingMode = TEXT("PARTY");
	PendingMatchmakingPartyId = NormalizedPartyId;
	PendingMatchmakingMatchId.Reset();
	bRaidServerReadyReceived = false;
	bRaidEntryStartedForTicket = false;
	ClientSetMatchmakingRequestInProgress(true);
	FRONTIER_LOG(
		Log,
		TEXT("[Matchmaking] Party context adopted on server. Controller=%s TicketId=%s MapId=%s PartyId=%s"),
		*GetNameSafe(GetOwner()),
		*PendingMatchmakingTicketId,
		*PendingMatchmakingMapId,
		*PendingMatchmakingPartyId);

	// The Steam Lobby metadata only distributes identifiers. This authenticated
	// GET verifies that this player is an actual participant before Entry is allowed.
	PollMatchmakingTicket(false);
}

void UFrontierBackendProtocolComponent::ServerRelayRaidServerReady_Implementation(
	const FString& TicketId,
	const FString& MatchId)
{
	HandleRaidServerReadyNotification(TicketId, MatchId);
}

void UFrontierBackendProtocolComponent::StartMatchmakingRequest(
	const FString& MapId,
	const FString& MatchMode,
	const FString& PartyId)
{
	FString NormalizedMode = MatchMode;
	NormalizedMode.TrimStartAndEndInline();
	NormalizedMode.ToUpperInline();
	if (!GetOwner() || !GetOwner()->HasAuthority() || bRequestInProgress
		|| !LastSteamLoginResult.bSuccess || LastPlayerLevel.Level <= 0 || MapId.IsEmpty()
		|| (NormalizedMode != TEXT("SOLO") && NormalizedMode != TEXT("PARTY"))
		|| (NormalizedMode == TEXT("SOLO") && !PartyId.IsEmpty()))
	{
		FailMatchmaking(TEXT("Matchmaking requires server authority and completed lobby initialization."), false);
		return;
	}
	PendingMatchmakingMapId = MapId;
	PendingMatchmakingMapId.TrimStartAndEndInline();
	PendingMatchmakingMode = NormalizedMode;
	PendingMatchmakingPartyId = PartyId;
	PendingMatchmakingPartyId.TrimStartAndEndInline();
	PendingMatchmakingTicketId.Reset();
	PendingMatchmakingMatchId.Reset();
	bRaidServerReadyReceived = false;
	bRaidEntryStartedForTicket = false;
	bRequestInProgress = true;
	bMatchmakingOperationInProgress = true;
	BroadcastMatchmakingStatus(TEXT("REQUESTING"));
	SendCreateMatchmakingTicketAttempt(false);
}

void UFrontierBackendProtocolComponent::SendCreateMatchmakingTicketAttempt(
	const bool bAfterAccessTokenRefresh)
{
	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession)
	{
		FailMatchmaking(TEXT("Player session store is unavailable for matchmaking."), false);
		return;
	}
	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis, bAfterAccessTokenRefresh](
			const bool bSucceeded,
			const FString& AccessToken,
			const FString& Error)
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded)
			{
				This->FailMatchmaking(Error, false);
				return;
			}

			FFrontierOnlineCreateMatchmakingTicketRequest Request;
			Request.MapId = This->PendingMatchmakingMapId;
			Request.PartyId = This->PendingMatchmakingPartyId;
			Request.bHasPartyId = !Request.PartyId.IsEmpty();
			Request.MatchMode = This->PendingMatchmakingMode;
			This->MatchmakingHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
			FString StartError;
			if (!This->MatchmakingHttpClient->CreateMatchmakingTicket(
				AccessToken,
				Request,
				[WeakThis, bAfterAccessTokenRefresh](const FFrontierOnlineMatchmakingTicketResponse& Response)
				{
					if (UFrontierBackendProtocolComponent* InnerThis = WeakThis.Get())
					{
						InnerThis->HandleCreateMatchmakingTicketResponse(bAfterAccessTokenRefresh, Response);
					}
				},
				StartError))
			{
				This->FailMatchmaking(StartError, false);
			}
		},
		bAfterAccessTokenRefresh);
}

void UFrontierBackendProtocolComponent::HandleCreateMatchmakingTicketResponse(
	const bool bAfterAccessTokenRefresh,
	const FFrontierOnlineMatchmakingTicketResponse& Response)
{
	if (!Response.bSuccess && Response.HttpStatus == 401
		&& Response.ErrorCode == TEXT("ACCESS_TOKEN_EXPIRED") && !bAfterAccessTokenRefresh)
	{
		SendCreateMatchmakingTicketAttempt(true);
		return;
	}
	bRequestInProgress = false;
	bMatchmakingOperationInProgress = false;
	if (!Response.bSuccess || Response.Data.TicketId.IsEmpty())
	{
		FailMatchmaking(Response.Message.IsEmpty() ? TEXT("Matchmaking ticket creation failed.") : Response.Message, Response.bRetryable);
		return;
	}
	PendingMatchmakingTicketId = Response.Data.TicketId;
	PendingMatchmakingMatchId = Response.Data.bHasMatchId ? Response.Data.MatchId : FString();
	ClientReceiveMatchmakingContext(
		PendingMatchmakingTicketId,
		PendingMatchmakingMapId,
		PendingMatchmakingPartyId);
	if (UFrontierBackendWebSocketSubsystem* WebSocket = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierBackendWebSocketSubsystem>()
		: nullptr)
	{
		WebSocket->SubscribeMatchmaking(PendingMatchmakingTicketId);
	}
	FRONTIER_LOG(
		Log,
		TEXT("[Matchmaking] Ticket created. Controller=%s Mode=%s PartyId=%s TicketId=%s Status=%s MatchId=%s MatchStatus=%s"),
		*GetNameSafe(GetOwner()),
		*PendingMatchmakingMode,
		PendingMatchmakingPartyId.IsEmpty() ? TEXT("<none>") : *PendingMatchmakingPartyId,
		*PendingMatchmakingTicketId,
		*Response.Data.Status,
		PendingMatchmakingMatchId.IsEmpty() ? TEXT("<none>") : *PendingMatchmakingMatchId,
		Response.Data.bHasMatchStatus ? *Response.Data.MatchStatus : TEXT("<none>"));
	BroadcastMatchmakingStatus(
		Response.Data.bHasMatchStatus && !Response.Data.MatchStatus.IsEmpty()
			? Response.Data.MatchStatus
			: Response.Data.Status);
	if (Response.Data.Status == TEXT("FAILED") || Response.Data.Status == TEXT("CANCELLED"))
	{
		FailMatchmaking(TEXT("Matchmaking ticket entered a terminal failure state."), false);
		return;
	}
	if (Response.Data.bHasMatchStatus && Response.Data.MatchStatus == TEXT("SERVER_READY")
		&& Response.Data.bHasMatchId)
	{
		HandleRaidServerReadyNotification(Response.Data.TicketId, Response.Data.MatchId);
		return;
	}
}

void UFrontierBackendProtocolComponent::ScheduleMatchmakingTicketPoll()
{
	if (!GetWorld() || PendingMatchmakingTicketId.IsEmpty())
	{
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(
		MatchmakingPollTimerHandle,
		FTimerDelegate::CreateUObject(this, &UFrontierBackendProtocolComponent::PollMatchmakingTicket, false),
		MatchmakingPollIntervalSeconds,
		false);
}

void UFrontierBackendProtocolComponent::PollMatchmakingTicket(const bool bAfterAccessTokenRefresh)
{
	if (bMatchmakingOperationInProgress || PendingMatchmakingTicketId.IsEmpty())
	{
		return;
	}
	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession)
	{
		FailMatchmaking(TEXT("Player session store is unavailable while polling matchmaking."), false);
		return;
	}
	bMatchmakingOperationInProgress = true;
	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis, bAfterAccessTokenRefresh](
			const bool bSucceeded,
			const FString& AccessToken,
			const FString& Error)
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded)
			{
				This->FailMatchmaking(Error, false);
				return;
			}
			This->MatchmakingHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
			FString StartError;
			if (!This->MatchmakingHttpClient->GetMatchmakingTicket(
				AccessToken,
				This->PendingMatchmakingTicketId,
				[WeakThis, bAfterAccessTokenRefresh](const FFrontierOnlineMatchmakingTicketResponse& Response)
				{
					if (UFrontierBackendProtocolComponent* InnerThis = WeakThis.Get())
					{
						InnerThis->HandleMatchmakingTicketPollResponse(bAfterAccessTokenRefresh, Response);
					}
				},
				StartError))
			{
				This->FailMatchmaking(StartError, false);
			}
		},
		bAfterAccessTokenRefresh);
}

void UFrontierBackendProtocolComponent::HandleMatchmakingTicketPollResponse(
	const bool bAfterAccessTokenRefresh,
	const FFrontierOnlineMatchmakingTicketResponse& Response)
{
	bRequestInProgress = false;
	if (bRaidServerReadyReceived && !PendingMatchmakingTicketId.IsEmpty()
		&& !PendingMatchmakingMatchId.IsEmpty())
	{
		HandleRaidServerReadyNotification(PendingMatchmakingTicketId, PendingMatchmakingMatchId);
		return;
	}
	if (!Response.bSuccess && Response.HttpStatus == 401
		&& Response.ErrorCode == TEXT("ACCESS_TOKEN_EXPIRED") && !bAfterAccessTokenRefresh)
	{
		PollMatchmakingTicket(true);
		return;
	}
	if (!Response.bSuccess)
	{
		if (Response.bRetryable)
		{
			ScheduleMatchmakingTicketPoll();
			return;
		}
		FailMatchmaking(Response.Message.IsEmpty() ? TEXT("Matchmaking status lookup failed.") : Response.Message, false);
		return;
	}
	if (Response.Data.TicketId != PendingMatchmakingTicketId)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("[Matchmaking] Ticket poll ignored because TicketId differs. Expected=%s Received=%s Controller=%s"),
			*PendingMatchmakingTicketId,
			*Response.Data.TicketId,
			*GetNameSafe(GetOwner()));
		FailMatchmaking(TEXT("Matchmaking status returned a different ticketId."), false);
		return;
	}
	BroadcastMatchmakingStatus(
		Response.Data.bHasMatchStatus && !Response.Data.MatchStatus.IsEmpty()
			? Response.Data.MatchStatus
			: Response.Data.Status);
	if (Response.Data.Status == TEXT("FAILED") || Response.Data.Status == TEXT("CANCELLED"))
	{
		FailMatchmaking(TEXT("Matchmaking ended before a raid server became ready."), false);
		return;
	}
	if (Response.Data.bHasMatchId)
	{
		PendingMatchmakingMatchId = Response.Data.MatchId;
	}
	if (Response.Data.bHasMatchStatus && Response.Data.MatchStatus == TEXT("SERVER_READY")
		&& Response.Data.bHasMatchId)
	{
		HandleRaidServerReadyNotification(Response.Data.TicketId, Response.Data.MatchId);
		return;
	}
}

void UFrontierBackendProtocolComponent::CancelMatchmaking()
{
	if (PendingMatchmakingTicketId.IsEmpty() || bMatchmakingOperationInProgress || bRaidEntryStartedForTicket)
	{
		BroadcastFailureLocal(TEXT("There is no cancellable matchmaking ticket, or another request is active."));
		return;
	}
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		bMatchmakingOperationInProgress = true;
		ServerCancelMatchmaking();
		return;
	}
	bMatchmakingOperationInProgress = true;
	SendCancelMatchmakingTicketAttempt(false);
}

void UFrontierBackendProtocolComponent::ServerCancelMatchmaking_Implementation()
{
	if (PendingMatchmakingTicketId.IsEmpty() || bMatchmakingOperationInProgress || bRaidEntryStartedForTicket)
	{
		ClientSetMatchmakingOperationInProgress(false);
		ClientReceiveBackendRequestFailed(TEXT("The matchmaking ticket is no longer cancellable."));
		return;
	}
	bMatchmakingOperationInProgress = true;
	SendCancelMatchmakingTicketAttempt(false);
}

void UFrontierBackendProtocolComponent::SendCancelMatchmakingTicketAttempt(
	const bool bAfterAccessTokenRefresh)
{
	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession)
	{
		bMatchmakingOperationInProgress = false;
		ClientSetMatchmakingOperationInProgress(false);
		ClientReceiveBackendRequestFailed(TEXT("Player session store is unavailable while cancelling matchmaking."));
		return;
	}
	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis, bAfterAccessTokenRefresh](
			const bool bSucceeded,
			const FString& AccessToken,
			const FString& Error)
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded)
			{
				This->bMatchmakingOperationInProgress = false;
				This->ClientSetMatchmakingOperationInProgress(false);
				This->ClientReceiveBackendRequestFailed(Error);
				return;
			}
			This->MatchmakingHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
			FString StartError;
			if (!This->MatchmakingHttpClient->CancelMatchmakingTicket(
				AccessToken,
				This->PendingMatchmakingTicketId,
				[WeakThis, bAfterAccessTokenRefresh](const FFrontierOnlineMatchmakingTicketResponse& Response)
				{
					if (UFrontierBackendProtocolComponent* InnerThis = WeakThis.Get())
					{
						InnerThis->HandleCancelMatchmakingTicketResponse(bAfterAccessTokenRefresh, Response);
					}
				},
				StartError))
			{
				This->bMatchmakingOperationInProgress = false;
				This->ClientSetMatchmakingOperationInProgress(false);
				This->ClientReceiveBackendRequestFailed(StartError);
			}
		},
		bAfterAccessTokenRefresh);
}

void UFrontierBackendProtocolComponent::HandleCancelMatchmakingTicketResponse(
	const bool bAfterAccessTokenRefresh,
	const FFrontierOnlineMatchmakingTicketResponse& Response)
{
	if (!Response.bSuccess && Response.HttpStatus == 401
		&& Response.ErrorCode == TEXT("ACCESS_TOKEN_EXPIRED") && !bAfterAccessTokenRefresh)
	{
		SendCancelMatchmakingTicketAttempt(true);
		return;
	}
	bMatchmakingOperationInProgress = false;
	if (!Response.bSuccess || Response.Data.TicketId != PendingMatchmakingTicketId
		|| Response.Data.Status != TEXT("CANCELLED"))
	{
		ClientSetMatchmakingOperationInProgress(false);
		ClientReceiveBackendRequestFailed(Response.Message.IsEmpty()
			? TEXT("Matchmaking cancellation failed.")
			: Response.Message);
		return;
	}

	const FString CancelledTicketId = PendingMatchmakingTicketId;
	if (UFrontierBackendWebSocketSubsystem* WebSocket = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierBackendWebSocketSubsystem>()
		: nullptr)
	{
		WebSocket->UnsubscribeMatchmaking(CancelledTicketId);
	}
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(MatchmakingPollTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(RaidEntryAfterServerReadyTimerHandle);
	}
	PendingMatchmakingMapId.Reset();
	PendingMatchmakingMode.Reset();
	PendingMatchmakingPartyId.Reset();
	PendingMatchmakingTicketId.Reset();
	PendingMatchmakingMatchId.Reset();
	bRaidServerReadyReceived = false;
	bRaidEntryStartedForTicket = false;
	ClientReceiveMatchmakingCancelled(CancelledTicketId);
	FRONTIER_LOG(Log, TEXT("[Matchmaking] Ticket cancelled. TicketId=%s"), *CancelledTicketId);
}

void UFrontierBackendProtocolComponent::HandleRaidServerReadyNotification(
	const FString& TicketId,
	const FString& MatchId)
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		if (!TicketId.IsEmpty() && !MatchId.IsEmpty()
			&& TicketId == PendingMatchmakingTicketId
			&& (PendingMatchmakingMatchId.IsEmpty() || MatchId == PendingMatchmakingMatchId))
		{
			PendingMatchmakingMatchId = MatchId;
			bRaidServerReadyReceived = true;
			ServerRelayRaidServerReady(TicketId, MatchId);
		}
		return;
	}
	if (!GetOwner() || !GetOwner()->HasAuthority() || TicketId.IsEmpty() || MatchId.IsEmpty()
		|| TicketId != PendingMatchmakingTicketId
		|| (!PendingMatchmakingMatchId.IsEmpty() && MatchId != PendingMatchmakingMatchId))
	{
		FRONTIER_LOG(Warning, TEXT("[Matchmaking] Ignored mismatched or unauthorized SERVER_READY notification."));
		return;
	}
	PendingMatchmakingMatchId = MatchId;
	bRaidServerReadyReceived = true;
	BroadcastMatchmakingStatus(TEXT("SERVER_READY"));
	if (bRequestInProgress)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("[Matchmaking] SERVER_READY received while another backend request is active. Entry deferred. TicketId=%s MatchId=%s"),
			*PendingMatchmakingTicketId,
			*PendingMatchmakingMatchId);
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().SetTimer(
				RaidEntryAfterServerReadyTimerHandle,
				this,
				&UFrontierBackendProtocolComponent::RetryRaidServerReadyNotification,
				0.25f,
				false);
		}
		return;
	}
	if (bRaidEntryStartedForTicket)
	{
		return;
	}
	bRaidEntryStartedForTicket = true;
	bMatchmakingOperationInProgress = false;
	ClientSetMatchmakingRequestInProgress(false);
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(MatchmakingPollTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(RaidEntryAfterServerReadyTimerHandle);
	}
	FRONTIER_LOG(
		Log,
		TEXT("[Matchmaking] SERVER_READY gate passed. Delaying Raid Entry by 3 seconds. TicketId=%s MatchId=%s"),
		*TicketId,
		*MatchId);
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			RaidEntryAfterServerReadyTimerHandle,
			this,
			&UFrontierBackendProtocolComponent::StartRaidEntryAfterServerReady,
			3.0f,
			false);
	}
}

void UFrontierBackendProtocolComponent::RetryRaidServerReadyNotification()
{
	if (PendingMatchmakingTicketId.IsEmpty() || PendingMatchmakingMatchId.IsEmpty())
	{
		return;
	}

	HandleRaidServerReadyNotification(PendingMatchmakingTicketId, PendingMatchmakingMatchId);
}

void UFrontierBackendProtocolComponent::StartRaidEntryAfterServerReady()
{
	if (!bRaidServerReadyReceived || !bRaidEntryStartedForTicket || PendingMatchmakingTicketId.IsEmpty())
	{
		FRONTIER_LOG(
			Warning,
			TEXT("[Matchmaking] Delayed Raid Entry skipped. Ready=%d EntryMarked=%d TicketId=%s MatchId=%s MapId=%s"),
			bRaidServerReadyReceived ? 1 : 0,
			bRaidEntryStartedForTicket ? 1 : 0,
			PendingMatchmakingTicketId.IsEmpty() ? TEXT("<empty>") : *PendingMatchmakingTicketId,
			PendingMatchmakingMatchId.IsEmpty() ? TEXT("<empty>") : *PendingMatchmakingMatchId,
			PendingMatchmakingMapId.IsEmpty() ? TEXT("<empty>") : *PendingMatchmakingMapId);
		return;
	}

	RequestCreateRaidEntry(PendingMatchmakingMapId, PendingMatchmakingPartyId);
}

void UFrontierBackendProtocolComponent::HandleBackendWebSocketConnected()
{
	if (PendingMatchmakingTicketId.IsEmpty())
	{
		return;
	}
	if (UFrontierBackendWebSocketSubsystem* WebSocket = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierBackendWebSocketSubsystem>()
		: nullptr)
	{
		WebSocket->SubscribeMatchmaking(PendingMatchmakingTicketId);
	}
	// Recover a notification that may have been emitted while the socket was disconnected.
	PollMatchmakingTicket(false);
}

void UFrontierBackendProtocolComponent::HandleBackendWebSocketDisconnected()
{
	if (!PendingMatchmakingTicketId.IsEmpty() && !bMatchmakingOperationInProgress)
	{
		// One HTTP read recovers authoritative state while realtime delivery is unavailable.
		PollMatchmakingTicket(false);
	}
}

void UFrontierBackendProtocolComponent::HandleBackendMatchmakingStatus(
	const FFrontierOnlineMatchmakingStatusEvent& Event)
{
	if (Event.TicketId != PendingMatchmakingTicketId)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("[Matchmaking] Status event ignored because TicketId differs. Controller=%s Expected=%s Received=%s TicketStatus=%s MatchStatus=%s"),
			*GetNameSafe(GetOwner()),
			PendingMatchmakingTicketId.IsEmpty() ? TEXT("<empty>") : *PendingMatchmakingTicketId,
			Event.TicketId.IsEmpty() ? TEXT("<empty>") : *Event.TicketId,
			Event.TicketStatus.IsEmpty() ? TEXT("<empty>") : *Event.TicketStatus,
			Event.MatchStatus.IsEmpty() ? TEXT("<empty>") : *Event.MatchStatus);
		return;
	}
	FRONTIER_LOG(
		Log,
		TEXT("[Matchmaking] Status event received. Controller=%s TicketId=%s TicketStatus=%s MatchId=%s MatchStatus=%s PartyId=%s"),
		*GetNameSafe(GetOwner()),
		*Event.TicketId,
		Event.TicketStatus.IsEmpty() ? TEXT("<empty>") : *Event.TicketStatus,
		Event.MatchId.IsEmpty() ? TEXT("<empty>") : *Event.MatchId,
		Event.MatchStatus.IsEmpty() ? TEXT("<empty>") : *Event.MatchStatus,
		PendingMatchmakingPartyId.IsEmpty() ? TEXT("<none>") : *PendingMatchmakingPartyId);
	if (!Event.MatchId.IsEmpty())
	{
		PendingMatchmakingMatchId = Event.MatchId;
	}
	BroadcastMatchmakingStatus(
		!Event.MatchStatus.IsEmpty() ? Event.MatchStatus : Event.TicketStatus);
	if (Event.TicketStatus == TEXT("FAILED") || Event.TicketStatus == TEXT("CANCELLED"))
	{
		FailMatchmaking(TEXT("Backend WebSocket reported a terminal matchmaking state."), false);
		return;
	}
	if (Event.MatchStatus == TEXT("SERVER_READY") && !Event.MatchId.IsEmpty())
	{
		HandleRaidServerReadyNotification(Event.TicketId, Event.MatchId);
	}
}

void UFrontierBackendProtocolComponent::HandleBackendMatchmakingFailed(
	const FFrontierOnlineMatchmakingFailedEvent& Event)
{
	if (Event.TicketId != PendingMatchmakingTicketId)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("[Matchmaking] Failure event ignored because TicketId differs. Controller=%s Expected=%s Received=%s Message=%s"),
			*GetNameSafe(GetOwner()),
			PendingMatchmakingTicketId.IsEmpty() ? TEXT("<empty>") : *PendingMatchmakingTicketId,
			Event.TicketId.IsEmpty() ? TEXT("<empty>") : *Event.TicketId,
			Event.Message.IsEmpty() ? TEXT("<empty>") : *Event.Message);
		return;
	}
	FRONTIER_LOG(
		Warning,
		TEXT("[Matchmaking] Failure event received. Controller=%s TicketId=%s Retryable=%d Message=%s"),
		*GetNameSafe(GetOwner()),
		*Event.TicketId,
		Event.bRetryable ? 1 : 0,
		Event.Message.IsEmpty() ? TEXT("<empty>") : *Event.Message);
	FailMatchmaking(
		Event.Message.IsEmpty() ? TEXT("Backend reported that matchmaking failed.") : Event.Message,
		Event.bRetryable);
}

void UFrontierBackendProtocolComponent::HandleBackendRaidServerReady(
	const FFrontierOnlineRaidServerReadyEvent& Event)
{
	if (Event.MatchStatus != TEXT("SERVER_READY") || Event.RaidServerId.IsEmpty())
	{
		FRONTIER_LOG(
			Warning,
			TEXT("[Matchmaking] RAID_SERVER_READY ignored as incomplete. Controller=%s TicketId=%s MatchId=%s RaidServerId=%s MatchStatus=%s"),
			*GetNameSafe(GetOwner()),
			Event.TicketId.IsEmpty() ? TEXT("<empty>") : *Event.TicketId,
			Event.MatchId.IsEmpty() ? TEXT("<empty>") : *Event.MatchId,
			Event.RaidServerId.IsEmpty() ? TEXT("<empty>") : *Event.RaidServerId,
			Event.MatchStatus.IsEmpty() ? TEXT("<empty>") : *Event.MatchStatus);
		return;
	}
	FRONTIER_LOG(
		Log,
		TEXT("[Matchmaking] RAID_SERVER_READY received. Controller=%s TicketId=%s MatchId=%s RaidServerId=%s PartyId=%s"),
		*GetNameSafe(GetOwner()),
		*Event.TicketId,
		*Event.MatchId,
		*Event.RaidServerId,
		PendingMatchmakingPartyId.IsEmpty() ? TEXT("<none>") : *PendingMatchmakingPartyId);
	HandleRaidServerReadyNotification(Event.TicketId, Event.MatchId);
}

void UFrontierBackendProtocolComponent::HandleSteamMatchmakingContextChanged(
	const FString& TicketId,
	const FString& MapId,
	const FString& PartyId)
{
	const APlayerController* OwnerController = Cast<APlayerController>(GetOwner());
	if (!OwnerController || !OwnerController->IsLocalController() || TicketId.IsEmpty()
		|| MapId.IsEmpty() || PartyId.IsEmpty())
	{
		FRONTIER_LOG(
			Warning,
			TEXT("[Matchmaking] Steam party context ignored. HasLocalController=%d TicketId=%s MapId=%s PartyId=%s"),
			OwnerController && OwnerController->IsLocalController() ? 1 : 0,
			TicketId.IsEmpty() ? TEXT("<empty>") : *TicketId,
			MapId.IsEmpty() ? TEXT("<empty>") : *MapId,
			PartyId.IsEmpty() ? TEXT("<empty>") : *PartyId);
		return;
	}
	if (!PendingMatchmakingTicketId.IsEmpty())
	{
		FRONTIER_LOG(
			Warning,
			TEXT("[Matchmaking] Steam party context ignored because a ticket is already active. Controller=%s ExistingTicketId=%s ReceivedTicketId=%s"),
			*GetNameSafe(GetOwner()),
			*PendingMatchmakingTicketId,
			*TicketId);
		return;
	}
	FRONTIER_LOG(
		Log,
		TEXT("[Matchmaking] Steam party context received. Controller=%s TicketId=%s MapId=%s PartyId=%s Authority=%d"),
		*GetNameSafe(GetOwner()),
		*TicketId,
		*MapId,
		*PartyId,
		GetOwner() && GetOwner()->HasAuthority() ? 1 : 0);

	PendingMatchmakingTicketId = TicketId;
	PendingMatchmakingMapId = MapId;
	PendingMatchmakingMode = TEXT("PARTY");
	PendingMatchmakingPartyId = PartyId;
	PendingMatchmakingMatchId.Reset();
	bRaidServerReadyReceived = false;
	bRaidEntryStartedForTicket = false;
	if (UFrontierBackendWebSocketSubsystem* WebSocket = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierBackendWebSocketSubsystem>()
		: nullptr)
	{
		WebSocket->SubscribeMatchmaking(TicketId);
	}
	bRequestInProgress = true;
	if (GetOwner()->HasAuthority())
	{
		// Standalone frontend lobbies own their local controller and perform the
		// authenticated verification directly in this process.
		bRequestInProgress = false;
		PollMatchmakingTicket(false);
	}
	else
	{
		ServerAdoptPartyMatchmakingContext(TicketId, MapId, PartyId);
	}
	BroadcastMatchmakingStatus(TEXT("WAITING"));
	FRONTIER_LOG(
		Log,
		TEXT("[Matchmaking] Party ticket adopted from Steam Lobby. Controller=%s TicketId=%s MapId=%s PartyId=%s"),
		*GetNameSafe(GetOwner()),
		*TicketId,
		*MapId,
		*PartyId);
}

void UFrontierBackendProtocolComponent::FailMatchmaking(const FString& Error, const bool bRetryable)
{
	BroadcastMatchmakingStatus(TEXT("FAILED"));
	bRequestInProgress = false;
	bMatchmakingOperationInProgress = false;
	const FString FailedTicketId = PendingMatchmakingTicketId;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(MatchmakingPollTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(RaidEntryAfterServerReadyTimerHandle);
	}
	if (!FailedTicketId.IsEmpty())
	{
		if (UFrontierBackendWebSocketSubsystem* WebSocket = GetWorld() && GetWorld()->GetGameInstance()
			? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierBackendWebSocketSubsystem>()
			: nullptr)
		{
			WebSocket->UnsubscribeMatchmaking(FailedTicketId);
		}
	}
	PendingMatchmakingMapId.Reset();
	PendingMatchmakingMode.Reset();
	PendingMatchmakingPartyId.Reset();
	PendingMatchmakingTicketId.Reset();
	PendingMatchmakingMatchId.Reset();
	bRaidServerReadyReceived = false;
	bRaidEntryStartedForTicket = false;
	if (AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwner()))
	{
		ClientSetMatchmakingRequestInProgress(false);
		Controller->ClientReceiveRaidFlowFailed(Error, bRetryable);
	}
	else
	{
		BroadcastFailureLocal(Error);
	}
}

void UFrontierBackendProtocolComponent::RequestCreateRaidEntry(
	const FString& MapId,
	const FString& PartyId)
{
	FRONTIER_LOG_FUNC();

	FString NormalizedMapId = MapId;
	NormalizedMapId.TrimStartAndEndInline();
	FString NormalizedPartyId = PartyId;
	NormalizedPartyId.TrimStartAndEndInline();
	UFrontierRaidSessionSubsystem* RaidSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr;
	FRONTIER_LOG(
		Log,
		TEXT("[RaidEntry] Request received. Owner=%s Authority=%d NetMode=%d LoginSuccess=%d PlayerLevel=%d RequestInProgress=%d MapId=%s HasPartyId=%d State=%s RaidSessionId=%s"),
		*GetNameSafe(GetOwner()),
		GetOwner() && GetOwner()->HasAuthority() ? 1 : 0,
		static_cast<int32>(GetNetMode()),
		LastSteamLoginResult.bSuccess ? 1 : 0,
		LastPlayerLevel.Level,
		bRequestInProgress ? 1 : 0,
		NormalizedMapId.IsEmpty() ? TEXT("<empty>") : *NormalizedMapId,
		NormalizedPartyId.IsEmpty() ? 0 : 1,
		*RaidFlowStateToString(RaidSession),
		RaidSession && !RaidSession->GetLastRaidSessionId().IsEmpty()
			? *RaidSession->GetLastRaidSessionId()
			: TEXT("<empty>"));

	if (!LastSteamLoginResult.bSuccess || LastPlayerLevel.Level <= 0 || NormalizedMapId.IsEmpty())
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidEntry] Rejected before state transition: lobby prerequisites are incomplete. LoginSuccess=%d PlayerLevel=%d HasMapId=%d"),
			LastSteamLoginResult.bSuccess ? 1 : 0,
			LastPlayerLevel.Level,
			NormalizedMapId.IsEmpty() ? 0 : 1);
		BroadcastFailureLocal(TEXT("Raid entry requires completed login, bootstrap, level lookup, and map selection."));
		return;
	}

	FString StartError;
	if (!RaidSession || !RaidSession->BeginClientRaidEntry(StartError))
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidEntry] Rejected by client raid-flow guard. HasSubsystem=%d State=%s RaidSessionId=%s StartError=%s"),
			RaidSession ? 1 : 0,
			*RaidFlowStateToString(RaidSession),
			RaidSession && !RaidSession->GetLastRaidSessionId().IsEmpty()
				? *RaidSession->GetLastRaidSessionId()
				: TEXT("<empty>"),
			StartError.IsEmpty() ? TEXT("<empty>") : *StartError);
		BroadcastFailureLocal(StartError.IsEmpty() ? TEXT("Raid entry is already in progress.") : StartError);
		return;
	}

	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		FRONTIER_LOG(Log, TEXT("[RaidEntry] Forwarding entry request to server."));
		ServerRequestCreateRaidEntry(NormalizedMapId, NormalizedPartyId);
		return;
	}
	StartRaidEntryRequest(NormalizedMapId, NormalizedPartyId);
}

void UFrontierBackendProtocolComponent::ServerRequestCreateRaidEntry_Implementation(
	const FString& MapId,
	const FString& PartyId)
{
	FRONTIER_LOG_FUNC();

	StartRaidEntryRequest(MapId, PartyId);
}

void UFrontierBackendProtocolComponent::StartRaidEntryRequest(
	const FString& MapId,
	const FString& PartyId)
{
	FRONTIER_LOG_FUNC();

	AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwner());
	UFrontierRaidSessionSubsystem* RaidSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr;
	const bool bHasOwner = GetOwner() != nullptr;
	const bool bHasAuthority = bHasOwner && GetOwner()->HasAuthority();
	const bool bHasController = Controller != nullptr;
	const bool bHasRaidSession = RaidSession != nullptr;
	const bool bHasServerContext = bHasController
		&& bHasRaidSession
		&& RaidSession->HasServerRaidContext(Controller);
	if (!bHasOwner || !bHasAuthority || bRequestInProgress
		|| !bHasController || !bHasRaidSession
		|| bHasServerContext || MapId.IsEmpty()
		|| PendingMatchmakingTicketId.IsEmpty() || PendingMatchmakingMatchId.IsEmpty())
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidEntry] Server precondition rejected request. HasOwner=%d Authority=%d RequestInProgress=%d HasLobbyController=%d HasRaidSubsystem=%d HasServerRaidContext=%d HasMapId=%d State=%s RaidSessionId=%s"),
			bHasOwner ? 1 : 0,
			bHasAuthority ? 1 : 0,
			bRequestInProgress ? 1 : 0,
			bHasController ? 1 : 0,
			bHasRaidSession ? 1 : 0,
			bHasServerContext ? 1 : 0,
			MapId.IsEmpty() ? 0 : 1,
			*RaidFlowStateToString(RaidSession),
			RaidSession && !RaidSession->GetLastRaidSessionId().IsEmpty()
				? *RaidSession->GetLastRaidSessionId()
				: TEXT("<empty>"));
		if (Controller)
		{
			Controller->ClientReceiveRaidFlowFailed(
				TEXT("Raid entry could not start because the backend component is busy or unauthorized."),
				false);
		}
		return;
	}

	FFrontierOnlineCreateRaidEntryRequest Request;
	Request.MapId = MapId;
	Request.MapId.TrimStartAndEndInline();
	Request.PartyId = PartyId;
	Request.PartyId.TrimStartAndEndInline();
	Request.bHasPartyId = !Request.PartyId.IsEmpty();
	Request.TicketId = PendingMatchmakingTicketId;
	Request.bHasTicketId = true;
	Request.MatchId = PendingMatchmakingMatchId;
	Request.bHasMatchId = true;
	bRequestInProgress = true;
	FRONTIER_LOG(
		Log,
		TEXT("[RaidEntry] HTTP request preparation started. RaidDefinitionId=%s Region=%s MapId=%s HasPartyId=%d TicketId=%s MatchId=%s"),
		*Request.RaidDefinitionId,
		*Request.Region,
		*Request.MapId,
		Request.bHasPartyId ? 1 : 0,
		*Request.TicketId,
		*Request.MatchId);
	SendRaidEntryAttempt(MoveTemp(Request), false);
}

void UFrontierBackendProtocolComponent::SendRaidEntryAttempt(
	FFrontierOnlineCreateRaidEntryRequest Request,
	const bool bAfterAccessTokenRefresh)
{
	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession)
	{
		bRequestInProgress = false;
		FRONTIER_LOG(Error, TEXT("[RaidEntry] Access-token acquisition could not start: player session subsystem is unavailable."));
		if (AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwner()))
		{
			Controller->ClientReceiveRaidFlowFailed(TEXT("Player session store is unavailable."), false);
		}
		return;
	}

	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis, Request = MoveTemp(Request), bAfterAccessTokenRefresh](
			const bool bSucceeded,
			const FString& CurrentAccessToken,
			const FString& Error) mutable
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded)
			{
				This->bRequestInProgress = false;
				FRONTIER_LOG(
					Error,
					TEXT("[RaidEntry] Access-token acquisition failed. AfterRefresh=%d Error=%s"),
					bAfterAccessTokenRefresh ? 1 : 0,
					Error.IsEmpty() ? TEXT("<empty>") : *Error);
				if (AFrontierLobbyPlayerController* Controller =
					Cast<AFrontierLobbyPlayerController>(This->GetOwner()))
				{
					Controller->ClientReceiveRaidFlowFailed(Error, false);
				}
				return;
			}

			This->RaidEntryHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
			FString StartError;
			if (!This->RaidEntryHttpClient->CreateRaidEntry(
				CurrentAccessToken,
				Request,
				[WeakThis, Request, bAfterAccessTokenRefresh](
					const FFrontierOnlineRaidEntryResponse& Response)
				{
					if (UFrontierBackendProtocolComponent* InnerThis = WeakThis.Get())
					{
						InnerThis->HandleRaidEntryResponse(
							Request,
							bAfterAccessTokenRefresh,
							Response);
					}
				},
				StartError))
			{
				This->bRequestInProgress = false;
				FRONTIER_LOG(
					Error,
					TEXT("[RaidEntry] HTTP request failed to start. AfterRefresh=%d StartError=%s"),
					bAfterAccessTokenRefresh ? 1 : 0,
					StartError.IsEmpty() ? TEXT("<empty>") : *StartError);
				if (AFrontierLobbyPlayerController* Controller =
					Cast<AFrontierLobbyPlayerController>(This->GetOwner()))
				{
					Controller->ClientReceiveRaidFlowFailed(StartError, false);
				}
			}
		},
		bAfterAccessTokenRefresh);
}

void UFrontierBackendProtocolComponent::HandleRaidEntryResponse(
	const FFrontierOnlineCreateRaidEntryRequest& Request,
	const bool bAfterAccessTokenRefresh,
	const FFrontierOnlineRaidEntryResponse& Response)
{
	FRONTIER_LOG(
		Log,
		TEXT("[RaidEntry] HTTP response received. AfterRefresh=%d TransportSucceeded=%d Success=%d HttpStatus=%d ErrorCode=%s Message=%s Retryable=%d RequestId=%s ServerTime=%s"),
		bAfterAccessTokenRefresh ? 1 : 0,
		Response.bTransportSucceeded ? 1 : 0,
		Response.bSuccess ? 1 : 0,
		Response.HttpStatus,
		Response.ErrorCode.IsEmpty() ? TEXT("<empty>") : *Response.ErrorCode,
		Response.Message.IsEmpty() ? TEXT("<empty>") : *Response.Message,
		Response.bRetryable ? 1 : 0,
		Response.RequestId.IsEmpty() ? TEXT("<empty>") : *Response.RequestId,
		Response.ServerTime.IsEmpty() ? TEXT("<empty>") : *Response.ServerTime);
	if (!Response.bSuccess
		&& Response.HttpStatus == 401
		&& Response.ErrorCode.Equals(TEXT("ACCESS_TOKEN_EXPIRED"), ESearchCase::CaseSensitive)
		&& !bAfterAccessTokenRefresh)
	{
		SendRaidEntryAttempt(Request, true);
		return;
	}

	bRequestInProgress = false;
	if (!Response.bTransportSucceeded || !Response.bSuccess)
	{
		bRaidEntryStartedForTicket = false;
		ClientSetMatchmakingRequestInProgress(false);
		if (AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwner()))
		{
			Controller->ClientReceiveRaidFlowFailed(
				Response.Message.IsEmpty() ? TEXT("Raid entry failed.") : Response.Message,
				Response.bRetryable);
		}
		return;
	}

	EnterActiveRaidSession(Response.Data.RaidSession);
	if (UFrontierBackendWebSocketSubsystem* WebSocket = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierBackendWebSocketSubsystem>()
		: nullptr)
	{
		WebSocket->UnsubscribeMatchmaking(PendingMatchmakingTicketId);
	}
	PendingMatchmakingMapId.Reset();
	PendingMatchmakingMode.Reset();
	PendingMatchmakingPartyId.Reset();
	PendingMatchmakingTicketId.Reset();
	PendingMatchmakingMatchId.Reset();
	bRaidServerReadyReceived = false;
	bRaidEntryStartedForTicket = false;
	if (AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwner()))
	{
		Controller->HandleRaidEntryCreated(Response.Data);
	}
}

void UFrontierBackendProtocolComponent::ClientSetMatchmakingRequestInProgress_Implementation(
	const bool bInProgress)
{
	bRequestInProgress = bInProgress;
}

void UFrontierBackendProtocolComponent::ClientSetMatchmakingOperationInProgress_Implementation(
	const bool bInProgress)
{
	bMatchmakingOperationInProgress = bInProgress;
}

void UFrontierBackendProtocolComponent::ClientReceiveMatchmakingCancelled_Implementation(
	const FString& TicketId)
{
	OnMatchmakingStatusChanged.Broadcast(TEXT("CANCELLED"));
	if (UFrontierBackendWebSocketSubsystem* WebSocket = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierBackendWebSocketSubsystem>()
		: nullptr)
	{
		WebSocket->UnsubscribeMatchmaking(TicketId);
	}
	PendingMatchmakingMapId.Reset();
	PendingMatchmakingMode.Reset();
	PendingMatchmakingPartyId.Reset();
	PendingMatchmakingTicketId.Reset();
	PendingMatchmakingMatchId.Reset();
	bMatchmakingOperationInProgress = false;
	bRaidServerReadyReceived = false;
	bRaidEntryStartedForTicket = false;
	bRequestInProgress = false;
}

void UFrontierBackendProtocolComponent::ClientReceiveMatchmakingStatus_Implementation(
	const FString& Status)
{
	if (!Status.IsEmpty())
	{
		FRONTIER_LOG(
			Log,
			TEXT("[Matchmaking] Client status RPC received. Controller=%s Status=%s TicketId=%s PartyId=%s"),
			*GetNameSafe(GetOwner()),
			*Status,
			PendingMatchmakingTicketId.IsEmpty() ? TEXT("<empty>") : *PendingMatchmakingTicketId,
			PendingMatchmakingPartyId.IsEmpty() ? TEXT("<none>") : *PendingMatchmakingPartyId);
		OnMatchmakingStatusChanged.Broadcast(Status);
	}
}

void UFrontierBackendProtocolComponent::ClientReceiveMatchmakingContext_Implementation(
	const FString& TicketId,
	const FString& MapId,
	const FString& PartyId)
{
	if (TicketId.IsEmpty() || MapId.IsEmpty())
	{
		FRONTIER_LOG(
			Warning,
			TEXT("[Matchmaking] Client matchmaking context rejected. Controller=%s TicketId=%s MapId=%s PartyId=%s"),
			*GetNameSafe(GetOwner()),
			TicketId.IsEmpty() ? TEXT("<empty>") : *TicketId,
			MapId.IsEmpty() ? TEXT("<empty>") : *MapId,
			PartyId.IsEmpty() ? TEXT("<empty>") : *PartyId);
		return;
	}
	PendingMatchmakingTicketId = TicketId;
	PendingMatchmakingMapId = MapId;
	PendingMatchmakingMode = PartyId.IsEmpty() ? TEXT("SOLO") : TEXT("PARTY");
	PendingMatchmakingPartyId = PartyId;
	bMatchmakingOperationInProgress = false;
	bRequestInProgress = false;
	FRONTIER_LOG(
		Log,
		TEXT("[Matchmaking] Client matchmaking context applied. Controller=%s TicketId=%s MapId=%s PartyId=%s Mode=%s"),
		*GetNameSafe(GetOwner()),
		*PendingMatchmakingTicketId,
		*PendingMatchmakingMapId,
		PendingMatchmakingPartyId.IsEmpty() ? TEXT("<none>") : *PendingMatchmakingPartyId,
		*PendingMatchmakingMode);
	OnMatchmakingStatusChanged.Broadcast(TEXT("WAITING"));
	if (UFrontierBackendWebSocketSubsystem* WebSocket = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierBackendWebSocketSubsystem>()
		: nullptr)
	{
		WebSocket->SubscribeMatchmaking(TicketId);
	}

	if (!PartyId.IsEmpty())
	{
		UFrontierSteamPartySubsystem* SteamParty = GetWorld() && GetWorld()->GetGameInstance()
			? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierSteamPartySubsystem>()
			: nullptr;
		if (!SteamParty || !SteamParty->PublishMatchmakingContext(TicketId, MapId, PartyId))
		{
			FRONTIER_LOG(Warning, TEXT("[Matchmaking] Party ticket was created but could not be published to Steam Lobby members."));
		}
	}
}

void UFrontierBackendProtocolComponent::RefreshLobbyAfterRaid(
	const FString& RaidSessionId)
{
	if (RaidSessionId.IsEmpty())
	{
		BroadcastFailureLocal(TEXT("Raid lobby refresh requires a raidSessionId."));
		return;
	}
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerRefreshLobbyAfterRaid(RaidSessionId);
		return;
	}
	StartRaidLobbyRefresh(RaidSessionId);
}

void UFrontierBackendProtocolComponent::ServerRefreshLobbyAfterRaid_Implementation(
	const FString& RaidSessionId)
{
	StartRaidLobbyRefresh(RaidSessionId);
}

void UFrontierBackendProtocolComponent::ServerAcknowledgeRaidLobbyRefresh_Implementation(
	const FString& RaidSessionId,
	const bool bSucceeded)
{
	AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwner());
	UFrontierRaidSessionSubsystem* RaidSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr;
	if (!Controller || !RaidSession)
	{
		return;
	}
	if (bSucceeded)
	{
		RaidSession->CompleteServerLobbyRefresh(Controller, RaidSessionId);
	}
	else
	{
		RaidSession->MarkServerLobbyRefreshFailed(Controller, RaidSessionId);
	}
}

void UFrontierBackendProtocolComponent::StartRaidLobbyRefresh(
	const FString& RaidSessionId)
{
	if (PendingRaidRefreshSessionId.IsEmpty() == false
		|| RaidSessionId.IsEmpty() || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwner());
	UFrontierRaidSessionSubsystem* RaidSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr;
	FString FlowError;
	if (!Controller || !RaidSession
		|| !RaidSession->BeginServerLobbyRefresh(Controller, RaidSessionId, FlowError))
	{
		if (Controller)
		{
			Controller->ClientReceiveRaidFlowFailed(
				FlowError.IsEmpty() ? TEXT("Server Raid context could not begin lobby refresh.") : FlowError,
				true);
		}
		return;
	}
	PendingRaidRefreshSessionId = RaidSessionId;
	bRaidRefreshLevelPending = true;
	bRaidRefreshResultPending = true;
	bRaidRefreshLevelSucceeded = false;
	bRaidRefreshResultSucceeded = false;
	RaidRefreshFailure.Reset();
	PendingRaidRefreshLevel = FFrontierOnlinePlayerLevelResponse();
	PendingRaidRefreshResult = FFrontierOnlineRaidResultResponse();
	RequestRaidRefreshLevel(false);
	RequestRaidRefreshResult(false);
}

void UFrontierBackendProtocolComponent::RequestRaidRefreshLevel(
	const bool bAfterAccessTokenRefresh)
{
	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession)
	{
		bRaidRefreshLevelPending = false;
		RaidRefreshFailure = TEXT("Player session store is unavailable.");
		TryCompleteRaidLobbyRefresh();
		return;
	}
	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis, bAfterAccessTokenRefresh](
			const bool bSucceeded,
			const FString& CurrentAccessToken,
			const FString& Error)
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded)
			{
				This->bRaidRefreshLevelPending = false;
				This->RaidRefreshFailure = Error;
				This->TryCompleteRaidLobbyRefresh();
				return;
			}
			This->RaidRefreshLevelHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
			FString StartError;
			if (!This->RaidRefreshLevelHttpClient->GetPlayerLevel(
				CurrentAccessToken,
				[WeakThis, bAfterAccessTokenRefresh](const FFrontierOnlinePlayerLevelResponse& Response)
				{
					UFrontierBackendProtocolComponent* InnerThis = WeakThis.Get();
					if (!InnerThis)
					{
						return;
					}
					if (!Response.bSuccess && Response.HttpStatus == 401
						&& Response.ErrorCode.Equals(TEXT("ACCESS_TOKEN_EXPIRED"), ESearchCase::CaseSensitive)
						&& !bAfterAccessTokenRefresh)
					{
						InnerThis->RequestRaidRefreshLevel(true);
						return;
					}
					InnerThis->bRaidRefreshLevelPending = false;
					InnerThis->bRaidRefreshLevelSucceeded = Response.bTransportSucceeded && Response.bSuccess;
					if (InnerThis->bRaidRefreshLevelSucceeded)
					{
						InnerThis->PendingRaidRefreshLevel = Response;
					}
					else if (InnerThis->RaidRefreshFailure.IsEmpty())
					{
						InnerThis->RaidRefreshFailure = Response.Message;
					}
					InnerThis->TryCompleteRaidLobbyRefresh();
				},
				StartError))
			{
				This->bRaidRefreshLevelPending = false;
				This->RaidRefreshFailure = StartError;
				This->TryCompleteRaidLobbyRefresh();
			}
		},
		bAfterAccessTokenRefresh);
}

void UFrontierBackendProtocolComponent::RequestRaidRefreshResult(
	const bool bAfterAccessTokenRefresh)
{
	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession)
	{
		bRaidRefreshResultPending = false;
		RaidRefreshFailure = TEXT("Player session store is unavailable.");
		TryCompleteRaidLobbyRefresh();
		return;
	}
	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis, bAfterAccessTokenRefresh](
			const bool bSucceeded,
			const FString& CurrentAccessToken,
			const FString& Error)
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded)
			{
				This->bRaidRefreshResultPending = false;
				This->RaidRefreshFailure = Error;
				This->TryCompleteRaidLobbyRefresh();
				return;
			}
			This->RaidRefreshResultHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
			FString StartError;
			if (!This->RaidRefreshResultHttpClient->GetRaidResult(
				CurrentAccessToken,
				This->PendingRaidRefreshSessionId,
				true,
				[WeakThis, bAfterAccessTokenRefresh](const FFrontierOnlineRaidResultResponse& Response)
				{
					UFrontierBackendProtocolComponent* InnerThis = WeakThis.Get();
					if (!InnerThis)
					{
						return;
					}
					if (!Response.bSuccess && Response.HttpStatus == 401
						&& Response.ErrorCode.Equals(TEXT("ACCESS_TOKEN_EXPIRED"), ESearchCase::CaseSensitive)
						&& !bAfterAccessTokenRefresh)
					{
						InnerThis->RequestRaidRefreshResult(true);
						return;
					}
					if (Response.bSuccess && Response.bHasRetryAfterSeconds
						&& Response.RetryAfterSeconds > 0 && !Response.Result.bHasCommittedAt)
					{
						if (UWorld* World = InnerThis->GetWorld())
						{
							FTimerDelegate Retry;
							Retry.BindUObject(
								InnerThis,
								&UFrontierBackendProtocolComponent::RequestRaidRefreshResult,
								false);
							FTimerHandle Handle;
							World->GetTimerManager().SetTimer(
								Handle,
								Retry,
								static_cast<float>(Response.RetryAfterSeconds),
								false);
							return;
						}
					}
					InnerThis->bRaidRefreshResultPending = false;
					InnerThis->bRaidRefreshResultSucceeded = Response.bTransportSucceeded && Response.bSuccess;
					if (InnerThis->bRaidRefreshResultSucceeded)
					{
						InnerThis->PendingRaidRefreshResult = Response;
					}
					else if (InnerThis->RaidRefreshFailure.IsEmpty())
					{
						InnerThis->RaidRefreshFailure = Response.Message;
					}
					InnerThis->TryCompleteRaidLobbyRefresh();
				},
				StartError))
			{
				This->bRaidRefreshResultPending = false;
				This->RaidRefreshFailure = StartError;
				This->TryCompleteRaidLobbyRefresh();
			}
		},
		bAfterAccessTokenRefresh);
}

void UFrontierBackendProtocolComponent::TryCompleteRaidLobbyRefresh()
{
	if (!AreRaidRefreshInputsReady(
		bRaidRefreshLevelPending,
		bRaidRefreshResultPending,
		bRaidRefreshLevelSucceeded,
		bRaidRefreshResultSucceeded))
	{
		if (!bRaidRefreshLevelPending && !bRaidRefreshResultPending
			&& (!bRaidRefreshLevelSucceeded || !bRaidRefreshResultSucceeded))
		{
			if (AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwner()))
			{
				Controller->ClientReceiveRaidFlowFailed(
					RaidRefreshFailure.IsEmpty() ? TEXT("Lobby raid refresh failed.") : RaidRefreshFailure,
					true);
				if (UFrontierRaidSessionSubsystem* RaidSession = GetWorld() && GetWorld()->GetGameInstance()
					? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
					: nullptr)
				{
					RaidSession->MarkServerLobbyRefreshFailed(
						Controller,
						PendingRaidRefreshSessionId);
				}
			}
			PendingRaidRefreshSessionId.Reset();
		}
		return;
	}

	FString ApplyError;
	if ((PendingRaidRefreshResult.bHasInventory
			&& !ApplyInventoryDataToPlayerState(PendingRaidRefreshResult.Inventory, ApplyError))
		|| (PendingRaidRefreshResult.bHasEquipment
			&& !ApplyEquipmentDataToPlayerState(PendingRaidRefreshResult.Equipment, ApplyError)))
	{
		if (AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwner()))
		{
			Controller->ClientReceiveRaidFlowFailed(ApplyError, true);
			if (UFrontierRaidSessionSubsystem* RaidSession = GetWorld() && GetWorld()->GetGameInstance()
				? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
				: nullptr)
			{
				RaidSession->MarkServerLobbyRefreshFailed(
					Controller,
					PendingRaidRefreshSessionId);
			}
		}
		PendingRaidRefreshSessionId.Reset();
		return;
	}

	const FFrontierPlayerLevelSnapshot RaidLevel = BuildPlayerLevelSnapshot(PendingRaidRefreshLevel.Data);
	if (!IsOlderPlayerLevelSnapshot(RaidLevel, LastPlayerLevel))
	{
		LastPlayerLevel = RaidLevel;
	}
	else
	{
		FRONTIER_LOG(Warning, TEXT("Ignored an out-of-order older raid level snapshot. Incoming=%s Accepted=%s"), *RaidLevel.UpdatedAt, *LastPlayerLevel.UpdatedAt);
	}
	ClientReceiveRaidRefreshReady(
		LastPlayerLevel,
		PendingRaidRefreshResult.Result.RaidSessionId,
		PendingRaidRefreshResult.Result.Outcome,
		PendingRaidRefreshResult.Result.bHasReasonCode,
		PendingRaidRefreshResult.Result.ReasonCode,
		PendingRaidRefreshResult.Result.bHasCommittedAt,
		PendingRaidRefreshResult.Result.CommittedAt);
	PendingRaidRefreshSessionId.Reset();
}

bool UFrontierBackendProtocolComponent::AreRaidRefreshInputsReady(
	const bool bLevelPending,
	const bool bResultPending,
	const bool bLevelSucceeded,
	const bool bResultSucceeded)
{
	return !bLevelPending && !bResultPending && bLevelSucceeded && bResultSucceeded;
}

bool UFrontierBackendProtocolComponent::RequestUpgradeItem(
	const FGuid& ItemInstanceId,
	const int32 ExpectedEnhancementLevel,
	const FString& IdempotencyKey)
{
	auto RejectRequest = [this](const FString& Message, const bool bRetryable)
	{
		FFrontierItemUpgradeResult Result;
		Result.bRetryable = bRetryable;
		Result.Message = Message;
		BroadcastItemUpgradeResult(Result);
	};

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		RejectRequest(TEXT("Item upgrade must be requested by the authoritative lobby controller."), false);
		return false;
	}
	if (bInventoryMutationInProgress || bEquipmentMutationInProgress || bRequestInProgress)
	{
		RejectRequest(TEXT("Another backend request is already in progress."), true);
		return false;
	}
	if (!ItemInstanceId.IsValid() || ExpectedEnhancementLevel < 0 || IdempotencyKey.IsEmpty())
	{
		RejectRequest(TEXT("Item upgrade request contains an invalid item, enhancement level, or idempotency key."), false);
		return false;
	}

	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession)
	{
		RejectRequest(TEXT("The authenticated player session is unavailable."), true);
		return false;
	}

	bRequestInProgress = true;
	bInventoryMutationInProgress = true;
	const FString ItemInstanceIdString = ItemInstanceId.ToString(EGuidFormats::DigitsWithHyphensLower);
	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis, ItemInstanceIdString, ExpectedEnhancementLevel, IdempotencyKey](
			const bool bSucceeded,
			const FString& CurrentAccessToken,
			const FString& Error)
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded)
			{
				This->bRequestInProgress = false;
				This->bInventoryMutationInProgress = false;
				FFrontierItemUpgradeResult Result;
				Result.bRetryable = true;
				Result.Message = Error.IsEmpty() ? TEXT("Could not acquire an access token for item upgrade.") : Error;
				This->BroadcastItemUpgradeResult(Result);
				return;
			}
			if (!This->OnlineHttpClient)
			{
				This->OnlineHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
			}

			FString StartError;
			if (!This->OnlineHttpClient->UpgradeInventoryItem(
				CurrentAccessToken,
				ItemInstanceIdString,
				ExpectedEnhancementLevel,
				IdempotencyKey,
				[WeakThis](const FFrontierOnlineItemUpgradeResponse& Response)
				{
					if (UFrontierBackendProtocolComponent* InnerThis = WeakThis.Get())
					{
						InnerThis->HandleItemUpgradeResponse(Response);
					}
				},
				StartError))
			{
				This->bRequestInProgress = false;
				This->bInventoryMutationInProgress = false;
				FFrontierItemUpgradeResult Result;
				Result.bRetryable = true;
				Result.Message = StartError;
				This->BroadcastItemUpgradeResult(Result);
			}
		});
	return true;
}

bool UFrontierBackendProtocolComponent::RequestMoveInventoryItem(
	const FGuid& ItemInstanceId,
	const int32 TargetSlotIndex,
	TOptional<int32> Quantity)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		FRONTIER_LOG(Warning, TEXT("Inventory move request ignored on non-authority backend protocol component."));
		return false;
	}
	if (bInventoryMutationInProgress || bRequestInProgress)
	{
		FRONTIER_LOG(Warning, TEXT("Inventory move request ignored because another backend request is in progress."));
		return false;
	}
	if (!ItemInstanceId.IsValid())
	{
		FRONTIER_LOG(Warning, TEXT("Inventory move request ignored because itemInstanceId is invalid."));
		return false;
	}

	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession)
	{
		FRONTIER_LOG(Warning, TEXT("Inventory move request ignored because the player session store is unavailable."));
		return false;
	}

	const FString ItemInstanceIdString = ItemInstanceId.ToString(EGuidFormats::DigitsWithHyphensLower);

	FRONTIER_LOG(Log, TEXT("Backend inventory move requested. Operation=InventoryMove ItemInstanceId=%s TargetSlotIndex=%d"),
		*ItemInstanceIdString,
		TargetSlotIndex);

	bRequestInProgress = true;
	bInventoryMutationInProgress = true;
	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis,
			ItemInstanceIdString,
			TargetSlotIndex,
			Quantity](
			const bool bSucceeded,
			const FString& CurrentAccessToken,
			const FString& Error)
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded)
			{
				This->bRequestInProgress = false;
				This->bInventoryMutationInProgress = false;
				This->BroadcastFailure(Error);
				return;
			}
			if (!This->OnlineHttpClient)
			{
				This->OnlineHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
			}
			FString StartError;
			if (!This->OnlineHttpClient->MoveInventoryItem(
				CurrentAccessToken,
				ItemInstanceIdString,
				TargetSlotIndex,
				Quantity,
				[WeakThis](const FFrontierOnlineInventoryResponse& Response)
				{
					if (UFrontierBackendProtocolComponent* InnerThis = WeakThis.Get())
					{
						InnerThis->HandleInventoryMutationResponse(Response);
					}
				},
				StartError))
			{
				This->bRequestInProgress = false;
				This->bInventoryMutationInProgress = false;
				This->BroadcastFailure(StartError);
			}
		});

	return true;
}

bool UFrontierBackendProtocolComponent::RequestSwapInventoryItems(
	const int32 SourceSlotIndex,
	const int32 TargetSlotIndex,
	const FGuid& SourceItemInstanceId,
	const FGuid& TargetItemInstanceId)
{
	if (SourceSlotIndex == TargetSlotIndex
		|| !SourceItemInstanceId.IsValid()
		|| !TargetItemInstanceId.IsValid()
		|| PendingInventorySwap.Kind != EPendingInventorySwapKind::None)
	{
		return false;
	}

	PendingInventorySwap.Kind = EPendingInventorySwapKind::RaidInventory;
	PendingInventorySwap.SourceSlotIndex = SourceSlotIndex;
	PendingInventorySwap.TargetSlotIndex = TargetSlotIndex;
	PendingInventorySwap.SourceItemInstanceId = SourceItemInstanceId;
	PendingInventorySwap.TargetItemInstanceId = TargetItemInstanceId;

	const int32 SlotCount = GetOwner() && GetOwner()->GetWorld()
		? [&]() -> int32
		{
			if (APlayerController* Controller = Cast<APlayerController>(GetOwner()))
			{
				if (AFrontierPlayerState* PlayerState = Controller->GetPlayerState<AFrontierPlayerState>())
				{
					if (UFrontierRaidInventoryComponent* Inventory = PlayerState->GetRaidInventoryComponent())
					{
						for (int32 Index = 0; Index < Inventory->GetSlots().Num(); ++Index)
						{
							if (!Inventory->GetSlots()[Index].bOccupied)
							{
								return Index;
							}
						}
					}
				}
			}
			return INDEX_NONE;
		}()
		: INDEX_NONE;
	PendingInventorySwap.TargetTempSlotIndex = SlotCount;
	if (PendingInventorySwap.TargetTempSlotIndex == INDEX_NONE)
	{
		ClearPendingInventorySwap();
		return false;
	}

	PendingInventorySwap.Step = 0;
	if (!RequestMoveInventoryItem(SourceItemInstanceId, PendingInventorySwap.TargetTempSlotIndex))
	{
		ClearPendingInventorySwap();
		return false;
	}
	return true;
}

bool UFrontierBackendProtocolComponent::RequestDepositStorageItem(
	const FGuid& ItemInstanceId,
	TOptional<int32> TargetSlotIndex,
	TOptional<int32> Quantity)
{
	return RequestStorageTransfer(true, ItemInstanceId, MoveTemp(TargetSlotIndex), MoveTemp(Quantity));
}

bool UFrontierBackendProtocolComponent::RequestWithdrawStorageItem(
	const FGuid& ItemInstanceId,
	TOptional<int32> TargetSlotIndex,
	TOptional<int32> Quantity)
{
	return RequestStorageTransfer(false, ItemInstanceId, MoveTemp(TargetSlotIndex), MoveTemp(Quantity));
}

bool UFrontierBackendProtocolComponent::RequestStorageTransfer(
	const bool bDeposit,
	const FGuid& ItemInstanceId,
	TOptional<int32> TargetSlotIndex,
	TOptional<int32> Quantity)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		FRONTIER_LOG(Warning, TEXT("Storage transfer request ignored on non-authority backend protocol component."));
		return false;
	}
	if (bInventoryMutationInProgress || bRequestInProgress)
	{
		FRONTIER_LOG(Warning, TEXT("Storage transfer request ignored because another backend request is in progress."));
		return false;
	}
	if (!ItemInstanceId.IsValid())
	{
		FRONTIER_LOG(Warning, TEXT("Storage transfer request ignored because itemInstanceId is invalid."));
		return false;
	}

	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession)
	{
		FRONTIER_LOG(Warning, TEXT("Storage transfer request ignored because the player session store is unavailable."));
		return false;
	}

	const FString ItemInstanceIdString = ItemInstanceId.ToString(EGuidFormats::DigitsWithHyphensLower);
	FRONTIER_LOG(
		Log,
		TEXT("Backend storage transfer requested. Operation=%s ItemInstanceId=%s TargetSlotIndex=%d"),
		bDeposit ? TEXT("Deposit") : TEXT("Withdraw"),
		*ItemInstanceIdString,
		TargetSlotIndex.IsSet() ? TargetSlotIndex.GetValue() : INDEX_NONE);

	bRequestInProgress = true;
	bInventoryMutationInProgress = true;
	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis, bDeposit, ItemInstanceIdString, TargetSlotIndex, Quantity](
			const bool bSucceeded,
			const FString& CurrentAccessToken,
			const FString& Error)
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded)
			{
				This->bRequestInProgress = false;
				This->bInventoryMutationInProgress = false;
				This->BroadcastFailure(Error);
				return;
			}
			if (!This->OnlineHttpClient)
			{
				This->OnlineHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
			}

			FString StartError;
			const FFrontierOnlineStorageTransferCompletion Completion =
				[WeakThis](const FFrontierOnlineStorageTransferResponse& Response)
				{
					if (UFrontierBackendProtocolComponent* InnerThis = WeakThis.Get())
					{
						InnerThis->HandleStorageTransferResponse(Response);
					}
				};
			const bool bStarted = bDeposit
				? This->OnlineHttpClient->DepositStorageItem(
					CurrentAccessToken,
					ItemInstanceIdString,
					TargetSlotIndex,
					Quantity,
					Completion,
					StartError)
				: This->OnlineHttpClient->WithdrawStorageItem(
					CurrentAccessToken,
					ItemInstanceIdString,
					TargetSlotIndex,
					Quantity,
					Completion,
					StartError);
			if (!bStarted)
			{
				This->bRequestInProgress = false;
				This->bInventoryMutationInProgress = false;
				This->BroadcastFailure(StartError);
			}
		});

	return true;
}

bool UFrontierBackendProtocolComponent::RequestMoveStorageItem(
	const FGuid& ItemInstanceId,
	const int32 TargetSlotIndex,
	TOptional<int32> Quantity)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		FRONTIER_LOG(Warning, TEXT("Storage move request ignored on non-authority backend protocol component."));
		return false;
	}
	if (bInventoryMutationInProgress || bRequestInProgress)
	{
		FRONTIER_LOG(Warning, TEXT("Storage move request ignored because another backend request is in progress."));
		return false;
	}
	if (!ItemInstanceId.IsValid() || TargetSlotIndex < 0)
	{
		FRONTIER_LOG(Warning, TEXT("Storage move request ignored because itemInstanceId or targetSlotIndex is invalid."));
		return false;
	}

	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession)
	{
		FRONTIER_LOG(Warning, TEXT("Storage move request ignored because the player session store is unavailable."));
		return false;
	}

	const FString ItemInstanceIdString = ItemInstanceId.ToString(EGuidFormats::DigitsWithHyphensLower);
	bRequestInProgress = true;
	bInventoryMutationInProgress = true;
	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis, ItemInstanceIdString, TargetSlotIndex, Quantity](
			const bool bSucceeded,
			const FString& CurrentAccessToken,
			const FString& Error)
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded)
			{
				This->bRequestInProgress = false;
				This->bInventoryMutationInProgress = false;
				This->BroadcastFailure(Error);
				return;
			}
			if (!This->OnlineHttpClient)
			{
				This->OnlineHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
			}
			FString StartError;
			if (!This->OnlineHttpClient->MoveStorageItem(
				CurrentAccessToken,
				ItemInstanceIdString,
				TargetSlotIndex,
				Quantity,
				[WeakThis](const FFrontierOnlineStorageResponse& Response)
				{
					if (UFrontierBackendProtocolComponent* InnerThis = WeakThis.Get())
					{
						InnerThis->HandleStorageMutationResponse(Response);
					}
				},
				StartError))
			{
				This->bRequestInProgress = false;
				This->bInventoryMutationInProgress = false;
				This->BroadcastFailure(StartError);
			}
		});

	return true;
}

bool UFrontierBackendProtocolComponent::RequestSwapStorageItems(
	const int32 SourceSlotIndex,
	const int32 TargetSlotIndex,
	const FGuid& SourceItemInstanceId,
	const FGuid& TargetItemInstanceId)
{
	if (SourceSlotIndex == TargetSlotIndex
		|| !SourceItemInstanceId.IsValid()
		|| !TargetItemInstanceId.IsValid()
		|| PendingInventorySwap.Kind != EPendingInventorySwapKind::None)
	{
		return false;
	}

	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	AFrontierPlayerState* PlayerState = Controller ? Controller->GetPlayerState<AFrontierPlayerState>() : nullptr;
	UFrontierStorageComponent* Storage = PlayerState ? PlayerState->GetStorageComponent() : nullptr;
	if (!Storage)
	{
		return false;
	}

	int32 TempSlotIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Storage->GetSlots().Num(); ++Index)
	{
		if (!Storage->GetSlots()[Index].bOccupied)
		{
			TempSlotIndex = Index;
			break;
		}
	}
	if (TempSlotIndex == INDEX_NONE)
	{
		return false;
	}

	PendingInventorySwap.Kind = EPendingInventorySwapKind::Storage;
	PendingInventorySwap.SourceSlotIndex = SourceSlotIndex;
	PendingInventorySwap.TargetSlotIndex = TargetSlotIndex;
	PendingInventorySwap.TargetTempSlotIndex = TempSlotIndex;
	PendingInventorySwap.SourceItemInstanceId = SourceItemInstanceId;
	PendingInventorySwap.TargetItemInstanceId = TargetItemInstanceId;
	PendingInventorySwap.Step = 0;
	if (!RequestMoveStorageItem(SourceItemInstanceId, TempSlotIndex))
	{
		ClearPendingInventorySwap();
		return false;
	}
	return true;
}

bool UFrontierBackendProtocolComponent::RequestSwapRaidInventoryWithStorage(
	const int32 SourceRaidSlotIndex,
	const int32 TargetStorageSlotIndex,
	const FGuid& SourceItemInstanceId,
	const FGuid& TargetItemInstanceId)
{
	if (!SourceItemInstanceId.IsValid()
		|| !TargetItemInstanceId.IsValid()
		|| PendingInventorySwap.Kind != EPendingInventorySwapKind::None)
	{
		return false;
	}

	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	AFrontierPlayerState* PlayerState = Controller ? Controller->GetPlayerState<AFrontierPlayerState>() : nullptr;
	UFrontierRaidInventoryComponent* RaidInventory = PlayerState ? PlayerState->GetRaidInventoryComponent() : nullptr;
	UFrontierStorageComponent* Storage = PlayerState ? PlayerState->GetStorageComponent() : nullptr;
	if (!RaidInventory || !Storage)
	{
		return false;
	}

	int32 EmptyStorageSlot = INDEX_NONE;
	for (int32 Index = 0; Index < Storage->GetSlots().Num(); ++Index)
	{
		if (!Storage->GetSlots()[Index].bOccupied)
		{
			EmptyStorageSlot = Index;
			break;
		}
	}
	int32 EmptyRaidSlot = INDEX_NONE;
	for (int32 Index = 0; Index < RaidInventory->GetSlots().Num(); ++Index)
	{
		if (!RaidInventory->GetSlots()[Index].bOccupied)
		{
			EmptyRaidSlot = Index;
			break;
		}
	}
	if (EmptyRaidSlot == INDEX_NONE)
	{
		return false;
	}

	PendingInventorySwap.Kind = EPendingInventorySwapKind::RaidToStorage;
	PendingInventorySwap.SourceSlotIndex = SourceRaidSlotIndex;
	PendingInventorySwap.TargetSlotIndex = TargetStorageSlotIndex;
	PendingInventorySwap.SourceTempSlotIndex = EmptyRaidSlot;
	PendingInventorySwap.TargetTempSlotIndex = EmptyStorageSlot;
	PendingInventorySwap.SourceItemInstanceId = SourceItemInstanceId;
	PendingInventorySwap.TargetItemInstanceId = TargetItemInstanceId;
	PendingInventorySwap.Step = 0;
	if (!RequestWithdrawStorageItem(TargetItemInstanceId, TOptional<int32>(EmptyRaidSlot)))
	{
		ClearPendingInventorySwap();
		return false;
	}
	return true;
}

bool UFrontierBackendProtocolComponent::RequestSwapStorageWithRaidInventory(
	const int32 SourceStorageSlotIndex,
	const int32 TargetRaidSlotIndex,
	const FGuid& SourceItemInstanceId,
	const FGuid& TargetItemInstanceId)
{
	if (!SourceItemInstanceId.IsValid()
		|| !TargetItemInstanceId.IsValid()
		|| PendingInventorySwap.Kind != EPendingInventorySwapKind::None)
	{
		return false;
	}

	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	AFrontierPlayerState* PlayerState = Controller ? Controller->GetPlayerState<AFrontierPlayerState>() : nullptr;
	UFrontierRaidInventoryComponent* RaidInventory = PlayerState ? PlayerState->GetRaidInventoryComponent() : nullptr;
	UFrontierStorageComponent* Storage = PlayerState ? PlayerState->GetStorageComponent() : nullptr;
	if (!RaidInventory || !Storage)
	{
		return false;
	}

	int32 EmptyStorageSlot = INDEX_NONE;
	for (int32 Index = 0; Index < Storage->GetSlots().Num(); ++Index)
	{
		if (!Storage->GetSlots()[Index].bOccupied)
		{
			EmptyStorageSlot = Index;
			break;
		}
	}
	int32 EmptyRaidSlot = INDEX_NONE;
	for (int32 Index = 0; Index < RaidInventory->GetSlots().Num(); ++Index)
	{
		if (!RaidInventory->GetSlots()[Index].bOccupied)
		{
			EmptyRaidSlot = Index;
			break;
		}
	}
	if (EmptyStorageSlot == INDEX_NONE)
	{
		return false;
	}

	PendingInventorySwap.Kind = EPendingInventorySwapKind::StorageToRaid;
	PendingInventorySwap.SourceSlotIndex = SourceStorageSlotIndex;
	PendingInventorySwap.TargetSlotIndex = TargetRaidSlotIndex;
	PendingInventorySwap.SourceTempSlotIndex = EmptyRaidSlot;
	PendingInventorySwap.TargetTempSlotIndex = EmptyStorageSlot;
	PendingInventorySwap.SourceItemInstanceId = SourceItemInstanceId;
	PendingInventorySwap.TargetItemInstanceId = TargetItemInstanceId;
	PendingInventorySwap.Step = 0;
	if (!RequestDepositStorageItem(TargetItemInstanceId, TOptional<int32>(EmptyStorageSlot)))
	{
		ClearPendingInventorySwap();
		return false;
	}
	return true;
}

bool UFrontierBackendProtocolComponent::ContinuePendingInventorySwap()
{
	if (PendingInventorySwap.Kind == EPendingInventorySwapKind::None)
	{
		return true;
	}

	bool bStarted = false;
	switch (PendingInventorySwap.Kind)
	{
	case EPendingInventorySwapKind::RaidInventory:
		switch (PendingInventorySwap.Step++)
		{
		case 0:
			bStarted = RequestMoveInventoryItem(PendingInventorySwap.TargetItemInstanceId, PendingInventorySwap.SourceSlotIndex);
			break;
		case 1:
			bStarted = RequestMoveInventoryItem(PendingInventorySwap.SourceItemInstanceId, PendingInventorySwap.TargetSlotIndex);
			break;
		default:
			ClearPendingInventorySwap();
			return true;
		}
		break;
	case EPendingInventorySwapKind::Storage:
		switch (PendingInventorySwap.Step++)
		{
		case 0:
			bStarted = RequestMoveStorageItem(PendingInventorySwap.TargetItemInstanceId, PendingInventorySwap.SourceSlotIndex);
			break;
		case 1:
			bStarted = RequestMoveStorageItem(PendingInventorySwap.SourceItemInstanceId, PendingInventorySwap.TargetSlotIndex);
			break;
		default:
			ClearPendingInventorySwap();
			return true;
		}
		break;
	case EPendingInventorySwapKind::RaidToStorage:
		switch (PendingInventorySwap.Step++)
		{
		case 0:
			bStarted = RequestDepositStorageItem(PendingInventorySwap.SourceItemInstanceId, TOptional<int32>(PendingInventorySwap.TargetSlotIndex));
			break;
		case 1:
			bStarted = RequestMoveInventoryItem(PendingInventorySwap.TargetItemInstanceId, PendingInventorySwap.SourceSlotIndex);
			break;
		default:
			ClearPendingInventorySwap();
			return true;
		}
		break;
	case EPendingInventorySwapKind::StorageToRaid:
		switch (PendingInventorySwap.Step++)
		{
		case 0:
			bStarted = RequestWithdrawStorageItem(PendingInventorySwap.SourceItemInstanceId, TOptional<int32>(PendingInventorySwap.TargetSlotIndex));
			break;
		case 1:
			bStarted = RequestMoveStorageItem(PendingInventorySwap.TargetItemInstanceId, PendingInventorySwap.SourceSlotIndex);
			break;
		default:
			ClearPendingInventorySwap();
			return true;
		}
		break;
	default:
		ClearPendingInventorySwap();
		return false;
	}

	if (!bStarted)
	{
		ClearPendingInventorySwap();
		BroadcastFailure(TEXT("Inventory swap could not continue after the backend snapshot was applied."));
		return false;
	}
	return true;
}

void UFrontierBackendProtocolComponent::ClearPendingInventorySwap()
{
	PendingInventorySwap.Reset();
}

bool UFrontierBackendProtocolComponent::RequestEquipInventoryItem(
	const FGuid& ItemInstanceId,
	const EFrontierEquipmentSlot SlotType,
	const bool bReplaceExisting,
	TOptional<int32> ReplacementTargetInventorySlot)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		FRONTIER_LOG(Warning, TEXT("Equipment equip request ignored on non-authority backend protocol component."));
		return false;
	}
	if (bEquipmentMutationInProgress || bInventoryMutationInProgress || bRequestInProgress)
	{
		FRONTIER_LOG(Warning, TEXT("Equipment equip request ignored because another backend request is in progress."));
		return false;
	}
	if (!ItemInstanceId.IsValid() || SlotType == EFrontierEquipmentSlot::None)
	{
		FRONTIER_LOG(Warning, TEXT("Equipment equip request ignored because itemInstanceId or slotType is invalid."));
		return false;
	}

	FString BackendSlotType;
	FString ConvertError;
	if (!FFrontierBackendInventoryMapper::ConvertEquipmentSlotToBackend(SlotType, BackendSlotType, ConvertError))
	{
		FRONTIER_LOG(Warning, TEXT("Equipment equip request ignored. Error=%s"), *ConvertError);
		return false;
	}

	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession)
	{
		FRONTIER_LOG(Warning, TEXT("Equipment equip request ignored because the player session store is unavailable."));
		return false;
	}

	const FString ItemInstanceIdString = ItemInstanceId.ToString(EGuidFormats::DigitsWithHyphensLower);
	FRONTIER_LOG(Log, TEXT("Backend equipment equip requested. Operation=EquipmentEquip ItemInstanceId=%s SlotType=%s ReplaceExisting=%d ReplacementTargetInventorySlot=%d"),
		*ItemInstanceIdString,
		*BackendSlotType,
		bReplaceExisting ? 1 : 0,
		ReplacementTargetInventorySlot.IsSet() ? ReplacementTargetInventorySlot.GetValue() : INDEX_NONE);

	bRequestInProgress = true;
	bInventoryMutationInProgress = true;
	bEquipmentMutationInProgress = true;
	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis,
			ItemInstanceIdString,
			BackendSlotType,
			bReplaceExisting,
			ReplacementTargetInventorySlot](
			const bool bSucceeded,
			const FString& CurrentAccessToken,
			const FString& Error)
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded)
			{
				This->bInventoryMutationInProgress = false;
				This->bEquipmentMutationInProgress = false;
				This->BroadcastFailure(Error);
				return;
			}
			if (!This->OnlineHttpClient)
			{
				This->OnlineHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
			}
			FString StartError;
			if (!This->OnlineHttpClient->EquipItem(
				CurrentAccessToken,
				ItemInstanceIdString,
				BackendSlotType,
				bReplaceExisting,
				ReplacementTargetInventorySlot,
				[WeakThis](const FFrontierOnlineEquipmentChangeResponse& Response)
				{
					if (UFrontierBackendProtocolComponent* InnerThis = WeakThis.Get())
					{
						InnerThis->HandleEquipmentMutationResponse(Response);
					}
				},
				StartError))
			{
				This->bInventoryMutationInProgress = false;
				This->bEquipmentMutationInProgress = false;
				This->BroadcastFailure(StartError);
			}
		});

	return true;
}

bool UFrontierBackendProtocolComponent::RequestUnequipItem(
	const EFrontierEquipmentSlot SlotType,
	const FGuid& ItemInstanceId,
	TOptional<int32> TargetInventorySlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		FRONTIER_LOG(Warning, TEXT("Equipment unequip request ignored on non-authority backend protocol component."));
		return false;
	}
	if (bEquipmentMutationInProgress || bInventoryMutationInProgress || bRequestInProgress)
	{
		FRONTIER_LOG(Warning, TEXT("Equipment unequip request ignored because another backend request is in progress."));
		return false;
	}
	if (SlotType == EFrontierEquipmentSlot::None || !ItemInstanceId.IsValid())
	{
		FRONTIER_LOG(Warning, TEXT("Equipment unequip request ignored because slotType or itemInstanceId is invalid."));
		return false;
	}

	FString BackendSlotType;
	FString ConvertError;
	if (!FFrontierBackendInventoryMapper::ConvertEquipmentSlotToBackend(SlotType, BackendSlotType, ConvertError))
	{
		FRONTIER_LOG(Warning, TEXT("Equipment unequip request ignored. Error=%s"), *ConvertError);
		return false;
	}

	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession)
	{
		FRONTIER_LOG(Warning, TEXT("Equipment unequip request ignored because the player session store is unavailable."));
		return false;
	}

	FRONTIER_LOG(Log, TEXT("Backend equipment unequip requested. Operation=EquipmentUnequip SlotType=%s TargetInventorySlotIndex=%d"),
		*BackendSlotType,
		TargetInventorySlotIndex.IsSet() ? TargetInventorySlotIndex.GetValue() : INDEX_NONE);

	bRequestInProgress = true;
	bInventoryMutationInProgress = true;
	bEquipmentMutationInProgress = true;
	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis,
			BackendSlotType,
			TargetInventorySlotIndex](
			const bool bSucceeded,
			const FString& CurrentAccessToken,
			const FString& Error)
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded)
			{
				This->bInventoryMutationInProgress = false;
				This->bEquipmentMutationInProgress = false;
				This->BroadcastFailure(Error);
				return;
			}
			if (!This->OnlineHttpClient)
			{
				This->OnlineHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
			}
			FString StartError;
			if (!This->OnlineHttpClient->UnequipItem(
				CurrentAccessToken,
				BackendSlotType,
				TargetInventorySlotIndex,
				[WeakThis](const FFrontierOnlineEquipmentChangeResponse& Response)
				{
					if (UFrontierBackendProtocolComponent* InnerThis = WeakThis.Get())
					{
						InnerThis->HandleEquipmentMutationResponse(Response);
					}
				},
				StartError))
			{
				This->bInventoryMutationInProgress = false;
				This->bEquipmentMutationInProgress = false;
				This->BroadcastFailure(StartError);
			}
		});

	return true;
}

void UFrontierBackendProtocolComponent::HandleSteamAuthResponse(const FFrontierOnlineSteamLoginResponse& Response)
{
	bRequestInProgress = false;
	if (!Response.bTransportSucceeded || !Response.bSuccess)
	{
		BroadcastFailure(Response.Message.IsEmpty() ? TEXT("Backend Steam login failed.") : Response.Message);
		return;
	}

	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession)
	{
		BroadcastFailure(TEXT("Player session store is unavailable after Steam login."));
		return;
	}
	PlayerSession->SetAuthenticatedSession(
		Response.Session,
		Response.Tokens,
		Response.Player.PlayerId,
		Response.Player.SteamId,
		Response.Player.Nickname);

	LastSteamLoginResult = BuildSanitizedLoginResult(Response);
	FRONTIER_LOG(
		Log,
		TEXT("Backend Steam auth succeeded. PlayerId=%lld SessionId=%s Nickname=%s IsNewPlayer=%d"),
		LastSteamLoginResult.Player.PlayerId,
		*LastSteamLoginResult.Session.SessionId,
		*LastSteamLoginResult.Player.Nickname,
		LastSteamLoginResult.bIsNewPlayer ? 1 : 0);

	RequestProfileAfterLogin(LastSteamLoginResult);
}

void UFrontierBackendProtocolComponent::RequestProfileAfterLogin(
	FFrontierBackendSteamLoginResult LoginResult,
	const bool bAfterAccessTokenRefresh)
{
	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession)
	{
		BroadcastFailure(TEXT("Player session store is unavailable."));
		return;
	}

	bRequestInProgress = true;
	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis, LoginResult = MoveTemp(LoginResult), bAfterAccessTokenRefresh](
			const bool bSucceeded,
			const FString& CurrentAccessToken,
			const FString& Error) mutable
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded)
			{
				This->BroadcastFailure(Error);
				return;
			}
			if (!This->OnlineHttpClient)
			{
				This->OnlineHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
			}

			FString StartError;
			if (!This->OnlineHttpClient->GetMyProfile(
				CurrentAccessToken,
				[WeakThis, LoginResult, bAfterAccessTokenRefresh](const FFrontierOnlineProfileResponse& Response)
				{
					if (UFrontierBackendProtocolComponent* InnerThis = WeakThis.Get())
					{
						InnerThis->HandleProfileAfterLogin(Response, LoginResult, bAfterAccessTokenRefresh);
					}
				},
				StartError))
			{
				This->bRequestInProgress = false;
				FRONTIER_LOG(Warning, TEXT("Profile query could not be started. Error=%s"), *StartError);
				This->RequestLobbyBootstrapAfterLogin(MoveTemp(LoginResult), bAfterAccessTokenRefresh);
			}
		});
}

void UFrontierBackendProtocolComponent::HandleProfileAfterLogin(
	const FFrontierOnlineProfileResponse& Response,
	FFrontierBackendSteamLoginResult LoginResult,
	const bool bAfterAccessTokenRefresh)
{
	bRequestInProgress = false;
	if (Response.bTransportSucceeded && Response.bSuccess)
	{
		if (!Response.DisplayName.IsEmpty())
		{
			LoginResult.Player.Nickname = Response.DisplayName;
			if (UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
				? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
				: nullptr)
			{
				PlayerSession->SetNickname(Response.DisplayName);
			}
			ApplyProfileDisplayNameToOwner(this, Response.DisplayName);
		}
		if (!Response.Locale.IsEmpty())
		{
			LoginResult.Player.Locale = Response.Locale;
			if (UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
				? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
				: nullptr)
			{
				PlayerSession->SetLocale(Response.Locale);
			}
		}
	}
	else
	{
		FRONTIER_LOG(Warning, TEXT("Profile query failed after login. HttpStatus=%d ErrorCode=%s Message=%s; continuing lobby bootstrap."),
			Response.HttpStatus,
			*Response.ErrorCode,
			*Response.Message);
	}

	RequestLobbyBootstrapAfterLogin(MoveTemp(LoginResult), bAfterAccessTokenRefresh);
}

void UFrontierBackendProtocolComponent::RequestLobbyBootstrapAfterLogin(
	FFrontierBackendSteamLoginResult LoginResult,
	const bool bAfterAccessTokenRefresh)
{
	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession)
	{
		BroadcastFailure(TEXT("Player session store is unavailable."));
		return;
	}

	bRequestInProgress = true;
	FRONTIER_LOG(Log, TEXT("Lobby bootstrap request started. PlayerId=%lld"),
		LoginResult.Player.PlayerId);
	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis, LoginResult = MoveTemp(LoginResult), bAfterAccessTokenRefresh](
			const bool bSucceeded,
			const FString& CurrentAccessToken,
			const FString& Error) mutable
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded)
			{
				This->BroadcastFailure(Error);
				return;
			}
			if (!This->OnlineHttpClient)
			{
				This->OnlineHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
			}
			FString StartError;
			if (!This->OnlineHttpClient->GetLobbyBootstrap(
				CurrentAccessToken,
				[WeakThis, LoginResult, bAfterAccessTokenRefresh](
					const bool bTransportSucceeded,
					const int32 HttpStatus,
					FString ResponseBody)
				{
					UFrontierBackendProtocolComponent* InnerThis = WeakThis.Get();
					if (!InnerThis)
					{
						return;
					}
					FFrontierOnlineLobbyBootstrapResponse ParsedResponse;
					if (!bTransportSucceeded)
					{
						ParsedResponse.Message = TEXT("Lobby bootstrap backend request failed before receiving a response.");
					}
					else
					{
						FString ParseError;
						if (!FrontierLobbyBootstrapJson::Parse(
							HttpStatus,
							ResponseBody,
							ParsedResponse,
							ParseError))
						{
							ParsedResponse.bSuccess = false;
							ParsedResponse.Message = MoveTemp(ParseError);
							FRONTIER_LOG(
								Warning,
								TEXT("Lobby bootstrap parse failed. Parser=FrontierLocal HttpStatus=%d ResponseBodyLength=%d Error=%s"),
								HttpStatus,
								ResponseBody.Len(),
								*ParsedResponse.Message);
						}
					}
					InnerThis->HandleLobbyBootstrapResponse(
						ParsedResponse,
						LoginResult,
						bAfterAccessTokenRefresh);
				},
				StartError))
			{
				This->BroadcastFailure(StartError);
			}
		},
		bAfterAccessTokenRefresh);
}

void UFrontierBackendProtocolComponent::HandleLobbyBootstrapResponse(
	const FFrontierOnlineLobbyBootstrapResponse& Response,
	FFrontierBackendSteamLoginResult LoginResult,
	const bool bAfterAccessTokenRefresh)
{
	bRequestInProgress = false;
	if (!Response.bSuccess
		&& Response.HttpStatus == 401
		&& Response.ErrorCode.Equals(TEXT("ACCESS_TOKEN_EXPIRED"), ESearchCase::CaseSensitive)
		&& !bAfterAccessTokenRefresh)
	{
		RequestLobbyBootstrapAfterLogin(MoveTemp(LoginResult), true);
		return;
	}
	if (!Response.bTransportSucceeded || !Response.bSuccess)
	{
		BroadcastFailure(Response.Message.IsEmpty() ? TEXT("Lobby bootstrap request failed.") : Response.Message);
		return;
	}

	FRONTIER_LOG(
		Log,
		TEXT("Lobby bootstrap parsed. Parser=FrontierLocal PlayerId=%s InventoryCapacity=%d StorageCapacity=%d InventoryItems=%d StorageItems=%d EquipmentSlots=%d"),
		*Response.Data.Player.PlayerIdString,
		Response.Data.Inventory.Container.SlotCapacity,
		Response.Data.Storage.Container.SlotCapacity,
		Response.Data.Inventory.Slots.Num(),
		Response.Data.Storage.Slots.Num(),
		Response.Data.Equipment.Slots.Num());

	FString ApplyError;
	if (!ApplyLobbyBootstrapToPlayerState(Response, ApplyError))
	{
		BroadcastFailure(ApplyError);
		return;
	}

	// The profile GET is authoritative for the display name. Bootstrap nickname is only
	// a compatibility fallback when the profile response did not provide one.
	if (LoginResult.Player.Nickname.IsEmpty() && !Response.Data.Player.Nickname.IsEmpty())
	{
		LoginResult.Player.Nickname = Response.Data.Player.Nickname;
		if (UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
			? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
			: nullptr)
		{
			PlayerSession->SetNickname(Response.Data.Player.Nickname);
		}
		ApplyProfileDisplayNameToOwner(this, Response.Data.Player.Nickname);
	}
	if (!Response.Data.Player.SteamId.IsEmpty())
	{
		LoginResult.Player.SteamId = Response.Data.Player.SteamId;
	}
	if (Response.Data.Player.PlayerId > 0)
	{
		LoginResult.Player.PlayerId = Response.Data.Player.PlayerId;
	}
	if (!Response.Data.Player.PlayerIdString.IsEmpty())
	{
		LoginResult.Player.PlayerIdString = Response.Data.Player.PlayerIdString;
	}

	APlayerController* OwnerController = Cast<APlayerController>(GetOwner());
	if (AFrontierPlayerState* PlayerState = OwnerController ? OwnerController->GetPlayerState<AFrontierPlayerState>() : nullptr)
	{
		PlayerState->SetBackendIdentity(LoginResult.Player.PlayerIdString, LoginResult.Player.SteamId);
		ApplyProfileDisplayNameToOwner(this, LoginResult.Player.Nickname);
	}

	RequestPlayerLevelAfterBootstrap(MoveTemp(LoginResult));
}

void UFrontierBackendProtocolComponent::RequestPlayerLevelAfterBootstrap(
	FFrontierBackendSteamLoginResult LoginResult,
	const bool bAfterAccessTokenRefresh)
{
	UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession)
	{
		BroadcastFailure(TEXT("Player session store is unavailable."));
		return;
	}

	bRequestInProgress = true;
	FRONTIER_LOG(Log, TEXT("Player level request started. PlayerId=%s"), *LoginResult.Player.PlayerIdString);
	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis, LoginResult = MoveTemp(LoginResult), bAfterAccessTokenRefresh](
			const bool bSucceeded,
			const FString& CurrentAccessToken,
			const FString& Error) mutable
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded)
			{
				This->BroadcastFailure(Error);
				return;
			}
			if (!This->OnlineHttpClient)
			{
				This->OnlineHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
			}
			FString StartError;
			if (!This->OnlineHttpClient->GetPlayerLevel(
				CurrentAccessToken,
				[WeakThis, LoginResult, bAfterAccessTokenRefresh](
					const FFrontierOnlinePlayerLevelResponse& Response)
				{
					if (UFrontierBackendProtocolComponent* InnerThis = WeakThis.Get())
					{
						InnerThis->HandlePlayerLevelResponse(
							Response,
							LoginResult,
							bAfterAccessTokenRefresh);
					}
				},
				StartError))
			{
				This->BroadcastFailure(StartError);
			}
		},
		bAfterAccessTokenRefresh);
}

void UFrontierBackendProtocolComponent::HandlePlayerLevelResponse(
	const FFrontierOnlinePlayerLevelResponse& Response,
	FFrontierBackendSteamLoginResult LoginResult,
	const bool bAfterAccessTokenRefresh)
{
	bRequestInProgress = false;
	if (!Response.bSuccess
		&& Response.HttpStatus == 401
		&& Response.ErrorCode.Equals(TEXT("ACCESS_TOKEN_EXPIRED"), ESearchCase::CaseSensitive)
		&& !bAfterAccessTokenRefresh)
	{
		RequestPlayerLevelAfterBootstrap(MoveTemp(LoginResult), true);
		return;
	}
	if (!Response.bTransportSucceeded || !Response.bSuccess)
	{
		// A failed GET never mutates the local level watermark or local skill points.
		BroadcastFailure(Response.Message.IsEmpty() ? TEXT("Player level request failed.") : Response.Message);
		return;
	}

	const FFrontierPlayerLevelSnapshot PlayerLevel = BuildPlayerLevelSnapshot(Response.Data);
	const bool bSameAccount = ResolveLoginAccountId(LastSteamLoginResult).Equals(
		ResolveLoginAccountId(LoginResult),
		ESearchCase::CaseSensitive);
	if (bSameAccount && IsOlderPlayerLevelSnapshot(PlayerLevel, LastPlayerLevel))
	{
		FRONTIER_LOG(Warning, TEXT("Ignored an out-of-order older player level snapshot. Incoming=%s Accepted=%s"), *PlayerLevel.UpdatedAt, *LastPlayerLevel.UpdatedAt);
		ClientReceiveLobbyProgressionReady(LoginResult, LastPlayerLevel);
		return;
	}
	LastSteamLoginResult = LoginResult;
	LastPlayerLevel = PlayerLevel;
	FRONTIER_LOG(
		Log,
		TEXT("Player level request succeeded. PlayerId=%s Level=%d CurrentLevelExperience=%lld NextLevelRequiredExperience=%lld HasNextLevel=%d"),
		*LoginResult.Player.PlayerIdString,
		LastPlayerLevel.Level,
		LastPlayerLevel.CurrentLevelExperience,
		LastPlayerLevel.NextLevelRequiredExperience,
		LastPlayerLevel.bHasNextLevelRequiredExperience ? 1 : 0);
	ClientReceiveLobbyProgressionReady(LoginResult, LastPlayerLevel);
}

bool UFrontierBackendProtocolComponent::ApplyInventoryResponseToPlayerState(
	const FFrontierOnlineInventoryResponse& Response,
	FString& OutError)
{
	return ApplyInventoryDataToPlayerState(Response.Data, OutError);
}

bool UFrontierBackendProtocolComponent::ApplyInventoryDataToPlayerState(
	const FFrontierOnlineInventoryData& InventoryData,
	FString& OutError)
{
	OutError.Reset();
	APlayerController* OwnerController = Cast<APlayerController>(GetOwner());
	AFrontierPlayerState* PlayerState = OwnerController ? OwnerController->GetPlayerState<AFrontierPlayerState>() : nullptr;
	if (!PlayerState || !PlayerState->HasAuthority())
	{
		OutError = TEXT("Cannot apply backend inventory because authoritative PlayerState is unavailable.");
		return false;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFrontierItemCatalogSubsystem* ItemCatalog = GameInstance
		? GameInstance->GetSubsystem<UFrontierItemCatalogSubsystem>()
		: nullptr;
	if (!ItemCatalog)
	{
		OutError = TEXT("Item catalog subsystem is unavailable.");
		return false;
	}

	TArray<FFrontierInventorySlot> RuntimeSlots;
	if (!FFrontierBackendInventoryMapper::BuildRuntimeSlotsFromBackendInventory(InventoryData, *ItemCatalog, RuntimeSlots, OutError))
	{
		return false;
	}

	for (const FFrontierOnlineItemDTO& BackendItem : InventoryData.Slots)
	{
		FRONTIER_LOG(
			Log,
			TEXT("[LobbyInventoryItem] Stage=BackendResponse SlotIndex=%d ItemInstanceId=%s ItemTemplateId=%s OptionCount=%d SkillCount=%d"),
			BackendItem.SlotIndex,
			*BackendItem.ItemInstanceId,
			*BackendItem.ItemTemplateId,
			BackendItem.RandomOptions.Num(),
			BackendItem.GeneratedSkills.Num());
	}
	for (const FFrontierInventorySlot& RuntimeSlot : RuntimeSlots)
	{
		if (!RuntimeSlot.bOccupied)
		{
			continue;
		}
		FRONTIER_LOG(
			Log,
			TEXT("[LobbyInventoryItem] Stage=RuntimeMapped SlotIndex=%d ItemInstanceId=%s ItemTemplateId=%s OptionCount=%d SkillCount=%d"),
			RuntimeSlot.SlotIndex,
			*RuntimeSlot.ItemInstance.ItemInstanceId.ToString(EGuidFormats::DigitsWithHyphensLower),
			*RuntimeSlot.ItemInstance.GetTemplateId().ToString(),
			RuntimeSlot.ItemInstance.RuntimeGeneratedStats.Num(),
			RuntimeSlot.ItemInstance.RuntimeGeneratedSkills.Num());
	}

	UFrontierRaidInventoryComponent* RaidInventory = PlayerState->GetRaidInventoryComponent();
	if (!RaidInventory)
	{
		OutError = TEXT("Raid inventory component is unavailable.");
		return false;
	}

	if (!RaidInventory->SetAuthoritativeSlotCapacity(InventoryData.Container.SlotCapacity)
		|| !RaidInventory->ApplyAuthoritativeInventoryState(RuntimeSlots))
	{
		OutError = TEXT("Failed to apply authoritative backend inventory slots.");
		return false;
	}

	FRONTIER_LOG(Log, TEXT("Initial backend inventory applied. InventoryId=%s SlotCapacity=%d OccupiedSlotCount=%d"),
		*InventoryData.Container.InventoryId,
		InventoryData.Container.SlotCapacity,
		InventoryData.Slots.Num());
	return true;
}

bool UFrontierBackendProtocolComponent::ApplyStorageResponseToPlayerState(
	const FFrontierOnlineStorageResponse& Response,
	FString& OutError)
{
	return ApplyStorageDataToPlayerState(Response.Data, OutError);
}

bool UFrontierBackendProtocolComponent::ApplyStorageDataToPlayerState(
	const FFrontierOnlineStorageData& StorageData,
	FString& OutError)
{
	OutError.Reset();
	APlayerController* OwnerController = Cast<APlayerController>(GetOwner());
	AFrontierPlayerState* PlayerState = OwnerController ? OwnerController->GetPlayerState<AFrontierPlayerState>() : nullptr;
	if (!PlayerState || !PlayerState->HasAuthority())
	{
		OutError = TEXT("Cannot apply backend storage because authoritative PlayerState is unavailable.");
		return false;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFrontierItemCatalogSubsystem* ItemCatalog = GameInstance
		? GameInstance->GetSubsystem<UFrontierItemCatalogSubsystem>()
		: nullptr;
	if (!ItemCatalog)
	{
		OutError = TEXT("Item catalog subsystem is unavailable.");
		return false;
	}

	UFrontierStorageComponent* Storage = PlayerState->GetStorageComponent();
	if (!Storage)
	{
		OutError = TEXT("Lobby storage inventory component is unavailable.");
		return false;
	}

	TArray<FFrontierInventorySlot> RuntimeSlots;
	if (!FFrontierBackendInventoryMapper::BuildRuntimeSlotsFromBackendStorage(StorageData, *ItemCatalog, RuntimeSlots, OutError))
	{
		return false;
	}

	if (!Storage->SetAuthoritativeSlotCapacity(StorageData.Container.SlotCapacity)
		|| !Storage->ApplyAuthoritativeInventoryState(RuntimeSlots))
	{
		OutError = TEXT("Failed to apply authoritative backend storage slots.");
		return false;
	}

	FRONTIER_LOG(Log, TEXT("Initial backend storage applied. StorageId=%s SlotCapacity=%d OccupiedSlotCount=%d"),
		*StorageData.Container.StorageId,
		StorageData.Container.SlotCapacity,
		StorageData.Slots.Num());
	return true;
}

bool UFrontierBackendProtocolComponent::ApplyEquipmentResponseToPlayerState(
	const FFrontierOnlineEquipmentResponse& Response,
	FString& OutError)
{
	return ApplyEquipmentDataToPlayerState(Response.Data, OutError);
}

bool UFrontierBackendProtocolComponent::ApplyEquipmentDataToPlayerState(
	const FFrontierOnlineEquipmentData& EquipmentData,
	FString& OutError)
{
	OutError.Reset();

	APlayerController* OwnerController = Cast<APlayerController>(GetOwner());
	AFrontierPlayerState* PlayerState = OwnerController ? OwnerController->GetPlayerState<AFrontierPlayerState>() : nullptr;
	if (!PlayerState || !PlayerState->HasAuthority())
	{
		OutError = TEXT("Cannot apply backend equipment because authoritative PlayerState is unavailable.");
		return false;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFrontierItemCatalogSubsystem* ItemCatalog = GameInstance
		? GameInstance->GetSubsystem<UFrontierItemCatalogSubsystem>()
		: nullptr;
	if (!ItemCatalog)
	{
		OutError = TEXT("Item catalog subsystem is unavailable.");
		return false;
	}

	TArray<FFrontierLoadoutSlot> RuntimeSlots;
	if (!FFrontierBackendInventoryMapper::BuildRuntimeLoadoutFromBackendEquipment(EquipmentData, *ItemCatalog, RuntimeSlots, OutError))
	{
		return false;
	}

	for (const FFrontierOnlineEquipmentSlot& BackendSlot : EquipmentData.Slots)
	{
		if (!BackendSlot.bHasItem)
		{
			continue;
		}
		FRONTIER_LOG(
			Log,
			TEXT("[LobbyEquipmentItem] Stage=BackendResponse SlotType=%s ItemInstanceId=%s ItemTemplateId=%s OptionCount=%d SkillCount=%d"),
			*BackendSlot.SlotType,
			*BackendSlot.Item.ItemInstanceId,
			*BackendSlot.Item.ItemTemplateId,
			BackendSlot.Item.RandomOptions.Num(),
			BackendSlot.Item.GeneratedSkills.Num());
	}
	for (const FFrontierLoadoutSlot& RuntimeSlot : RuntimeSlots)
	{
		if (!RuntimeSlot.bOccupied)
		{
			continue;
		}
		FRONTIER_LOG(
			Log,
			TEXT("[LobbyEquipmentItem] Stage=RuntimeMapped SlotType=%d ItemInstanceId=%s ItemTemplateId=%s OptionCount=%d SkillCount=%d"),
			static_cast<int32>(RuntimeSlot.SlotType),
			*RuntimeSlot.ItemInstance.ItemInstanceId.ToString(EGuidFormats::DigitsWithHyphensLower),
			*RuntimeSlot.ItemInstance.GetTemplateId().ToString(),
			RuntimeSlot.ItemInstance.RuntimeGeneratedStats.Num(),
			RuntimeSlot.ItemInstance.RuntimeGeneratedSkills.Num());
	}

	UFrontierLoadoutComponent* Loadout = PlayerState->GetLoadoutComponent();
	if (!Loadout)
	{
		OutError = TEXT("Loadout component is unavailable.");
		return false;
	}

	Loadout->SetLoadoutSlotsFromSnapshot(RuntimeSlots);

	FRONTIER_LOG(Log, TEXT("Initial backend equipment applied. EquipmentId=%s SlotCount=%d"),
		*EquipmentData.Container.EquipmentId,
		EquipmentData.Slots.Num());
	return true;
}

bool UFrontierBackendProtocolComponent::ApplyLobbyBootstrapToPlayerState(
	const FFrontierOnlineLobbyBootstrapResponse& Response,
	FString& OutError)
{
	OutError.Reset();

	FString ApplyError;
	if (!ApplyInventoryDataToPlayerState(Response.Data.Inventory, ApplyError))
	{
		if (!IsTemporarilySkippableMissingItemDataAssetError(ApplyError))
		{
			OutError = MoveTemp(ApplyError);
			return false;
		}
		FRONTIER_LOG(
			Warning,
			TEXT("[TEMPORARY] Lobby bootstrap inventory apply skipped because an Item DataAsset is missing. Existing inventory state was left unchanged. Error=%s"),
			*ApplyError);
	}

	ApplyError.Reset();
	if (!ApplyStorageDataToPlayerState(Response.Data.Storage, ApplyError))
	{
		if (!IsTemporarilySkippableMissingItemDataAssetError(ApplyError))
		{
			OutError = MoveTemp(ApplyError);
			return false;
		}
		FRONTIER_LOG(
			Warning,
			TEXT("[TEMPORARY] Lobby bootstrap storage apply skipped because an Item DataAsset is missing. Existing storage state was left unchanged. Error=%s"),
			*ApplyError);
	}

	ApplyError.Reset();
	if (!ApplyEquipmentDataToPlayerState(Response.Data.Equipment, ApplyError))
	{
		if (!IsTemporarilySkippableMissingItemDataAssetError(ApplyError))
		{
			OutError = MoveTemp(ApplyError);
			return false;
		}
		FRONTIER_LOG(
			Warning,
			TEXT("[TEMPORARY] Lobby bootstrap equipment apply skipped because an Item DataAsset is missing. Existing equipment state was left unchanged. Error=%s"),
			*ApplyError);
	}
	LastCurrencies.Reset(Response.Data.Currencies.Num());
	for (const FFrontierOnlineCurrencyDTO& BackendCurrency : Response.Data.Currencies)
	{
		FFrontierBackendCurrencyInfo& CurrencyInfo = LastCurrencies.AddDefaulted_GetRef();
		CurrencyInfo.CurrencyCode = BackendCurrency.CurrencyCode;
		CurrencyInfo.Balance = BackendCurrency.Balance;
	}

	if (Response.Data.bHasActiveRaid)
	{
		EnterActiveRaidSession(Response.Data.ActiveRaid);
	}

	FRONTIER_LOG(Log, TEXT("Lobby bootstrap applied. PlayerId=%lld CurrencyCount=%d HasActiveRaid=%d"),
		Response.Data.Player.PlayerId,
		LastCurrencies.Num(),
		Response.Data.bHasActiveRaid ? 1 : 0);
	return true;
}

void UFrontierBackendProtocolComponent::EnterActiveRaidSession(const FFrontierOnlineRaidSessionDTO& ActiveRaid)
{
	FRONTIER_LOG(Log, TEXT("Active raid session received from bootstrap. RaidSessionId=%s State=%s ServerId=%s"),
		*ActiveRaid.RaidSessionId,
		*ActiveRaid.State,
		ActiveRaid.bHasServerId ? *ActiveRaid.ServerId : TEXT(""));

	// TODO: When active raids are enabled, route the client into this raid session/server here.
}

void UFrontierBackendProtocolComponent::HandleInventoryMutationResponse(const FFrontierOnlineInventoryResponse& Response)
{
	bRequestInProgress = false;
	bInventoryMutationInProgress = false;

	if (!Response.bTransportSucceeded || !Response.bSuccess)
	{
		ClearPendingInventorySwap();
		BroadcastFailure(Response.Message.IsEmpty() ? TEXT("Backend inventory mutation failed.") : Response.Message);
		return;
	}

	FString ApplyError;
	if (!ApplyInventoryResponseToPlayerState(Response, ApplyError))
	{
		ClearPendingInventorySwap();
		BroadcastFailure(ApplyError);
		return;
	}

	if (PendingInventorySwap.Kind == EPendingInventorySwapKind::RaidInventory
		|| PendingInventorySwap.Kind == EPendingInventorySwapKind::StorageToRaid
		|| PendingInventorySwap.Kind == EPendingInventorySwapKind::RaidToStorage)
	{
		ContinuePendingInventorySwap();
		return;
	}

	FRONTIER_LOG(Log, TEXT("Backend inventory mutation applied. ItemCount=%d"), Response.Data.Slots.Num());
}

void UFrontierBackendProtocolComponent::HandleStorageTransferResponse(
	const FFrontierOnlineStorageTransferResponse& Response)
{
	bRequestInProgress = false;
	bInventoryMutationInProgress = false;

	if (!Response.bTransportSucceeded || !Response.bSuccess)
	{
		ClearPendingInventorySwap();
		BroadcastFailure(Response.Message.IsEmpty() ? TEXT("Backend storage transfer failed.") : Response.Message);
		return;
	}

	FString ApplyError;
	if (!ApplyInventoryDataToPlayerState(Response.Data.Inventory, ApplyError)
		|| !ApplyStorageDataToPlayerState(Response.Data.Storage, ApplyError))
	{
		ClearPendingInventorySwap();
		BroadcastFailure(ApplyError);
		return;
	}

	if (PendingInventorySwap.Kind == EPendingInventorySwapKind::RaidToStorage
		|| PendingInventorySwap.Kind == EPendingInventorySwapKind::StorageToRaid)
	{
		ContinuePendingInventorySwap();
		return;
	}

	FRONTIER_LOG(
		Log,
		TEXT("Backend storage transfer applied. InventoryItemCount=%d StorageItemCount=%d"),
		Response.Data.Inventory.Slots.Num(),
		Response.Data.Storage.Slots.Num());
}

void UFrontierBackendProtocolComponent::HandleStorageMutationResponse(
	const FFrontierOnlineStorageResponse& Response)
{
	bRequestInProgress = false;
	bInventoryMutationInProgress = false;

	if (!Response.bTransportSucceeded || !Response.bSuccess)
	{
		ClearPendingInventorySwap();
		BroadcastFailure(Response.Message.IsEmpty() ? TEXT("Backend storage mutation failed.") : Response.Message);
		return;
	}

	FString ApplyError;
	if (!ApplyStorageResponseToPlayerState(Response, ApplyError))
	{
		ClearPendingInventorySwap();
		BroadcastFailure(ApplyError);
		return;
	}

	if (PendingInventorySwap.Kind == EPendingInventorySwapKind::Storage
		|| PendingInventorySwap.Kind == EPendingInventorySwapKind::StorageToRaid)
	{
		ContinuePendingInventorySwap();
		return;
	}

	FRONTIER_LOG(Log, TEXT("Backend storage mutation applied. ItemCount=%d"), Response.Data.Slots.Num());
}

void UFrontierBackendProtocolComponent::HandleEquipmentMutationResponse(const FFrontierOnlineEquipmentChangeResponse& Response)
{
	bRequestInProgress = false;
	bInventoryMutationInProgress = false;
	bEquipmentMutationInProgress = false;

	if (!Response.bTransportSucceeded || !Response.bSuccess)
	{
		BroadcastFailure(Response.Message.IsEmpty() ? TEXT("Backend equipment mutation failed.") : Response.Message);
		return;
	}

	FString ApplyError;
	if (!ApplyInventoryDataToPlayerState(Response.Inventory, ApplyError))
	{
		BroadcastFailure(ApplyError);
		return;
	}
	if (!ApplyEquipmentDataToPlayerState(Response.Equipment, ApplyError))
	{
		BroadcastFailure(ApplyError);
		return;
	}

	FRONTIER_LOG(Log, TEXT("Backend equipment mutation applied. ItemCount=%d"), Response.Inventory.Slots.Num());
}

void UFrontierBackendProtocolComponent::HandleItemUpgradeResponse(const FFrontierOnlineItemUpgradeResponse& Response)
{
	bRequestInProgress = false;
	bInventoryMutationInProgress = false;

	FFrontierItemUpgradeResult Result;
	Result.bRetryable = Response.bRetryable;
	Result.bUpgradeSucceeded = Response.bUpgradeSucceeded;
	Result.PreviousEnhancementLevel = Response.PreviousEnhancementLevel;
	Result.CurrentEnhancementLevel = Response.CurrentEnhancementLevel;
	Result.FailureReason = Response.FailureReason;
	Result.ErrorCode = Response.ErrorCode;
	Result.Message = Response.Message;

	if (!Response.bTransportSucceeded || !Response.bSuccess)
	{
		if (Result.Message.IsEmpty())
		{
			Result.Message = TEXT("The backend did not complete the item upgrade request.");
		}
		BroadcastItemUpgradeResult(Result);
		return;
	}

	FString ApplyError;
	if (!ApplyItemUpgradeResponseToPlayerState(Response, Result.Item, ApplyError))
	{
		Result.Message = ApplyError;
		BroadcastItemUpgradeResult(Result);
		return;
	}

	for (const FFrontierOnlineUpgradeCurrencyChange& Change : Response.CurrencyChanges)
	{
		FFrontierBackendCurrencyInfo* Currency = LastCurrencies.FindByPredicate(
			[&Change](const FFrontierBackendCurrencyInfo& Existing)
			{
				return Existing.CurrencyCode.Equals(Change.CurrencyCode, ESearchCase::IgnoreCase);
			});
		if (!Currency)
		{
			Currency = &LastCurrencies.AddDefaulted_GetRef();
			Currency->CurrencyCode = Change.CurrencyCode;
		}
		Currency->Balance = Change.bHasBalance
			? FMath::Max<int64>(0, Change.Balance)
			: FMath::Max<int64>(0, Currency->Balance - Change.ConsumedAmount);
	}

	Result.bRequestSucceeded = true;
	Result.Currencies = LastCurrencies;
	if (Result.Message.IsEmpty())
	{
		Result.Message = Result.bUpgradeSucceeded
			? TEXT("Item upgrade succeeded.")
			: TEXT("Item upgrade attempt failed.");
	}
	BroadcastItemUpgradeResult(Result);
}

bool UFrontierBackendProtocolComponent::ApplyItemUpgradeResponseToPlayerState(
	const FFrontierOnlineItemUpgradeResponse& Response,
	FFrontierItemInstance& OutUpdatedItem,
	FString& OutError)
{
	OutUpdatedItem = FFrontierItemInstance();
	OutError.Reset();
	APlayerController* OwnerController = Cast<APlayerController>(GetOwner());
	AFrontierPlayerState* PlayerState = OwnerController ? OwnerController->GetPlayerState<AFrontierPlayerState>() : nullptr;
	if (!PlayerState || !PlayerState->HasAuthority())
	{
		OutError = TEXT("Cannot apply item upgrade because the authoritative PlayerState is unavailable.");
		return false;
	}

	UFrontierRaidInventoryComponent* Inventory = PlayerState->GetRaidInventoryComponent();
	UFrontierStorageComponent* Storage = PlayerState->GetStorageComponent();
	UFrontierLoadoutComponent* Loadout = PlayerState->GetLoadoutComponent();
	if (!Inventory || !Storage || !Loadout)
	{
		OutError = TEXT("Cannot apply item upgrade because an inventory container is unavailable.");
		return false;
	}

	FGuid UpgradedItemId;
	if (!FGuid::Parse(Response.Item.ItemInstanceId, UpgradedItemId) || !UpgradedItemId.IsValid())
	{
		OutError = TEXT("Item upgrade response contains an invalid itemInstanceId.");
		return false;
	}

	FFrontierItemInstance ExistingItem;
	auto FindItem = [&ExistingItem, UpgradedItemId](
		const TArray<FFrontierInventorySlot>& InventorySlots,
		const TArray<FFrontierInventorySlot>& StorageSlots,
		const TArray<FFrontierLoadoutSlot>& LoadoutSlots)
	{
		for (const FFrontierInventorySlot& Slot : InventorySlots)
		{
			if (Slot.bOccupied && Slot.ItemInstance.ItemInstanceId == UpgradedItemId)
			{
				ExistingItem = Slot.ItemInstance;
				return true;
			}
		}
		for (const FFrontierInventorySlot& Slot : StorageSlots)
		{
			if (Slot.bOccupied && Slot.ItemInstance.ItemInstanceId == UpgradedItemId)
			{
				ExistingItem = Slot.ItemInstance;
				return true;
			}
		}
		for (const FFrontierLoadoutSlot& Slot : LoadoutSlots)
		{
			if (Slot.bOccupied && Slot.ItemInstance.ItemInstanceId == UpgradedItemId)
			{
				ExistingItem = Slot.ItemInstance;
				return true;
			}
		}
		return false;
	};

	if (!FindItem(Inventory->GetSlots(), Storage->GetSlots(), Loadout->GetLoadoutSlots()))
	{
		OutError = TEXT("The upgraded item is not present in inventory, storage, or equipment.");
		return false;
	}

	FFrontierOnlineItemDTO MergedItem = Response.Item;
	MergedItem.Quantity = MergedItem.Quantity > 0 ? MergedItem.Quantity : ExistingItem.Quantity;
	if (!MergedItem.bHasDurability)
	{
		MergedItem.bHasDurability = true;
		MergedItem.Durability = ExistingItem.Durability;
	}
	if (MergedItem.FinalRarityTag.IsEmpty())
	{
		MergedItem.FinalRarityTag = ConvertItemRarityToBackendString(ExistingItem.GetDisplayRarity());
	}
	if (MergedItem.BindState.IsEmpty())
	{
		switch (ExistingItem.BindState)
		{
		case EFrontierItemBindState::AccountBound:
			MergedItem.BindState = TEXT("ACCOUNT_BOUND");
			break;
		case EFrontierItemBindState::CharacterBound:
			MergedItem.BindState = TEXT("CHARACTER_BOUND");
			break;
		default:
			MergedItem.BindState = TEXT("UNBOUND");
			break;
		}
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFrontierItemCatalogSubsystem* ItemCatalog = GameInstance
		? GameInstance->GetSubsystem<UFrontierItemCatalogSubsystem>()
		: nullptr;
	if (!ItemCatalog
		|| !FFrontierBackendInventoryMapper::ConvertBackendInventoryItemToRuntime(
			MergedItem,
			*ItemCatalog,
			OutUpdatedItem,
			OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("The upgraded item could not be converted to a runtime item.");
		}
		return false;
	}

	if (Response.bHasInventory && !ApplyInventoryDataToPlayerState(Response.Inventory, OutError))
	{
		return false;
	}
	if (Response.bHasStorage && !ApplyStorageDataToPlayerState(Response.Storage, OutError))
	{
		return false;
	}
	if (Response.bHasEquipment && !ApplyEquipmentDataToPlayerState(Response.Equipment, OutError))
	{
		return false;
	}

	TArray<FFrontierInventorySlot> InventorySlots = Inventory->GetSlots();
	TArray<FFrontierInventorySlot> StorageSlots = Storage->GetSlots();
	TArray<FFrontierLoadoutSlot> LoadoutSlots = Loadout->GetLoadoutSlots();
	bool bInventoryChanged = false;
	bool bStorageChanged = false;
	bool bLoadoutChanged = false;

	auto ReplaceUpgradedItem = [UpgradedItemId, &OutUpdatedItem](TArray<FFrontierInventorySlot>& Slots)
	{
		for (FFrontierInventorySlot& Slot : Slots)
		{
			if (Slot.bOccupied && Slot.ItemInstance.ItemInstanceId == UpgradedItemId)
			{
				Slot.ItemInstance = OutUpdatedItem;
				return true;
			}
		}
		return false;
	};
	if (!Response.bHasInventory)
	{
		bInventoryChanged = ReplaceUpgradedItem(InventorySlots);
	}
	if (!Response.bHasStorage)
	{
		bStorageChanged = ReplaceUpgradedItem(StorageSlots);
	}
	if (!Response.bHasEquipment)
	{
		for (FFrontierLoadoutSlot& Slot : LoadoutSlots)
		{
			if (Slot.bOccupied && Slot.ItemInstance.ItemInstanceId == UpgradedItemId)
			{
				Slot.ItemInstance = OutUpdatedItem;
				bLoadoutChanged = true;
				break;
			}
		}
	}

	for (const FFrontierOnlineConsumedUpgradeMaterial& Material : Response.ConsumedMaterials)
	{
		int32 RemainingToConsume = Material.ConsumedAmount;
		FGuid MaterialItemId;
		const bool bHasMaterialId = FGuid::Parse(Material.ItemInstanceId, MaterialItemId) && MaterialItemId.IsValid();
		auto ConsumeFromSlots = [
			&Material,
			&RemainingToConsume,
			bHasMaterialId,
			MaterialItemId](TArray<FFrontierInventorySlot>& Slots)
		{
			bool bChanged = false;
			for (FFrontierInventorySlot& Slot : Slots)
			{
				if (!Slot.bOccupied || !Slot.ItemInstance.IsValid())
				{
					continue;
				}
				const bool bMatches = bHasMaterialId
					? Slot.ItemInstance.ItemInstanceId == MaterialItemId
					: Slot.ItemInstance.GetTemplateId() == FName(*Material.ItemTemplateId);
				if (!bMatches)
				{
					continue;
				}

				const int32 NewQuantity = Material.bHasRemainingQuantity && bHasMaterialId
					? Material.RemainingQuantity
					: FMath::Max(0, Slot.ItemInstance.Quantity - RemainingToConsume);
				RemainingToConsume = FMath::Max(0, RemainingToConsume - Slot.ItemInstance.Quantity);
				if (NewQuantity <= 0)
				{
					const int32 SlotIndex = Slot.SlotIndex;
					Slot = FFrontierInventorySlot();
					Slot.SlotIndex = SlotIndex;
				}
				else
				{
					Slot.ItemInstance.Quantity = NewQuantity;
				}
				bChanged = true;
				if (bHasMaterialId || RemainingToConsume <= 0)
				{
					break;
				}
			}
			return bChanged;
		};

		if (!Response.bHasInventory)
		{
			bInventoryChanged |= ConsumeFromSlots(InventorySlots);
		}
		if (!Response.bHasStorage && RemainingToConsume > 0)
		{
			bStorageChanged |= ConsumeFromSlots(StorageSlots);
		}
	}

	if (bInventoryChanged && !Inventory->ApplyAuthoritativeInventoryState(InventorySlots))
	{
		OutError = TEXT("Failed to apply upgraded inventory state.");
		return false;
	}
	if (bStorageChanged && !Storage->ApplyAuthoritativeInventoryState(StorageSlots))
	{
		OutError = TEXT("Failed to apply upgraded storage state.");
		return false;
	}
	if (bLoadoutChanged)
	{
		Loadout->SetLoadoutSlotsFromSnapshot(LoadoutSlots);
	}
	return true;
}

void UFrontierBackendProtocolComponent::BroadcastItemUpgradeResult(const FFrontierItemUpgradeResult& Result)
{
	if (GetOwner() && GetOwner()->HasAuthority() && GetNetMode() != NM_Standalone)
	{
		ClientReceiveItemUpgradeResult(Result);
		return;
	}
	if (!Result.Currencies.IsEmpty())
	{
		OnCurrenciesChanged.Broadcast();
	}
	OnItemUpgradeCompleted.Broadcast(Result);
}

void UFrontierBackendProtocolComponent::ClientReceiveBackendLoginSucceeded_Implementation(const FFrontierBackendSteamLoginResult& Result)
{
	LastSteamLoginResult = Result;
	OnSteamLoginSucceeded.Broadcast(LastSteamLoginResult);
	OnLobbyBackendDataReady.Broadcast(LastSteamLoginResult);
}

void UFrontierBackendProtocolComponent::ClientReceiveBackendRequestFailed_Implementation(const FString& ErrorMessage)
{
	BroadcastFailureLocal(ErrorMessage);
}

void UFrontierBackendProtocolComponent::ClientReceiveNicknameUpdateSucceeded_Implementation(const FString& Nickname)
{
	if (UFrontierPlayerSessionSubsystem* PlayerSession = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr)
	{
		PlayerSession->SetNickname(Nickname);
	}
	LastSteamLoginResult.Player.Nickname = Nickname;
	ApplyProfileDisplayNameToOwner(this, Nickname);
	OnNicknameUpdateSucceeded.Broadcast(Nickname);
}

void UFrontierBackendProtocolComponent::ClientReceiveNicknameUpdateFailed_Implementation(const FString& ErrorMessage)
{
	OnNicknameUpdateFailed.Broadcast(ErrorMessage);
}

void UFrontierBackendProtocolComponent::ClientReceiveItemUpgradeResult_Implementation(
	const FFrontierItemUpgradeResult& Result)
{
	if (!Result.Currencies.IsEmpty())
	{
		LastCurrencies = Result.Currencies;
		OnCurrenciesChanged.Broadcast();
	}
	OnItemUpgradeCompleted.Broadcast(Result);
}

void UFrontierBackendProtocolComponent::ClientReceiveLobbyProgressionReady_Implementation(
	const FFrontierBackendSteamLoginResult& LoginResult,
	const FFrontierPlayerLevelSnapshot& Level)
{
	CompleteLocalLobbyProgression(LoginResult, Level);
}

void UFrontierBackendProtocolComponent::ClientReceiveRaidRefreshReady_Implementation(
	const FFrontierPlayerLevelSnapshot& Level,
	const FString& RaidSessionId,
	const FString& Outcome,
	const bool bHasReasonCode,
	const FString& ReasonCode,
	const bool bHasCommittedAt,
	const FString& CommittedAt)
{
	FFrontierOnlineRaidResultDTO Result;
	Result.RaidSessionId = RaidSessionId;
	Result.Outcome = Outcome;
	Result.bHasReasonCode = bHasReasonCode;
	Result.ReasonCode = ReasonCode;
	Result.bHasCommittedAt = bHasCommittedAt;
	Result.CommittedAt = CommittedAt;
	CompleteLocalRaidRefresh(Level, Result);
}

void UFrontierBackendProtocolComponent::CompleteLocalRaidRefresh(
	const FFrontierPlayerLevelSnapshot& Level,
	const FFrontierOnlineRaidResultDTO& RaidResult)
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UFrontierTemporarySkillPointSubsystem* SkillPointService = GameInstance
		? GameInstance->GetSubsystem<UFrontierTemporarySkillPointSubsystem>()
		: nullptr;
	const FString AccountId = !LastSteamLoginResult.Player.PlayerIdString.IsEmpty()
		? LastSteamLoginResult.Player.PlayerIdString
		: LastSteamLoginResult.Player.SteamId;
	if (!SkillPointService || AccountId.IsEmpty())
	{
		BroadcastFailureLocal(TEXT("Local progression service is unavailable during raid refresh."));
		if (UGameInstance* InnerGameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UFrontierRaidSessionSubsystem* RaidSession =
				InnerGameInstance->GetSubsystem<UFrontierRaidSessionSubsystem>())
			{
				RaidSession->FailClientFlow(
					TEXT("Local progression service is unavailable during raid refresh."),
					true);
			}
		}
		ServerAcknowledgeRaidLobbyRefresh(RaidResult.RaidSessionId, false);
		return;
	}

	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	SkillPointService->ProcessLevelSnapshot(
		AccountId,
		Level,
		[WeakThis, Level, RaidResult](const FFrontierTemporarySkillPointResult& SkillPointResult)
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This || !SkillPointResult.bSucceeded)
			{
				if (This)
				{
					This->BroadcastFailureLocal(SkillPointResult.ErrorMessage.IsEmpty()
						? TEXT("Local progression save failed during raid refresh.")
						: SkillPointResult.ErrorMessage);
					if (UGameInstance* InnerGameInstance = This->GetWorld()
						? This->GetWorld()->GetGameInstance()
						: nullptr)
					{
						if (UFrontierRaidSessionSubsystem* RaidSession =
							InnerGameInstance->GetSubsystem<UFrontierRaidSessionSubsystem>())
						{
							RaidSession->FailClientFlow(
								SkillPointResult.ErrorMessage.IsEmpty()
									? TEXT("Local progression save failed during raid refresh.")
									: SkillPointResult.ErrorMessage,
								true);
						}
					}
					This->ServerAcknowledgeRaidLobbyRefresh(RaidResult.RaidSessionId, false);
				}
				return;
			}
			if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(This->GetOwner()))
			{
				if (UFrontierSkillTreePersistenceComponent* Persistence = LobbyController->GetSkillTreePersistenceComponent())
				{
					const FString AccountId = !This->LastSteamLoginResult.Player.PlayerIdString.IsEmpty()
						? This->LastSteamLoginResult.Player.PlayerIdString
						: This->LastSteamLoginResult.Player.SteamId;
					Persistence->StartSkillTreeSyncForAccount(AccountId);
				}
			}
			This->LastPlayerLevel = Level;
			This->LastTemporarySkillPointResult = SkillPointResult;
			This->OnLobbyLevelReady.Broadcast(Level, SkillPointResult);
			if (UGameInstance* InnerGameInstance = This->GetWorld()
				? This->GetWorld()->GetGameInstance()
				: nullptr)
			{
				if (UFrontierRaidSessionSubsystem* RaidSession =
					InnerGameInstance->GetSubsystem<UFrontierRaidSessionSubsystem>())
				{
					RaidSession->CompleteClientLobbyRefresh(RaidResult);
				}
			}
			This->ServerAcknowledgeRaidLobbyRefresh(RaidResult.RaidSessionId, true);
		});
}

void UFrontierBackendProtocolComponent::CompleteLocalLobbyProgression(
	const FFrontierBackendSteamLoginResult& LoginResult,
	const FFrontierPlayerLevelSnapshot& Level)
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UFrontierTemporarySkillPointSubsystem* SkillPointService = GameInstance
		? GameInstance->GetSubsystem<UFrontierTemporarySkillPointSubsystem>()
		: nullptr;
	const FString AccountId = !LoginResult.Player.PlayerIdString.IsEmpty()
		? LoginResult.Player.PlayerIdString
		: LoginResult.Player.SteamId;
	if (!SkillPointService || AccountId.IsEmpty())
	{
		BroadcastFailureLocal(TEXT("Local temporary skill-point service or verified account identity is unavailable."));
		return;
	}

	const TWeakObjectPtr<UFrontierBackendProtocolComponent> WeakThis(this);
	SkillPointService->ProcessLevelSnapshot(
		AccountId,
		Level,
		[WeakThis, LoginResult, Level, AccountId](const FFrontierTemporarySkillPointResult& Result)
		{
			UFrontierBackendProtocolComponent* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!Result.bSucceeded)
			{
				This->BroadcastFailureLocal(Result.ErrorMessage.IsEmpty()
					? TEXT("Local progression save failed.")
					: Result.ErrorMessage);
				return;
			}

			This->LastSteamLoginResult = LoginResult;
			This->LastPlayerLevel = Level;
			This->LastTemporarySkillPointResult = Result;
			if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(This->GetOwner()))
			{
				if (UFrontierSkillTreePersistenceComponent* Persistence = LobbyController->GetSkillTreePersistenceComponent())
				{
					Persistence->StartSkillTreeSyncForAccount(AccountId);
				}
			}

			This->OnLobbyLevelReady.Broadcast(Level, Result);
			This->OnSteamLoginSucceeded.Broadcast(This->LastSteamLoginResult);
			This->OnLobbyBackendDataReady.Broadcast(This->LastSteamLoginResult);
		});
}

FFrontierBackendSteamLoginResult UFrontierBackendProtocolComponent::BuildSanitizedLoginResult(const FFrontierOnlineSteamLoginResponse& Response) const
{
	FFrontierBackendSteamLoginResult Result;
	Result.bSuccess = true;
	Result.Session.SessionId = Response.Session.SessionId;
	Result.Session.PlayerIdString = Response.Session.PlayerIdString;
	Result.Session.PlayerId = Response.Session.PlayerId;
	Result.Session.Platform = Response.Session.Platform;
	Result.Session.ExpiresAt = Response.Session.ExpiresAt;
	Result.Tokens.TokenType = Response.Tokens.TokenType;
	Result.Tokens.AccessTokenExpiresAt = Response.Tokens.AccessTokenExpiresAt;
	Result.Tokens.RefreshTokenExpiresAt = Response.Tokens.RefreshTokenExpiresAt;
	Result.Player.PlayerId = Response.Player.PlayerId;
	Result.Player.PlayerIdString = Response.Player.PlayerIdString;
	Result.Player.SteamId = Response.Player.SteamId;
	Result.Player.Nickname = Response.Player.Nickname;
	Result.bIsNewPlayer = Response.bIsNewPlayer;
	Result.RequestId = Response.RequestId;
	Result.ServerTime = Response.ServerTime;
	Result.Message = Response.Message;
	return Result;
}

void UFrontierBackendProtocolComponent::BroadcastFailure(const FString& ErrorMessage)
{
	bRequestInProgress = false;
	if (GetOwner() && GetOwner()->HasAuthority() && GetNetMode() != NM_Standalone)
	{
		ClientReceiveBackendRequestFailed(ErrorMessage);
		return;
	}
	BroadcastFailureLocal(ErrorMessage);
}

void UFrontierBackendProtocolComponent::BroadcastFailureLocal(const FString& ErrorMessage)
{
	FRONTIER_LOG(Warning, TEXT("Backend request failed. Error=%s"), *ErrorMessage);
	OnBackendRequestFailed.Broadcast(ErrorMessage);
}

void UFrontierBackendProtocolComponent::BroadcastMatchmakingStatus(const FString& Status)
{
	if (Status.IsEmpty())
	{
		return;
	}

	FRONTIER_LOG(
		Log,
		TEXT("[Matchmaking] Broadcasting status. Controller=%s Local=%d Authority=%d Status=%s TicketId=%s PartyId=%s"),
		*GetNameSafe(GetOwner()),
		GetOwner() && Cast<APlayerController>(GetOwner()) && Cast<APlayerController>(GetOwner())->IsLocalController() ? 1 : 0,
		GetOwner() && GetOwner()->HasAuthority() ? 1 : 0,
		*Status,
		PendingMatchmakingTicketId.IsEmpty() ? TEXT("<empty>") : *PendingMatchmakingTicketId,
		PendingMatchmakingPartyId.IsEmpty() ? TEXT("<none>") : *PendingMatchmakingPartyId);
	OnMatchmakingStatusChanged.Broadcast(Status);
	if (GetOwner() && GetOwner()->HasAuthority() && GetNetMode() != NM_Standalone)
	{
		ClientReceiveMatchmakingStatus(Status);
	}
}
