#pragma once

#include "CoreMinimal.h"

/** Dedicated-server-only settings. Secrets are loaded at runtime and are never written to logs. */
struct FRONTIER_API FFrontierInternalApiConfig
{
	FString InternalApiBaseUrl = TEXT("https://15.135.135.207:8443");
	FString ServiceJwtPath;
	FString ServiceJwtEnvironmentKey = TEXT("FRONTIER_SERVICE_JWT");
	FString MtlsCertificatePath = TEXT("C:/mtls/client.crt");
	FString MtlsPrivateKeyPath = TEXT("C:/mtls/client.key");
	FString MtlsCaCertificatePath = TEXT("C:/mtls/ca.crt");
	FString ServiceName = TEXT("frontier-dedicated-server");
	FString RaidServerId;
	FString MatchId;
	FString MapId;
	FString PublicServerAddress;
	int32 PublicServerPort = 0;
	int64 ServiceTokenLifetimeSeconds = 900;
	bool bRaidResultCommitAutomaticallyGrantsExperience = false;
	float RequestTimeoutSeconds = 15.0f;
	int32 MaximumRetryAttempts = 3;
	float RetryBaseDelaySeconds = 0.5f;

	static FFrontierInternalApiConfig Load();
	bool ResolveServiceJwt(FString& OutJwt, FString& OutError) const;
	bool Validate(FString& OutError) const;
	FString BuildInternalUrl(const FString& Endpoint) const;
	FString BuildServiceTokenUrl() const;
	FString BuildJoinAuthorizationUrl(const FString& RaidSessionId) const;
	FString BuildRaidResultCommitUrl(const FString& RaidSessionId) const;
	FString BuildRaidExtractUrl(const FString& RaidSessionId) const;
	FString BuildExperienceGrantUrl(const FString& PlayerId) const;
	FString BuildRaidLootBatchUrl(const FString& MatchId) const;
	FString BuildRaidLootRefillUrl(const FString& MatchId) const;
	FString BuildRaidServerReadyUrl(const FString& RaidServerId) const;
	FString BuildRaidServerFailureUrl(const FString& RaidServerId) const;
};
