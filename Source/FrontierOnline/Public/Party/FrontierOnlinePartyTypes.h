#pragma once

#include "CoreMinimal.h"

struct FRONTIERONLINE_API FFrontierOnlinePartyMemberDTO
{
	int64 PlayerId = 0;
	FString SteamId;
	bool bIsLeader = false;
};

struct FRONTIERONLINE_API FFrontierOnlinePartyDTO
{
	FString PartyId;
	FString SteamLobbyId;
	int64 LeaderPlayerId = 0;
	FString Status;
	TArray<FFrontierOnlinePartyMemberDTO> Members;
};

struct FRONTIERONLINE_API FFrontierOnlinePartyResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	bool bHasData = false;
	int32 HttpStatus = 0;
	FFrontierOnlinePartyDTO Data;
	FString RequestId;
	FString ServerTime;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

using FFrontierOnlinePartyCompletion = TFunction<void(const FFrontierOnlinePartyResponse&)>;

struct FRONTIERONLINE_API FFrontierOnlineLeavePartyDTO
{
	FString PartyId;
	int64 LeftPlayerId = 0;
	FString Status;
	int32 RemainingMemberCount = 0;
};

struct FRONTIERONLINE_API FFrontierOnlineLeavePartyResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	FFrontierOnlineLeavePartyDTO Data;
	FString RequestId;
	FString ServerTime;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

using FFrontierOnlineLeavePartyCompletion = TFunction<void(const FFrontierOnlineLeavePartyResponse&)>;
