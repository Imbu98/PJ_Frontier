#pragma once

#include "CoreMinimal.h"
#include "Equipment/FrontierOnlineEquipmentTypes.h"
#include "Inventory/FrontierOnlineInventoryTypes.h"
#include "Storage/FrontierOnlineStorageTypes.h"

struct FRONTIERONLINE_API FFrontierOnlineConsumedUpgradeMaterial
{
	FString ItemInstanceId;
	FString ItemTemplateId;
	int32 ConsumedAmount = 0;
	bool bHasRemainingQuantity = false;
	int32 RemainingQuantity = 0;
};

struct FRONTIERONLINE_API FFrontierOnlineUpgradeCurrencyChange
{
	FString CurrencyCode;
	int64 ConsumedAmount = 0;
	bool bHasBalance = false;
	int64 Balance = 0;
};

struct FRONTIERONLINE_API FFrontierOnlineItemUpgradeResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	bool bUpgradeSucceeded = false;
	FString FailureReason;
	int32 PreviousEnhancementLevel = 0;
	int32 CurrentEnhancementLevel = 0;
	FFrontierOnlineItemDTO Item;
	TArray<FFrontierOnlineConsumedUpgradeMaterial> ConsumedMaterials;
	TArray<FFrontierOnlineUpgradeCurrencyChange> CurrencyChanges;
	bool bHasInventory = false;
	FFrontierOnlineInventoryData Inventory;
	bool bHasStorage = false;
	FFrontierOnlineStorageData Storage;
	bool bHasEquipment = false;
	FFrontierOnlineEquipmentData Equipment;
	FString RequestId;
	FString ServerTime;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

using FFrontierOnlineItemUpgradeCompletion = TFunction<void(const FFrontierOnlineItemUpgradeResponse&)>;
