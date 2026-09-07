#pragma once

#include "CoreMinimal.h"
#include "Raid/FrontierOnlineRaidTypes.h"

struct FRONTIER_API FFrontierRaidLootRequestEntry
{
	FString LootTableId;
	int32 RequestedCount = 0;
};

struct FRONTIER_API FFrontierRaidLootBatchRequest
{
	FString RaidServerId;
	FString MapId;
	FString GenerationReason;
	TArray<FFrontierRaidLootRequestEntry> LootRequests;
};

struct FRONTIER_API FFrontierRaidLootEntryDTO
{
	FString LootEntryId;
	TArray<FFrontierOnlineRaidRuntimeItemDTO> Items;
};

struct FRONTIER_API FFrontierRaidLootPoolDTO
{
	FString LootTableId;
	TArray<FFrontierRaidLootEntryDTO> Entries;
};

struct FRONTIER_API FFrontierRaidLootBatchData
{
	FString LootBatchId;
	FString MatchId;
	TArray<FFrontierRaidLootPoolDTO> LootPools;
};

struct FRONTIER_API FFrontierRaidLootBatchResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	bool bRetryable = false;
	int32 HttpStatus = 0;
	FFrontierRaidLootBatchData Data;
	FString RequestId;
	FString ErrorCode;
	FString Message;
};

struct FRONTIER_API FFrontierRaidServerReadyRequest
{
	FString RaidServerId;
	FString MatchId;
	FString MapId;
	FString ServerAddress;
	int32 Port = 0;
	bool bLootReady = false;
};

struct FRONTIER_API FFrontierRaidServerReadyResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	bool bRetryable = false;
	bool bAccepted = false;
	int32 HttpStatus = 0;
	FString RaidServerId;
	FString MatchId;
	FString Status;
	FString RequestId;
	FString ErrorCode;
	FString Message;
};

struct FRONTIER_API FFrontierRaidServerFailureRequest
{
	FString RaidServerId;
	FString MatchId;
	FString MapId;
	FString ErrorCode;
	FString Message;
};

struct FRONTIER_API FFrontierRaidLootJson
{
	static bool SerializeBatchRequest(
		const FFrontierRaidLootBatchRequest& Request,
		bool bIncludeMapId,
		FString& OutBody,
		FString& OutError);

	static bool ParseBatchResponse(
		const FString& Body,
		FFrontierRaidLootBatchResponse& OutResponse,
		FString& OutError);

	static bool SerializeReadyRequest(
		const FFrontierRaidServerReadyRequest& Request,
		FString& OutBody,
		FString& OutError);

	static bool SerializeFailureRequest(
		const FFrontierRaidServerFailureRequest& Request,
		FString& OutBody,
		FString& OutError);

	static bool ParseReadyResponse(
		const FString& Body,
		FFrontierRaidServerReadyResponse& OutResponse,
		FString& OutError);
};
