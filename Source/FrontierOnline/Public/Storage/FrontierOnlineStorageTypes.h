#pragma once

#include "CoreMinimal.h"
#include "Inventory/FrontierOnlineInventoryTypes.h"

struct FRONTIERONLINE_API FFrontierOnlineStorageHeader
{
	FString StorageId;
	int64 OwnerPlayerId = 0;
	int32 SlotCapacity = 0;
	int32 CapacityLevel = 0;
	FString UpdatedAt;
};

struct FRONTIERONLINE_API FFrontierOnlineStorageCurrencyCost
{
	FString CurrencyCode;
	int64 Amount = 0;
};

struct FRONTIERONLINE_API FFrontierOnlineStorageUpgrade
{
	int32 CapacityLevel = 0;
	int32 MaxCapacityLevel = 0;
	bool bHasNextCapacityLevel = false;
	int32 NextCapacityLevel = 0;
	bool bHasNextSlotCapacity = false;
	int32 NextSlotCapacity = 0;
	TArray<FFrontierOnlineStorageCurrencyCost> NextUpgradeCost;
};

struct FRONTIERONLINE_API FFrontierOnlineStorageData
{
	FFrontierOnlineStorageHeader Container;
	TArray<FFrontierOnlineItemDTO> Slots;
	FFrontierOnlineStorageUpgrade Upgrade;
};

struct FRONTIERONLINE_API FFrontierOnlineStorageResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	FFrontierOnlineStorageData Data;
	FString RequestId;
	FString ServerTime;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

using FFrontierOnlineStorageCompletion = TFunction<void(const FFrontierOnlineStorageResponse&)>;

struct FRONTIERONLINE_API FFrontierOnlineStorageTransferData
{
	FFrontierOnlineInventoryData Inventory;
	FFrontierOnlineStorageData Storage;
};

struct FRONTIERONLINE_API FFrontierOnlineStorageTransferResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	FFrontierOnlineStorageTransferData Data;
	FString RequestId;
	FString ServerTime;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

using FFrontierOnlineStorageTransferCompletion = TFunction<void(const FFrontierOnlineStorageTransferResponse&)>;
