#pragma once

#include "CoreMinimal.h"
#include "Equipment/FrontierOnlineEquipmentTypes.h"
#include "Inventory/FrontierOnlineInventoryTypes.h"
#include "Player/FrontierOnlineBootstrapTypes.h"
#include "Player/FrontierOnlineLevelTypes.h"

struct FRONTIERONLINE_API FFrontierOnlineRaidRuntimeItemDTO
{
	FString RaidItemId;
	bool bHasOriginItemInstanceId = false;
	FString OriginItemInstanceId;
	FString ItemTemplateId;
	int32 Quantity = 0;
	bool bHasDurability = false;
	double Durability = 0.0;
	int32 EnhancementLevel = 0;
	FString FinalRarityTag;
	FString RandomOptionsJson;
	FString GeneratedSkillsJson;
	bool bHasLootSourceId = false;
	FString LootSourceId;
};

struct FRONTIERONLINE_API FFrontierOnlineRaidInventorySlotDTO
{
	int32 SlotIndex = INDEX_NONE;
	FFrontierOnlineRaidRuntimeItemDTO Item;
};

struct FRONTIERONLINE_API FFrontierOnlineRaidEquipmentSlotDTO
{
	FString SlotType;
	FFrontierOnlineRaidRuntimeItemDTO Item;
};

struct FRONTIERONLINE_API FFrontierOnlineRaidLoadoutManifestDTO
{
	TArray<FFrontierOnlineRaidInventorySlotDTO> InventorySlots;
	TArray<FFrontierOnlineRaidEquipmentSlotDTO> EquipmentSlots;
};

struct FRONTIERONLINE_API FFrontierOnlineCreateRaidEntryRequest
{
	FString RaidDefinitionId = TEXT("Raid.Factory.Standard");
	FString Region = TEXT("ap-northeast-2");
	FString MapId;
	bool bHasPartyId = false;
	FString PartyId;
	bool bHasTicketId = false;
	FString TicketId;
	bool bHasMatchId = false;
	FString MatchId;
};

struct FRONTIERONLINE_API FFrontierOnlineRaidEntryDTO
{
	FFrontierOnlineRaidSessionDTO RaidSession;
	FString JoinToken;
	FString JoinTokenExpiresAt;
	FString ServerEndpoint;
	FString Transport;
};

struct FRONTIERONLINE_API FFrontierOnlineRaidEntryResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	FFrontierOnlineRaidEntryDTO Data;
	FString RequestId;
	FString ServerTime;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

using FFrontierOnlineRaidEntryCompletion = TFunction<void(const FFrontierOnlineRaidEntryResponse&)>;

struct FRONTIERONLINE_API FFrontierOnlineJoinAuthorizationResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	FFrontierOnlineRaidSessionDTO RaidSession;
	int64 PlayerId = 0;
	FString SteamId;
	FString DisplayName;
	FFrontierOnlineRaidLoadoutManifestDTO LoadoutManifest;
	FString RequestId;
	FString ServerTime;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

struct FRONTIERONLINE_API FFrontierOnlineRaidResultDTO
{
	FString RaidSessionId;
	int64 PlayerId = 0;
	FString Outcome;
	bool bHasReasonCode = false;
	FString ReasonCode;
	bool bHasCommittedAt = false;
	FString CommittedAt;
};

struct FRONTIERONLINE_API FFrontierOnlineRaidResultResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	FFrontierOnlineRaidResultDTO Result;
	bool bHasInventory = false;
	FFrontierOnlineInventoryData Inventory;
	bool bHasEquipment = false;
	FFrontierOnlineEquipmentData Equipment;
	bool bHasRetryAfterSeconds = false;
	int32 RetryAfterSeconds = 0;
	FString RequestId;
	FString ServerTime;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

using FFrontierOnlineRaidResultCompletion = TFunction<void(const FFrontierOnlineRaidResultResponse&)>;

struct FRONTIERONLINE_API FFrontierOnlineMintedRaidItemDTO
{
	FString RaidItemId;
	FString ItemInstanceId;
};

struct FRONTIERONLINE_API FFrontierOnlineRaidCommitResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	FFrontierOnlineRaidResultDTO Result;
	bool bHasInventory = false;
	FFrontierOnlineInventoryData Inventory;
	bool bHasEquipment = false;
	FFrontierOnlineEquipmentData Equipment;
	bool bHasLevel = false;
	FFrontierOnlinePlayerLevelDTO Level;
	TArray<FFrontierOnlineMintedRaidItemDTO> MintedItems;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

struct FRONTIERONLINE_API FFrontierOnlineExperienceGrantResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	int32 PreviousLevel = 0;
	FFrontierOnlinePlayerLevelDTO Level;
	TArray<int32> LevelUps;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};
