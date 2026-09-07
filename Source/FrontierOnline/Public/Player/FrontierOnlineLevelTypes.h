#pragma once

#include "CoreMinimal.h"

/** Wire-compatible representation of Backend PlayerLevelDTO. */
struct FRONTIERONLINE_API FFrontierOnlinePlayerLevelDTO
{
	int32 Level = 0;
	int64 TotalExperience = 0;
	int64 CurrentLevelExperience = 0;
	bool bHasNextLevelRequiredExperience = false;
	int64 NextLevelRequiredExperience = 0;
	int32 MaxLevel = 0;
	FString UpdatedAt;
};

struct FRONTIERONLINE_API FFrontierOnlinePlayerLevelResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	FFrontierOnlinePlayerLevelDTO Data;
	FString RequestId;
	FString ServerTime;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

using FFrontierOnlinePlayerLevelCompletion = TFunction<void(const FFrontierOnlinePlayerLevelResponse&)>;
