#pragma once

#include "CoreMinimal.h"
#include "Inventory/FrontierOnlineInventoryTypes.h"

struct FRONTIERONLINE_API FFrontierOnlineEquipmentHeader
{
	FString EquipmentId;
	int64 OwnerPlayerId = 0;
};

struct FRONTIERONLINE_API FFrontierOnlineEquipmentSlot
{
	FString SlotType;
	bool bHasItem = false;
	FFrontierOnlineItemDTO Item;
};

struct FRONTIERONLINE_API FFrontierOnlineEquipmentData
{
	FFrontierOnlineEquipmentHeader Container;
	TArray<FFrontierOnlineEquipmentSlot> Slots;
};

struct FRONTIERONLINE_API FFrontierOnlineEquipmentResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	FFrontierOnlineEquipmentData Data;
	FString RequestId;
	FString ServerTime;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

struct FRONTIERONLINE_API FFrontierOnlineEquipmentChangeResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	FFrontierOnlineInventoryData Inventory;
	FFrontierOnlineEquipmentData Equipment;
	FString RequestId;
	FString ServerTime;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

using FFrontierOnlineEquipmentCompletion = TFunction<void(const FFrontierOnlineEquipmentResponse&)>;
using FFrontierOnlineEquipmentChangeCompletion = TFunction<void(const FFrontierOnlineEquipmentChangeResponse&)>;
