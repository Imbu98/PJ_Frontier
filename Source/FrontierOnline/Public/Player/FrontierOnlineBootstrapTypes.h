#pragma once

#include "CoreMinimal.h"
#include "Equipment/FrontierOnlineEquipmentTypes.h"
#include "Inventory/FrontierOnlineInventoryTypes.h"
#include "Storage/FrontierOnlineStorageTypes.h"

struct FRONTIERONLINE_API FFrontierOnlineCurrencyDTO
{
	FString CurrencyCode;
	int64 Balance = 0;
};

struct FRONTIERONLINE_API FFrontierOnlineRaidSessionDTO
{
	FString RaidSessionId;
	int64 PlayerId = 0;
	FString State;
	bool bHasServerId = false;
	FString ServerId;
	FString CreatedAt;
	bool bHasStartedAt = false;
	FString StartedAt;
	bool bHasCompletedAt = false;
	FString CompletedAt;
};

struct FRONTIERONLINE_API FFrontierOnlineBootstrapPlayerInfo
{
	FString PlayerIdString;
	int64 PlayerId = 0;
	FString SteamId;
	FString Nickname;
};

struct FRONTIERONLINE_API FFrontierOnlineLobbyBootstrapData
{
	FFrontierOnlineBootstrapPlayerInfo Player;
	TArray<FFrontierOnlineCurrencyDTO> Currencies;
	FFrontierOnlineInventoryData Inventory;
	FFrontierOnlineStorageData Storage;
	FFrontierOnlineEquipmentData Equipment;
	bool bHasActiveRaid = false;
	FFrontierOnlineRaidSessionDTO ActiveRaid;
};

struct FRONTIERONLINE_API FFrontierOnlineLobbyBootstrapResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	FFrontierOnlineLobbyBootstrapData Data;
	FString RequestId;
	FString ServerTime;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

/**
 * Keep the asynchronous module boundary limited to wire data. The parsed bootstrap
 * DTO is intentionally constructed by the caller because it contains several
 * nested, non-reflected structs whose layout can change while iterating locally.
 */
using FFrontierOnlineLobbyBootstrapCompletion = TFunction<void(
	bool bTransportSucceeded,
	int32 HttpStatus,
	FString ResponseBody)>;
