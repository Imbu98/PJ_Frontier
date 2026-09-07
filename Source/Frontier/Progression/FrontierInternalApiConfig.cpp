#include "Progression/FrontierInternalApiConfig.h"

#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "GenericPlatform/GenericPlatformHttp.h"

namespace
{
constexpr TCHAR InternalApiConfigSection[] = TEXT("FrontierInternalApi");

void ApplyEnvironmentOverride(const TCHAR* VariableName, FString& InOutValue)
{
	const FString Value = FPlatformMisc::GetEnvironmentVariable(VariableName);
	if (!Value.IsEmpty())
	{
		InOutValue = Value;
	}
}

void ApplyCommandLineOverride(const TCHAR* Key, FString& InOutValue)
{
	FString Value;
	if (FParse::Value(FCommandLine::Get(), Key, Value) && !Value.IsEmpty())
	{
		InOutValue = MoveTemp(Value);
	}
}
}

FFrontierInternalApiConfig FFrontierInternalApiConfig::Load()
{
	FFrontierInternalApiConfig Config;
	if (GConfig)
	{
		FString ConfiguredBaseUrl;
		if (GConfig->GetString(
				InternalApiConfigSection,
			TEXT("InternalApiBaseUrl"),
			ConfiguredBaseUrl,
			GGameIni)
			&& ConfiguredBaseUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
		{
			Config.InternalApiBaseUrl = MoveTemp(ConfiguredBaseUrl);
		}
		GConfig->GetString(InternalApiConfigSection, TEXT("ServiceJwtPath"), Config.ServiceJwtPath, GGameIni);
		GConfig->GetString(InternalApiConfigSection, TEXT("ServiceJwtEnvironmentKey"), Config.ServiceJwtEnvironmentKey, GGameIni);
		GConfig->GetString(InternalApiConfigSection, TEXT("MtlsCertificatePath"), Config.MtlsCertificatePath, GGameIni);
		GConfig->GetString(InternalApiConfigSection, TEXT("MtlsPrivateKeyPath"), Config.MtlsPrivateKeyPath, GGameIni);
		GConfig->GetString(InternalApiConfigSection, TEXT("MtlsCaCertificatePath"), Config.MtlsCaCertificatePath, GGameIni);
		GConfig->GetString(InternalApiConfigSection, TEXT("ServiceName"), Config.ServiceName, GGameIni);
		GConfig->GetString(InternalApiConfigSection, TEXT("RaidServerId"), Config.RaidServerId, GGameIni);
		GConfig->GetString(InternalApiConfigSection, TEXT("MatchId"), Config.MatchId, GGameIni);
		GConfig->GetString(InternalApiConfigSection, TEXT("MapId"), Config.MapId, GGameIni);
		GConfig->GetString(InternalApiConfigSection, TEXT("PublicServerAddress"), Config.PublicServerAddress, GGameIni);
		GConfig->GetInt(InternalApiConfigSection, TEXT("PublicServerPort"), Config.PublicServerPort, GGameIni);
		GConfig->GetInt64(InternalApiConfigSection, TEXT("ServiceTokenLifetimeSeconds"), Config.ServiceTokenLifetimeSeconds, GGameIni);
		GConfig->GetBool(
			InternalApiConfigSection,
			TEXT("RaidResultCommitAutomaticallyGrantsExperience"),
			Config.bRaidResultCommitAutomaticallyGrantsExperience,
			GGameIni);
		GConfig->GetFloat(InternalApiConfigSection, TEXT("RequestTimeoutSeconds"), Config.RequestTimeoutSeconds, GGameIni);
		GConfig->GetInt(InternalApiConfigSection, TEXT("MaximumRetryAttempts"), Config.MaximumRetryAttempts, GGameIni);
		GConfig->GetFloat(InternalApiConfigSection, TEXT("RetryBaseDelaySeconds"), Config.RetryBaseDelaySeconds, GGameIni);
	}

	ApplyEnvironmentOverride(TEXT("FRONTIER_INTERNAL_API_BASE_URL"), Config.InternalApiBaseUrl);
	ApplyEnvironmentOverride(TEXT("FRONTIER_MTLS_CERT_PATH"), Config.MtlsCertificatePath);
	ApplyEnvironmentOverride(TEXT("FRONTIER_MTLS_KEY_PATH"), Config.MtlsPrivateKeyPath);
	ApplyEnvironmentOverride(TEXT("FRONTIER_MTLS_CA_PATH"), Config.MtlsCaCertificatePath);
	ApplyEnvironmentOverride(TEXT("FRONTIER_RAID_SERVER_ID"), Config.RaidServerId);
	ApplyEnvironmentOverride(TEXT("FRONTIER_MATCH_ID"), Config.MatchId);
	ApplyEnvironmentOverride(TEXT("FRONTIER_MAP_ID"), Config.MapId);
	ApplyEnvironmentOverride(TEXT("FRONTIER_SERVER_ADDRESS"), Config.PublicServerAddress);
	ApplyEnvironmentOverride(TEXT("FRONTIER_SERVICE_JWT_PATH"), Config.ServiceJwtPath);
	ApplyCommandLineOverride(TEXT("InternalApiBaseUrl="), Config.InternalApiBaseUrl);
	ApplyCommandLineOverride(TEXT("FrontierInternalApiBaseUrl="), Config.InternalApiBaseUrl);
	ApplyCommandLineOverride(TEXT("FrontierMtlsCertPath="), Config.MtlsCertificatePath);
	ApplyCommandLineOverride(TEXT("FrontierMtlsKeyPath="), Config.MtlsPrivateKeyPath);
	ApplyCommandLineOverride(TEXT("FrontierMtlsCaPath="), Config.MtlsCaCertificatePath);
	ApplyCommandLineOverride(TEXT("FrontierRaidServerId="), Config.RaidServerId);
	ApplyCommandLineOverride(TEXT("RaidServerId="), Config.RaidServerId);
	ApplyCommandLineOverride(TEXT("MatchId="), Config.MatchId);
	ApplyCommandLineOverride(TEXT("MapId="), Config.MapId);
	ApplyCommandLineOverride(TEXT("MapID="), Config.MapId);
	ApplyCommandLineOverride(TEXT("FrontierMapId="), Config.MapId);
	ApplyCommandLineOverride(TEXT("ServerAddress="), Config.PublicServerAddress);
	ApplyCommandLineOverride(TEXT("PublicServerAddress="), Config.PublicServerAddress);
	ApplyCommandLineOverride(TEXT("FrontierServiceJwtPath="), Config.ServiceJwtPath);
	FParse::Value(FCommandLine::Get(), TEXT("Port="), Config.PublicServerPort);
	FParse::Value(FCommandLine::Get(), TEXT("PublicServerPort="), Config.PublicServerPort);
	return Config;
}

bool FFrontierInternalApiConfig::ResolveServiceJwt(FString& OutJwt, FString& OutError) const
{
	OutJwt.Reset();
	OutError.Reset();
	if (!ServiceJwtEnvironmentKey.IsEmpty())
	{
		OutJwt = FPlatformMisc::GetEnvironmentVariable(*ServiceJwtEnvironmentKey);
	}
	if (OutJwt.IsEmpty() && !ServiceJwtPath.IsEmpty())
	{
		if (!FFileHelper::LoadFileToString(OutJwt, *ServiceJwtPath))
		{
			OutError = TEXT("Service JWT file could not be read.");
			return false;
		}
	}
	OutJwt.TrimStartAndEndInline();
	if (OutJwt.IsEmpty())
	{
		OutError = TEXT("Service JWT is unavailable from the configured environment variable or file.");
		return false;
	}
	return true;
}

bool FFrontierInternalApiConfig::Validate(FString& OutError) const
{
	OutError.Reset();
	if (!InternalApiBaseUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
	{
		OutError = TEXT("InternalApiBaseUrl must be an explicitly configured HTTPS URL.");
		return false;
	}
	if (ServiceName.IsEmpty() || MtlsCertificatePath.IsEmpty() || MtlsPrivateKeyPath.IsEmpty()
		|| MtlsCaCertificatePath.IsEmpty() || RequestTimeoutSeconds <= 0.0f
		|| ServiceTokenLifetimeSeconds < 60 || ServiceTokenLifetimeSeconds > 3600
		|| MaximumRetryAttempts < 1 || RetryBaseDelaySeconds < 0.0f)
	{
		OutError = TEXT("Internal API service, mTLS, timeout, or retry configuration is incomplete.");
		return false;
	}
	if (!FPaths::FileExists(MtlsCertificatePath) || !FPaths::FileExists(MtlsPrivateKeyPath)
		|| !FPaths::FileExists(MtlsCaCertificatePath))
	{
		OutError = TEXT("One or more configured mTLS files do not exist.");
		return false;
	}
	return true;
}

FString FFrontierInternalApiConfig::BuildInternalUrl(const FString& Endpoint) const
{
	FString BaseUrl = InternalApiBaseUrl;
	while (BaseUrl.EndsWith(TEXT("/")))
	{
		BaseUrl.LeftChopInline(1);
	}
	return Endpoint.StartsWith(TEXT("/"))
		? BaseUrl + Endpoint
		: BaseUrl + TEXT("/") + Endpoint;
}

FString FFrontierInternalApiConfig::BuildServiceTokenUrl() const
{
	return BuildInternalUrl(TEXT("/internal-auth/token"));
}

FString FFrontierInternalApiConfig::BuildJoinAuthorizationUrl(const FString& RaidSessionId) const
{
	return BuildInternalUrl(FString::Printf(
		TEXT("/v1/internal/raids/%s/join-authorizations"),
		*FGenericPlatformHttp::UrlEncode(RaidSessionId)));
}

FString FFrontierInternalApiConfig::BuildRaidResultCommitUrl(const FString& RaidSessionId) const
{
	return BuildInternalUrl(FString::Printf(
		TEXT("/v1/internal/raids/%s/results"),
		*FGenericPlatformHttp::UrlEncode(RaidSessionId)));
}

FString FFrontierInternalApiConfig::BuildRaidExtractUrl(const FString& RaidSessionId) const
{
	return BuildInternalUrl(FString::Printf(
		TEXT("/v1/internal/raids/%s/extract"),
		*FGenericPlatformHttp::UrlEncode(RaidSessionId)));
}

FString FFrontierInternalApiConfig::BuildExperienceGrantUrl(const FString& PlayerId) const
{
	return BuildInternalUrl(FString::Printf(
		TEXT("/v1/internal/players/%s/experience-grants"),
		*FGenericPlatformHttp::UrlEncode(PlayerId)));
}

FString FFrontierInternalApiConfig::BuildRaidLootBatchUrl(const FString& InMatchId) const
{
	return BuildInternalUrl(FString::Printf(
		TEXT("/v1/internal/raids/%s/loot-batches"),
		*FGenericPlatformHttp::UrlEncode(InMatchId)));
}

FString FFrontierInternalApiConfig::BuildRaidLootRefillUrl(const FString& InMatchId) const
{
	return BuildInternalUrl(FString::Printf(
		TEXT("/v1/internal/raids/%s/loot-batches/refill"),
		*FGenericPlatformHttp::UrlEncode(InMatchId)));
}

FString FFrontierInternalApiConfig::BuildRaidServerReadyUrl(const FString& InRaidServerId) const
{
	return BuildInternalUrl(FString::Printf(
		TEXT("/v1/internal/raid-servers/%s/ready"),
		*FGenericPlatformHttp::UrlEncode(InRaidServerId)));
}

FString FFrontierInternalApiConfig::BuildRaidServerFailureUrl(const FString& InRaidServerId) const
{
	return BuildInternalUrl(FString::Printf(
		TEXT("/v1/internal/raid-servers/%s/failed"),
		*FGenericPlatformHttp::UrlEncode(InRaidServerId)));
}
