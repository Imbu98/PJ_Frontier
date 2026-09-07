#pragma once

#include "CoreMinimal.h"
#include "Equipment/FrontierOnlineEquipmentTypes.h"
#include "FrontierOnlineConfig.h"
#include "HttpFwd.h"
#include "Inventory/FrontierOnlineInventoryTypes.h"
#include "Inventory/FrontierOnlineItemUpgradeTypes.h"
#include "Matchmaking/FrontierOnlineMatchmakingTypes.h"
#include "Party/FrontierOnlinePartyTypes.h"
#include "Player/FrontierOnlineBootstrapTypes.h"
#include "Player/FrontierOnlineLevelTypes.h"
#include "Raid/FrontierOnlineRaidTypes.h"
#include "Storage/FrontierOnlineStorageTypes.h"

struct FRONTIERONLINE_API FFrontierOnlineSessionInfo
{
	FString SessionId;
	FString PlayerIdString;
	int64 PlayerId = 0;
	FString Platform;
	FString ExpiresAt;
};

struct FRONTIERONLINE_API FFrontierOnlineTokenInfo
{
	FString TokenType;
	FString AccessToken;
	FString AccessTokenExpiresAt;
	FString RefreshToken;
	FString RefreshTokenExpiresAt;
};

struct FRONTIERONLINE_API FFrontierOnlinePlayerInfo
{
	FString PlayerIdString;
	int64 PlayerId = 0;
	FString SteamId;
	FString Nickname;
};

struct FRONTIERONLINE_API FFrontierOnlineSteamLoginResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	FFrontierOnlineSessionInfo Session;
	FFrontierOnlineTokenInfo Tokens;
	FFrontierOnlinePlayerInfo Player;
	bool bIsNewPlayer = false;
	FString RequestId;
	FString ServerTime;
	FString Message;
};

using FFrontierOnlineSteamLoginCompletion = TFunction<void(const FFrontierOnlineSteamLoginResponse&)>;

struct FRONTIERONLINE_API FFrontierOnlineProfileResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	int64 PlayerId = 0;
	FString DisplayName;
	FString Locale;
	FString RequestId;
	FString ServerTime;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

using FFrontierOnlineProfileCompletion = TFunction<void(const FFrontierOnlineProfileResponse&)>;

struct FRONTIERONLINE_API FFrontierOnlineRefreshResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	FFrontierOnlineSessionInfo Session;
	FFrontierOnlineTokenInfo Tokens;
	FString RequestId;
	FString ServerTime;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

using FFrontierOnlineRefreshCompletion = TFunction<void(const FFrontierOnlineRefreshResponse&)>;

struct FRONTIERONLINE_API FFrontierOnlineLogoutResponse
{
	bool bTransportSucceeded = false;
	bool bSuccess = false;
	int32 HttpStatus = 0;
	bool bRevoked = false;
	int32 RevokedSessionCount = 0;
	FString RequestId;
	FString ServerTime;
	FString ErrorCode;
	FString Message;
	bool bRetryable = false;
};

using FFrontierOnlineLogoutCompletion = TFunction<void(const FFrontierOnlineLogoutResponse&)>;

/** HTTP/JSON boundary with no dependency on Frontier gameplay or UI modules. */
class FRONTIERONLINE_API FFrontierOnlineHttpClient : public TSharedFromThis<FFrontierOnlineHttpClient>
{
public:
	explicit FFrontierOnlineHttpClient(FFrontierOnlineConfig InConfig);

	bool LoginWithSteamTicket(
		const FString& SteamTicket,
		FFrontierOnlineSteamLoginCompletion Completion,
		FString& OutError);

	bool RefreshSession(
		const FString& RefreshToken,
		const FString& SessionId,
		FFrontierOnlineRefreshCompletion Completion,
		FString& OutError);

	bool LogoutSession(
		const FString& AccessToken,
		const FString& RefreshToken,
		bool bAllSessions,
		FFrontierOnlineLogoutCompletion Completion,
		FString& OutError);

	bool UpdateMyProfile(
		const FString& AccessToken,
		const FString& DisplayName,
		const FString& Locale,
		const FString& IdempotencyKey,
		FFrontierOnlineProfileCompletion Completion,
		FString& OutError);

	bool GetMyProfile(
		const FString& AccessToken,
		FFrontierOnlineProfileCompletion Completion,
		FString& OutError);

	bool CreateParty(
		const FString& AccessToken,
		const FString& SteamLobbyId,
		const FString& IdempotencyKey,
		FFrontierOnlinePartyCompletion Completion,
		FString& OutError);

	bool JoinParty(
		const FString& AccessToken,
		const FString& PartyId,
		const FString& SteamLobbyId,
		const FString& IdempotencyKey,
		FFrontierOnlinePartyCompletion Completion,
		FString& OutError);

	bool GetParty(
		const FString& AccessToken,
		const FString& PartyId,
		FFrontierOnlinePartyCompletion Completion,
		FString& OutError);

	bool GetMyParty(
		const FString& AccessToken,
		FFrontierOnlinePartyCompletion Completion,
		FString& OutError);

	bool LeaveParty(
		const FString& AccessToken,
		const FString& PartyId,
		FFrontierOnlineLeavePartyCompletion Completion,
		FString& OutError);

	bool DisbandParty(
		const FString& AccessToken,
		const FString& PartyId,
		FFrontierOnlinePartyCompletion Completion,
		FString& OutError);

	bool GetInventory(
		const FString& AccessToken,
		FFrontierOnlineInventoryCompletion Completion,
		FString& OutError);

	bool GetStorage(
		const FString& AccessToken,
		FFrontierOnlineStorageCompletion Completion,
		FString& OutError);

	bool DepositStorageItem(
		const FString& AccessToken,
		const FString& ItemInstanceId,
		const TOptional<int32>& TargetSlotIndex,
		const TOptional<int32>& Quantity,
		FFrontierOnlineStorageTransferCompletion Completion,
		FString& OutError);

	bool WithdrawStorageItem(
		const FString& AccessToken,
		const FString& ItemInstanceId,
		const TOptional<int32>& TargetSlotIndex,
		const TOptional<int32>& Quantity,
		FFrontierOnlineStorageTransferCompletion Completion,
		FString& OutError);

	bool MoveStorageItem(
		const FString& AccessToken,
		const FString& ItemInstanceId,
		int32 TargetSlotIndex,
		const TOptional<int32>& Quantity,
		FFrontierOnlineStorageCompletion Completion,
		FString& OutError);

	bool GetLobbyBootstrap(
		const FString& AccessToken,
		FFrontierOnlineLobbyBootstrapCompletion Completion,
		FString& OutError);

	bool GetPlayerLevel(
		const FString& AccessToken,
		FFrontierOnlinePlayerLevelCompletion Completion,
		FString& OutError);

	bool CreateRaidEntry(
		const FString& AccessToken,
		const FFrontierOnlineCreateRaidEntryRequest& EntryRequest,
		FFrontierOnlineRaidEntryCompletion Completion,
		FString& OutError);

	bool GetRaidResult(
		const FString& AccessToken,
		const FString& RaidSessionId,
		bool bIncludeSnapshot,
		FFrontierOnlineRaidResultCompletion Completion,
		FString& OutError);

	bool MoveInventoryItem(
		const FString& AccessToken,
		const FString& ItemInstanceId,
		int32 TargetSlotIndex,
		const TOptional<int32>& Quantity,
		FFrontierOnlineInventoryCompletion Completion,
		FString& OutError);

	bool GetEquipment(
		const FString& AccessToken,
		FFrontierOnlineEquipmentCompletion Completion,
		FString& OutError);

	bool EquipItem(
		const FString& AccessToken,
		const FString& ItemInstanceId,
		const FString& SlotType,
		bool bReplaceExisting,
		const TOptional<int32>& ReplacementTargetInventorySlot,
		FFrontierOnlineEquipmentChangeCompletion Completion,
		FString& OutError);

	bool UnequipItem(
		const FString& AccessToken,
		const FString& SlotType,
		const TOptional<int32>& TargetInventorySlotIndex,
		FFrontierOnlineEquipmentChangeCompletion Completion,
		FString& OutError);

	const FFrontierOnlineConfig& GetConfig() const { return Config; }
	bool IsRequestInProgress() const { return bRequestInProgress; }

	static bool ParseSteamLoginResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlineSteamLoginResponse& OutResponse,
		FString& OutError);

	bool UpgradeInventoryItem(
		const FString& AccessToken,
		const FString& ItemInstanceId,
		int32 ExpectedEnhancementLevel,
		const FString& IdempotencyKey,
		FFrontierOnlineItemUpgradeCompletion Completion,
		FString& OutError);

	static bool ParseProfileResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlineProfileResponse& OutResponse,
		FString& OutError);

	static bool ParseInventoryResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlineInventoryResponse& OutResponse,
		FString& OutError);

	bool CreateMatchmakingTicket(
		const FString& AccessToken,
		const FFrontierOnlineCreateMatchmakingTicketRequest& TicketRequest,
		FFrontierOnlineMatchmakingTicketCompletion Completion,
		FString& OutError);

	bool GetMatchmakingTicket(
		const FString& AccessToken,
		const FString& TicketId,
		FFrontierOnlineMatchmakingTicketCompletion Completion,
		FString& OutError);

	static bool ParseItemUpgradeResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlineItemUpgradeResponse& OutResponse,
		FString& OutError);

	bool CancelMatchmakingTicket(
		const FString& AccessToken,
		const FString& TicketId,
		FFrontierOnlineMatchmakingTicketCompletion Completion,
		FString& OutError);

	static bool ParsePartyResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlinePartyResponse& OutResponse,
		FString& OutError);

	static bool ParseLeavePartyResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlineLeavePartyResponse& OutResponse,
		FString& OutError);

	static bool ParseStorageResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlineStorageResponse& OutResponse,
		FString& OutError);

	static bool ParseStorageTransferResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlineStorageTransferResponse& OutResponse,
		FString& OutError);

	static bool ParseLobbyBootstrapResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlineLobbyBootstrapResponse& OutResponse,
		FString& OutError);

	static bool ParseRefreshResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlineRefreshResponse& OutResponse,
		FString& OutError);

	static bool ParseLogoutResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlineLogoutResponse& OutResponse,
		FString& OutError);

	static bool ParsePlayerLevelResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlinePlayerLevelResponse& OutResponse,
		FString& OutError);

	static bool ParseRaidEntryResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlineRaidEntryResponse& OutResponse,
		FString& OutError);

	static bool ParseMatchmakingTicketResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlineMatchmakingTicketResponse& OutResponse,
		FString& OutError);

	static bool ParseJoinAuthorizationResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlineJoinAuthorizationResponse& OutResponse,
		FString& OutError);

	static bool ParseRaidResultResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlineRaidResultResponse& OutResponse,
		FString& OutError);

	static bool ParseRaidCommitResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		bool bExtractResponse,
		FFrontierOnlineRaidCommitResponse& OutResponse,
		FString& OutError);

	static bool ParseExperienceGrantResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlineExperienceGrantResponse& OutResponse,
		FString& OutError);

	static bool ParseEquipmentResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlineEquipmentResponse& OutResponse,
		FString& OutError);

	static bool ParseEquipmentChangeResponse(
		int32 HttpStatus,
		const FString& ResponseBody,
		FFrontierOnlineEquipmentChangeResponse& OutResponse,
		FString& OutError);

private:
	void HandleSteamLoginResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FFrontierOnlineSteamLoginCompletion Completion);

	void HandleRefreshResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FFrontierOnlineRefreshCompletion Completion);

	void HandleLogoutResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FFrontierOnlineLogoutCompletion Completion);

	void HandleProfileResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FFrontierOnlineProfileCompletion Completion);

	void HandleInventoryResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FFrontierOnlineInventoryCompletion Completion);

	void HandleItemUpgradeResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FFrontierOnlineItemUpgradeCompletion Completion);

	void HandlePartyResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FFrontierOnlinePartyCompletion Completion);

	void HandleLeavePartyResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FFrontierOnlineLeavePartyCompletion Completion);

	void HandleStorageResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FFrontierOnlineStorageCompletion Completion);

	void HandleStorageTransferResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FFrontierOnlineStorageTransferCompletion Completion);

	void HandleLobbyBootstrapResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FFrontierOnlineLobbyBootstrapCompletion Completion);

	void HandlePlayerLevelResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FFrontierOnlinePlayerLevelCompletion Completion);

	void HandleRaidEntryResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FFrontierOnlineRaidEntryCompletion Completion);

	void HandleMatchmakingTicketResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FFrontierOnlineMatchmakingTicketCompletion Completion);

	void HandleRaidResultResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FFrontierOnlineRaidResultCompletion Completion);

	void HandleEquipmentResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FFrontierOnlineEquipmentCompletion Completion);

	void HandleEquipmentChangeResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FFrontierOnlineEquipmentChangeCompletion Completion);

	FFrontierOnlineConfig Config;
	bool bRequestInProgress = false;
};
