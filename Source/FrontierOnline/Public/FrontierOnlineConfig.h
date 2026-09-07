#pragma once

#include "CoreMinimal.h"

struct FRONTIERONLINE_API FFrontierOnlineConfig
{
	FString BackendBaseUrl;
	FString BackendWebSocketUrl;
	FString SteamAuthEndpoint;
	FString SteamIdentity;
	FString ClientVersion;
	FString Platform;

	static FFrontierOnlineConfig Load();
	bool IsValid(FString& OutError) const;
	FString BuildUrl(const FString& Endpoint) const;
};
