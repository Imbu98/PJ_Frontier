#pragma once

#include "CoreMinimal.h"

/** UTF-8 JSON body delivered through /user/queue/matchmaking. */
struct FRONTIERONLINE_API FFrontierOnlineMatchmakingStatusEvent
{
	FString Type;
	FString TicketId;
	FString TicketStatus;
	FString MatchId;
	FString MatchStatus;
	int32 CurrentPlayerCount = 0;
	int32 MaximumPlayerCount = 0;
	FString TeamSide;
};

/** SERVER_READY notification delivered through /user/queue/matchmaking. */
struct FRONTIERONLINE_API FFrontierOnlineRaidServerReadyEvent
{
	FString Type;
	FString TicketId;
	FString MatchId;
	FString RaidServerId;
	FString MatchStatus;
};

struct FRONTIERONLINE_API FFrontierOnlineMatchmakingFailedEvent
{
	FString Type;
	FString TicketId;
	FString Status;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

struct FRONTIERONLINE_API FFrontierOnlineCreateMatchmakingTicketRequest
{
	FString MatchMode = TEXT("SOLO");
	bool bHasPartyId = false;
	FString PartyId;
	FString RaidDefinitionId = TEXT("Raid.Factory.Standard");
	FString MapId;
	FString Region = TEXT("ap-northeast-2");
};

struct FRONTIERONLINE_API FFrontierOnlineMatchmakingTicketDTO
{
	FString TicketId;
	FString MatchMode;
	bool bHasPartyId = false;
	FString PartyId;
	FString Status;
	bool bHasMatchId = false;
	FString MatchId;
	bool bHasMatchStatus = false;
	FString MatchStatus;
	int32 CurrentPlayerCount = 0;
	int32 MaximumPlayerCount = 0;
	int32 MaximumWaitSeconds = 0;
};

struct FRONTIERONLINE_API FFrontierOnlineMatchmakingTicketResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	bool bRetryable = false;
	int32 HttpStatus = 0;
	FFrontierOnlineMatchmakingTicketDTO Data;
	FString RequestId;
	FString ServerTime;
	FString ErrorCode;
	FString Message;
};

using FFrontierOnlineMatchmakingTicketCompletion =
	TFunction<void(const FFrontierOnlineMatchmakingTicketResponse&)>;
