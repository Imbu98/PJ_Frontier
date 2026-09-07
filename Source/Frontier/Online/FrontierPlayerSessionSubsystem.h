#pragma once

#include "CoreMinimal.h"
#include "FrontierOnlineHttpClient.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FrontierPlayerSessionSubsystem.generated.h"

/**
 * Process-local player session store. Player access and refresh tokens never leave
 * memory and survive PlayerController/world replacement through the GameInstance.
 */
UCLASS()
class FRONTIER_API UFrontierPlayerSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	using FAccessTokenCompletion = TFunction<void(bool bSucceeded, const FString& AccessToken, const FString& Error)>;
	using FLogoutCompletion = TFunction<void(bool bSucceeded, const FString& Error)>;
	using FRefreshExecutor = TFunction<bool(
		const FString& RefreshToken,
		const FString& SessionId,
		FFrontierOnlineRefreshCompletion Completion,
		FString& OutError)>;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void SetAuthenticatedSession(
		const FFrontierOnlineSessionInfo& Session,
		const FFrontierOnlineTokenInfo& Tokens,
		int64 PlayerId,
		const FString& SteamId,
		const FString& Nickname = FString());
	void SetNickname(const FString& InNickname) { Nickname = InNickname; }
	void SetLocale(const FString& InLocale) { Locale = InLocale; }
	void ClearSession();

	bool HasAuthenticatedSession() const;
	const FString& GetSessionId() const { return Session.SessionId; }
	int64 GetPlayerId() const { return PlayerId; }
	const FString& GetSteamId() const { return SteamId; }
	const FString& GetNickname() const { return Nickname; }
	const FString& GetLocale() const { return Locale; }

	void AcquireAccessToken(FAccessTokenCompletion Completion, bool bForceRefresh = false);
	void InvalidateAccessTokenAndRefresh(FAccessTokenCompletion Completion);
	void RequestLogout(bool bAllSessions, FLogoutCompletion Completion = FLogoutCompletion());

	bool IsRefreshInFlight() const { return bRefreshInFlight; }
	bool IsLogoutInFlight() const { return bLogoutInFlight; }
	int32 GetPendingRequestCount() const { return PendingTokenCallbacks.Num(); }

	static bool IsExpiryWithinWindow(
		const FString& ExpiresAt,
		const FDateTime& NowUtc,
		FTimespan RefreshWindow);

#if WITH_DEV_AUTOMATION_TESTS
	void SetRefreshExecutorForTests(FRefreshExecutor InExecutor) { RefreshExecutor = MoveTemp(InExecutor); }
#endif

private:
	void StartRefresh();
	void HandleRefreshCompleted(const FFrontierOnlineRefreshResponse& Response);
	void CompletePending(bool bSucceeded, const FString& Error);
	void StartLogoutRequest(bool bAllSessions);
	void HandleApplicationPreExit();
	void HandleLogoutCompleted(const FFrontierOnlineLogoutResponse& Response);
	void CompleteLogout(bool bSucceeded, const FString& Error);

	FFrontierOnlineSessionInfo Session;
	FFrontierOnlineTokenInfo Tokens;
	int64 PlayerId = 0;
	FString SteamId;
	FString Nickname;
	FString Locale;
	bool bRefreshInFlight = false;
	TArray<FAccessTokenCompletion> PendingTokenCallbacks;
	TSharedPtr<FFrontierOnlineHttpClient> RefreshHttpClient;
	TSharedPtr<FFrontierOnlineHttpClient> LogoutHttpClient;
	FRefreshExecutor RefreshExecutor;
	TArray<FLogoutCompletion> PendingLogoutCallbacks;
	FDelegateHandle PreExitDelegateHandle;
	bool bLogoutInFlight = false;
};
