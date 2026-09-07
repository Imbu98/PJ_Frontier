#include "Online/FrontierPlayerSessionSubsystem.h"

#include "Frontier.h"
#include "Misc/CoreDelegates.h"
#include "Online/FrontierBackendWebSocketSubsystem.h"

namespace
{
constexpr int32 AccessTokenRefreshWindowSeconds = 60;
}

void UFrontierPlayerSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	FRONTIER_LOG_FUNC();

	Super::Initialize(Collection);
	RefreshHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
	LogoutHttpClient = MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
	PreExitDelegateHandle = FCoreDelegates::OnPreExit.AddUObject(
		this,
		&UFrontierPlayerSessionSubsystem::HandleApplicationPreExit);
	RefreshExecutor = [this](
		const FString& RefreshToken,
		const FString& SessionId,
		FFrontierOnlineRefreshCompletion Completion,
		FString& OutError)
	{
		return RefreshHttpClient.IsValid()
			&& RefreshHttpClient->RefreshSession(RefreshToken, SessionId, MoveTemp(Completion), OutError);
	};
}

void UFrontierPlayerSessionSubsystem::Deinitialize()
{
	FRONTIER_LOG_FUNC();

	if (PreExitDelegateHandle.IsValid())
	{
		FCoreDelegates::OnPreExit.Remove(PreExitDelegateHandle);
		PreExitDelegateHandle.Reset();
	}
	CompletePending(false, TEXT("Player session subsystem is shutting down."));
	CompleteLogout(false, TEXT("Player session subsystem is shutting down."));
	RefreshExecutor = nullptr;
	RefreshHttpClient.Reset();
	LogoutHttpClient.Reset();
	ClearSession();
	Super::Deinitialize();
}

void UFrontierPlayerSessionSubsystem::SetAuthenticatedSession(
	const FFrontierOnlineSessionInfo& InSession,
	const FFrontierOnlineTokenInfo& InTokens,
	const int64 InPlayerId,
	const FString& InSteamId,
	const FString& InNickname)
{
	Session = InSession;
	Tokens = InTokens;
	PlayerId = InPlayerId;
	SteamId = InSteamId;
	Nickname = InNickname;
	if (UFrontierBackendWebSocketSubsystem* WebSocket = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierBackendWebSocketSubsystem>()
		: nullptr)
	{
		WebSocket->ConnectAuthenticated();
	}
}

void UFrontierPlayerSessionSubsystem::ClearSession()
{
	FRONTIER_LOG_FUNC();
	if (UFrontierBackendWebSocketSubsystem* WebSocket = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierBackendWebSocketSubsystem>()
		: nullptr)
	{
		WebSocket->Disconnect();
	}

	Session = FFrontierOnlineSessionInfo();
	Tokens = FFrontierOnlineTokenInfo();
	PlayerId = 0;
	SteamId.Reset();
	Nickname.Reset();
	Locale.Reset();
	bRefreshInFlight = false;
	PendingTokenCallbacks.Reset();
}

void UFrontierPlayerSessionSubsystem::RequestLogout(
	const bool bAllSessions,
	FLogoutCompletion Completion)
{
	FRONTIER_LOG_FUNC();

	if (Completion)
	{
		PendingLogoutCallbacks.Add(MoveTemp(Completion));
	}
	if (bLogoutInFlight)
	{
		return;
	}
	if (!HasAuthenticatedSession())
	{
		CompleteLogout(true, FString());
		return;
	}
	if (!LogoutHttpClient.IsValid())
	{
		CompleteLogout(false, TEXT("Logout transport is unavailable."));
		return;
	}

	bLogoutInFlight = true;
	const TWeakObjectPtr<UFrontierPlayerSessionSubsystem> WeakThis(this);
	AcquireAccessToken(
		[WeakThis, bAllSessions](
			const bool bSucceeded,
			const FString& AccessToken,
			const FString& Error)
		{
			UFrontierPlayerSessionSubsystem* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (!bSucceeded || AccessToken.IsEmpty())
			{
				This->CompleteLogout(
					false,
					Error.IsEmpty() ? TEXT("A valid access token could not be acquired for logout.") : Error);
				return;
			}
			This->StartLogoutRequest(bAllSessions);
		});
}

void UFrontierPlayerSessionSubsystem::StartLogoutRequest(const bool bAllSessions)
{
	FRONTIER_LOG_FUNC();

	if (!bLogoutInFlight || !LogoutHttpClient.IsValid() || Tokens.RefreshToken.IsEmpty())
	{
		CompleteLogout(false, TEXT("Logout session data is unavailable."));
		return;
	}

	FString StartError;
	const TWeakObjectPtr<UFrontierPlayerSessionSubsystem> WeakThis(this);
	if (!LogoutHttpClient->LogoutSession(
		Tokens.AccessToken,
		Tokens.RefreshToken,
		bAllSessions,
		[WeakThis](const FFrontierOnlineLogoutResponse& Response)
		{
			if (UFrontierPlayerSessionSubsystem* This = WeakThis.Get())
			{
				This->HandleLogoutCompleted(Response);
			}
		},
		StartError))
	{
		bLogoutInFlight = false;
		CompleteLogout(false, StartError.IsEmpty() ? TEXT("Logout request could not start.") : StartError);
	}
}

bool UFrontierPlayerSessionSubsystem::HasAuthenticatedSession() const
{
	return !Session.SessionId.IsEmpty() && PlayerId > 0
		&& !Tokens.AccessToken.IsEmpty() && !Tokens.RefreshToken.IsEmpty();
}

bool UFrontierPlayerSessionSubsystem::IsExpiryWithinWindow(
	const FString& ExpiresAt,
	const FDateTime& NowUtc,
	const FTimespan RefreshWindow)
{
	FDateTime Expiration;
	if (ExpiresAt.IsEmpty() || !FDateTime::ParseIso8601(*ExpiresAt, Expiration))
	{
		// Unknown expiry is treated as requiring refresh instead of risking a stale request.
		return true;
	}
	return Expiration <= NowUtc + RefreshWindow;
}

void UFrontierPlayerSessionSubsystem::AcquireAccessToken(
	FAccessTokenCompletion Completion,
	const bool bForceRefresh)
{
	if (!Completion)
	{
		return;
	}
	if (!HasAuthenticatedSession())
	{
		Completion(false, FString(), TEXT("No authenticated player session is available."));
		return;
	}

	const bool bNeedsRefresh = bForceRefresh || IsExpiryWithinWindow(
		Tokens.AccessTokenExpiresAt,
		FDateTime::UtcNow(),
		FTimespan::FromSeconds(AccessTokenRefreshWindowSeconds));
	if (!bNeedsRefresh)
	{
		Completion(true, Tokens.AccessToken, FString());
		return;
	}

	PendingTokenCallbacks.Add(MoveTemp(Completion));
	if (!bRefreshInFlight)
	{
		StartRefresh();
	}
}

void UFrontierPlayerSessionSubsystem::InvalidateAccessTokenAndRefresh(FAccessTokenCompletion Completion)
{
	AcquireAccessToken(MoveTemp(Completion), true);
}

void UFrontierPlayerSessionSubsystem::StartRefresh()
{
	if (bRefreshInFlight)
	{
		return;
	}
	if (!RefreshExecutor)
	{
		CompletePending(false, TEXT("Player session refresh transport is unavailable."));
		return;
	}

	bRefreshInFlight = true;
	FString StartError;
	const TWeakObjectPtr<UFrontierPlayerSessionSubsystem> WeakThis(this);
	if (!RefreshExecutor(
		Tokens.RefreshToken,
		Session.SessionId,
		[WeakThis](const FFrontierOnlineRefreshResponse& Response)
		{
			if (UFrontierPlayerSessionSubsystem* This = WeakThis.Get())
			{
				This->HandleRefreshCompleted(Response);
			}
		},
		StartError))
	{
		bRefreshInFlight = false;
		CompletePending(false, StartError.IsEmpty() ? TEXT("Player session refresh could not start.") : StartError);
	}
}

void UFrontierPlayerSessionSubsystem::HandleRefreshCompleted(
	const FFrontierOnlineRefreshResponse& Response)
{
	bRefreshInFlight = false;
	if (!Response.bTransportSucceeded || !Response.bSuccess
		|| Response.Tokens.AccessToken.IsEmpty() || Response.Tokens.RefreshToken.IsEmpty())
	{
		const FString Error = Response.Message.IsEmpty()
			? TEXT("Player session refresh failed.")
			: Response.Message;
		CompletePending(false, Error);
		return;
	}

	// Rotation is applied as one game-thread assignment before any waiter resumes.
	Session = Response.Session;
	Tokens = Response.Tokens;
	PlayerId = Response.Session.PlayerId;
	CompletePending(true, FString());
}

void UFrontierPlayerSessionSubsystem::CompletePending(
	const bool bSucceeded,
	const FString& Error)
{
	TArray<FAccessTokenCompletion> Callbacks = MoveTemp(PendingTokenCallbacks);
	PendingTokenCallbacks.Reset();
	const FString AccessToken = bSucceeded ? Tokens.AccessToken : FString();
	for (FAccessTokenCompletion& Callback : Callbacks)
	{
		if (Callback)
		{
			Callback(bSucceeded, AccessToken, Error);
		}
	}
}

void UFrontierPlayerSessionSubsystem::HandleApplicationPreExit()
{
	FRONTIER_LOG_FUNC();

	if (HasAuthenticatedSession() && !bLogoutInFlight)
	{
		RequestLogout(false);
	}
}

void UFrontierPlayerSessionSubsystem::HandleLogoutCompleted(
	const FFrontierOnlineLogoutResponse& Response)
{
	FRONTIER_LOG_FUNC();

	const bool bSucceeded = Response.bTransportSucceeded && Response.bSuccess && Response.bRevoked;
	const FString Error = bSucceeded
		? FString()
		: (Response.Message.IsEmpty() ? TEXT("Backend logout failed.") : Response.Message);
	if (bSucceeded)
	{
		FRONTIER_LOG(
			Log,
			TEXT("Backend logout succeeded. RevokedSessionCount=%d"),
			Response.RevokedSessionCount);
		ClearSession();
	}
	else
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Backend logout failed. HttpStatus=%d ErrorCode=%s Message=%s"),
			Response.HttpStatus,
			Response.ErrorCode.IsEmpty() ? TEXT("<empty>") : *Response.ErrorCode,
			Error.IsEmpty() ? TEXT("<empty>") : *Error);
	}
	CompleteLogout(bSucceeded, Error);
}

void UFrontierPlayerSessionSubsystem::CompleteLogout(
	const bool bSucceeded,
	const FString& Error)
{
	FRONTIER_LOG_FUNC();

	bLogoutInFlight = false;
	TArray<FLogoutCompletion> Callbacks = MoveTemp(PendingLogoutCallbacks);
	PendingLogoutCallbacks.Reset();
	for (FLogoutCompletion& Callback : Callbacks)
	{
		if (Callback)
		{
			Callback(bSucceeded, Error);
		}
	}
}
