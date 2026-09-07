#include "FrontierOnlineConfig.h"

#include "Misc/ConfigCacheIni.h"

namespace
{
constexpr TCHAR ConfigSection[] = TEXT("FrontierOnline");
constexpr TCHAR DefaultBackendBaseUrl[] = TEXT("http://15.135.135.207:8080");
constexpr TCHAR DefaultSteamAuthEndpoint[] = TEXT("/v1/auth/steam/login");
constexpr TCHAR DefaultSteamIdentity[] = TEXT("frontier-backend");
constexpr TCHAR DefaultClientVersion[] = TEXT("1.0.0+100");
constexpr TCHAR DefaultPlatform[] = TEXT("STEAM");
}

FFrontierOnlineConfig FFrontierOnlineConfig::Load()
{
	FFrontierOnlineConfig Config;
	if (GConfig)
	{
		GConfig->GetString(ConfigSection, TEXT("BackendBaseUrl"), Config.BackendBaseUrl, GGameIni);
		GConfig->GetString(ConfigSection, TEXT("BackendWebSocketUrl"), Config.BackendWebSocketUrl, GGameIni);
		GConfig->GetString(ConfigSection, TEXT("SteamAuthEndpoint"), Config.SteamAuthEndpoint, GGameIni);
		GConfig->GetString(ConfigSection, TEXT("SteamIdentity"), Config.SteamIdentity, GGameIni);
		GConfig->GetString(ConfigSection, TEXT("ClientVersion"), Config.ClientVersion, GGameIni);
		GConfig->GetString(ConfigSection, TEXT("Platform"), Config.Platform, GGameIni);
	}

	// Preserve current deployment behavior when a generated config hierarchy omits the custom section.
	if (Config.BackendBaseUrl.IsEmpty()
		|| Config.BackendBaseUrl.Equals(TEXT("http:"), ESearchCase::IgnoreCase)
		|| Config.BackendBaseUrl.Equals(TEXT("https:"), ESearchCase::IgnoreCase))
	{
		Config.BackendBaseUrl = DefaultBackendBaseUrl;
	}
	if (Config.SteamAuthEndpoint.IsEmpty())
	{
		Config.SteamAuthEndpoint = DefaultSteamAuthEndpoint;
	}
	Config.BackendWebSocketUrl.TrimStartAndEndInline();
	if (Config.SteamIdentity.IsEmpty())
	{
		Config.SteamIdentity = DefaultSteamIdentity;
	}
	if (Config.ClientVersion.IsEmpty())
	{
		Config.ClientVersion = DefaultClientVersion;
	}
	if (Config.Platform.IsEmpty())
	{
		Config.Platform = DefaultPlatform;
	}

	return Config;
}

bool FFrontierOnlineConfig::IsValid(FString& OutError) const
{
	OutError.Reset();
	if (!BackendBaseUrl.StartsWith(TEXT("http://")) && !BackendBaseUrl.StartsWith(TEXT("https://")))
	{
		OutError = TEXT("BackendBaseUrl must use http or https.");
		return false;
	}
	if (SteamAuthEndpoint.IsEmpty() || SteamIdentity.IsEmpty() || ClientVersion.IsEmpty() || Platform.IsEmpty())
	{
		OutError = TEXT("FrontierOnline configuration contains an empty required value.");
		return false;
	}

	return true;
}

FString FFrontierOnlineConfig::BuildUrl(const FString& Endpoint) const
{
	FString NormalizedBaseUrl = BackendBaseUrl;
	while (NormalizedBaseUrl.EndsWith(TEXT("/")))
	{
		NormalizedBaseUrl.LeftChopInline(1);
	}

	return Endpoint.StartsWith(TEXT("/"))
		? NormalizedBaseUrl + Endpoint
		: NormalizedBaseUrl + TEXT("/") + Endpoint;
}
