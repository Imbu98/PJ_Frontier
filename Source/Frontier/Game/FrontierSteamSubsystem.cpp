#include "FrontierSteamSubsystem.h"

#include "Frontier.h"
#include "FrontierOnlineConfig.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"

bool UFrontierSteamSubsystem::CheckSteamLogin()
{
    CachedSteamId.Reset();
    CachedNickname.Reset();

    IOnlineSubsystem* OnlineSubsystem =
        IOnlineSubsystem::Get(TEXT("STEAM"));

	if (!OnlineSubsystem)
	{
		return false;
    }

    const FName SubsystemName =
        OnlineSubsystem->GetSubsystemName();

	if (SubsystemName != FName(TEXT("STEAM")))
	{
		return false;
    }

    const IOnlineIdentityPtr IdentityInterface =
        OnlineSubsystem->GetIdentityInterface();

	if (!IdentityInterface.IsValid())
	{
		return false;
    }

    constexpr int32 LocalUserNum = 0;

    const ELoginStatus::Type LoginStatus =
        IdentityInterface->GetLoginStatus(LocalUserNum);

	if (LoginStatus != ELoginStatus::LoggedIn)
	{
		return false;
    }

    const FUniqueNetIdPtr UniquePlayerId =
        IdentityInterface->GetUniquePlayerId(LocalUserNum);

	if (!UniquePlayerId.IsValid())
	{
		return false;
    }

    CachedSteamId =
        UniquePlayerId->ToString();

    CachedNickname =
        IdentityInterface->GetPlayerNickname(LocalUserNum);

	return true;
}

void UFrontierSteamSubsystem::RequestSteamWebApiTicket()
{
    CachedAuthTicket.Reset();

    IOnlineSubsystem* OnlineSubsystem =
        IOnlineSubsystem::Get(TEXT("STEAM"));

	if (!OnlineSubsystem)
	{
		return;
    }

    const IOnlineIdentityPtr Identity =
        OnlineSubsystem->GetIdentityInterface();

	if (!Identity.IsValid())
	{
		return;
    }

    constexpr int32 LocalUserNum = 0;

	if (Identity->GetLoginStatus(LocalUserNum)
		!= ELoginStatus::LoggedIn)
	{
		return;
    }

    const IOnlineIdentity::
        FOnGetLinkedAccountAuthTokenCompleteDelegate
        CompleteDelegate =
        IOnlineIdentity::
            FOnGetLinkedAccountAuthTokenCompleteDelegate::
            CreateUObject(
                this,
                &ThisClass::
                    HandleLinkedAccountAuthTokenComplete);

    /*
     * Backend /auth/steam expects a Steam Web API ticket for the
     * "frontier-backend" remote service identity. This maps to
     * ISteamUser::GetAuthTicketForWebApi through OnlineSubsystemSteam.
     */
    

    const FFrontierOnlineConfig OnlineConfig = FFrontierOnlineConfig::Load();
    FString ConfigError;
    if (!OnlineConfig.IsValid(ConfigError))
    {
        OnSteamAuthTicketFailed.Broadcast(ConfigError);
        return;
    }

    Identity->GetLinkedAccountAuthToken(
        LocalUserNum,
        FString::Printf(TEXT("WebAPI:%s"), *OnlineConfig.SteamIdentity),
        CompleteDelegate);
}

void UFrontierSteamSubsystem::
HandleLinkedAccountAuthTokenComplete(
    const int32 LocalUserNum,
    const bool bWasSuccessful,
    const FExternalAuthToken& AuthToken)
{
    if (!bWasSuccessful)
    {
        const FString Error =
            TEXT("Steam 인증 티켓 발급에 실패했습니다.");

		OnSteamAuthTicketFailed.Broadcast(Error);
        return;
    }
    
    FString TicketString;

    if (!AuthToken.TokenString.IsEmpty())
    {
        TicketString = AuthToken.TokenString;
    }
	else if (!AuthToken.TokenData.IsEmpty())
	{
		TicketString = BytesToHexString(
			AuthToken.TokenData);
	}

    CachedAuthTicket = MoveTemp(TicketString);

	OnSteamAuthTicketReceived.Broadcast(
        CachedAuthTicket);
}

FString UFrontierSteamSubsystem::BytesToHexString(
    const TArray<uint8>& Bytes)
{
    if (Bytes.IsEmpty())
    {
        return FString();
    }

    return BytesToHex(
        Bytes.GetData(),
        Bytes.Num());
}
