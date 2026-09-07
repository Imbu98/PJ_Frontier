#pragma once

#include "CoreMinimal.h"

struct FRONTIERONLINE_API FFrontierOnlineInventoryHeader
{
	FString InventoryId;
	int64 OwnerPlayerId = 0;
	int32 SlotCapacity = 0;
	int32 CapacityLevel = 0;
	FString UpdatedAt;
};

struct FRONTIERONLINE_API FFrontierOnlineItemOption
{
	FString OptionId;
	FString Unit;
	double BaseValue = 0.0;
	double RandomValue = 0.0;
	double UpgradeValue = 0.0;
	double FinalValue = 0.0;
	double NormalizedValue = 0.0;
	bool bHasNormalizedValue = false;
};

struct FRONTIERONLINE_API FFrontierOnlineGeneratedItemSkill
{
	FString SkillId;
	int32 Level = 1;
	int32 SlotIndex = INDEX_NONE;
};

struct FRONTIERONLINE_API FFrontierOnlineItemDTO
{
	FString ItemInstanceId;
	FString ItemTemplateId;
	int32 SlotIndex = INDEX_NONE;
	int32 Quantity = 1;
	bool bHasDurability = false;
	double Durability = 0.0;
	int32 EnhancementLevel = 0;
	FString FinalRarityTag;
	FString BindState;
	TArray<FString> InstanceTags;
	TArray<FFrontierOnlineItemOption> RandomOptions;
	TArray<FFrontierOnlineGeneratedItemSkill> GeneratedSkills;
	FString CreatedAt;
	FString AcquiredAt;
	FString UpdatedAt;
	bool bHasMetadata = false;
	FString MetadataJson;
};

struct FRONTIERONLINE_API FFrontierOnlineInventoryCurrencyCost
{
	FString CurrencyCode;
	int64 Amount = 0;
};

struct FRONTIERONLINE_API FFrontierOnlineInventoryUpgrade
{
	int32 CapacityLevel = 0;
	int32 MaxCapacityLevel = 0;
	bool bHasNextCapacityLevel = false;
	int32 NextCapacityLevel = 0;
	bool bHasNextSlotCapacity = false;
	int32 NextSlotCapacity = 0;
	TArray<FFrontierOnlineInventoryCurrencyCost> NextUpgradeCost;
};

struct FRONTIERONLINE_API FFrontierOnlineInventoryData
{
	FFrontierOnlineInventoryHeader Container;
	TArray<FFrontierOnlineItemDTO> Slots;
	FFrontierOnlineInventoryUpgrade Upgrade;
};

struct FRONTIERONLINE_API FFrontierOnlineInventoryResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	FFrontierOnlineInventoryData Data;
	FString RequestId;
	FString ServerTime;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

using FFrontierOnlineInventoryCompletion = TFunction<void(const FFrontierOnlineInventoryResponse&)>;
