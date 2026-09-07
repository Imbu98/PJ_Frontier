#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/FrontierBackendProtocolComponent.h"
#include "Components/FrontierLobbyBootstrapJsonParser.h"
#include "FrontierOnlineConfig.h"
#include "FrontierOnlineHttpClient.h"
#include "Game/FrontierSteamSubsystem.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierOnlineModuleBoundaryTest,
	"Frontier.Architecture.Online.ModuleBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierOnlineModuleBoundaryTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("FrontierOnline runtime module is loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("FrontierOnline")));

	const FFrontierOnlineConfig Config = FFrontierOnlineConfig::Load();
	FString ConfigError;
	TestTrue(TEXT("FrontierOnline deployment configuration is valid"), Config.IsValid(ConfigError));
	TestTrue(*FString::Printf(TEXT("Configuration error is empty: %s"), *ConfigError), ConfigError.IsEmpty());
	TestEqual(TEXT("Steam ticket identity remains compatible"), Config.SteamIdentity, FString(TEXT("frontier-backend")));
	TestEqual(
		TEXT("Auth URL joins without duplicate separators"),
		Config.BuildUrl(Config.SteamAuthEndpoint),
		Config.BackendBaseUrl + TEXT("/v1/auth/steam/login"));

	const FString ResponseBody = TEXT(
		"{"
		"\"success\":true,"
		"\"message\":\"ok\","
		"\"data\":{"
			"\"session\":{\"sessionId\":\"session-1\",\"playerId\":42,\"platform\":\"STEAM\",\"expiresAt\":\"2026-07-15T01:00:00Z\"},"
			"\"tokens\":{\"tokenType\":\"Bearer\",\"accessToken\":\"access\",\"accessTokenExpiresAt\":\"2026-07-15T00:15:00Z\",\"refreshToken\":\"refresh\",\"refreshTokenExpiresAt\":\"2026-08-15T00:00:00Z\"},"
			"\"player\":{\"playerId\":42,\"steamId\":\"76561198000000000\",\"nickname\":\"FrontierPlayer\"},"
			"\"isNewPlayer\":false"
		"},"
		"\"meta\":{\"requestId\":\"request-1\",\"serverTime\":\"2026-07-15T00:00:00Z\"}"
		"}");

	FFrontierOnlineSteamLoginResponse ParsedResponse;
	FString ParseError;
	TestTrue(
		TEXT("FrontierOnline parses the existing Steam login contract"),
		FFrontierOnlineHttpClient::ParseSteamLoginResponse(200, ResponseBody, ParsedResponse, ParseError));
	TestTrue(TEXT("Parsed response succeeds"), ParsedResponse.bSuccess);
	TestEqual(TEXT("Session is preserved"), ParsedResponse.Session.SessionId, FString(TEXT("session-1")));
	TestEqual(TEXT("Player ID is preserved"), ParsedResponse.Player.PlayerId, static_cast<int64>(42));
	TestEqual(TEXT("Request ID is preserved"), ParsedResponse.RequestId, FString(TEXT("request-1")));
	TestTrue(TEXT("Parse error remains empty"), ParseError.IsEmpty());

	FFrontierOnlineSteamLoginResponse RejectedResponse;
	FString RejectedError;
	TestFalse(
		TEXT("HTTP authentication failure is rejected"),
		FFrontierOnlineHttpClient::ParseSteamLoginResponse(
			401,
			TEXT("{\"error\":{\"message\":\"ticket expired\"}}"),
			RejectedResponse,
			RejectedError));
	TestEqual(
		TEXT("Existing authentication failure guidance remains compatible"),
		RejectedError,
		FString(TEXT("Steam 인증이 거부되었습니다. 인증 티켓이 만료되었거나 서버가 요구하는 티켓 형식과 다릅니다. (ticket expired)")));

	const FString InventoryResponseBody = TEXT(
		"{"
		"\"success\":true,"
		"\"data\":{"
			"\"schemaVersion\":1,"
			"\"container\":{\"containerId\":\"0190f7e4-b088-7608-a30d-0fcb9ceff8fd\",\"ownerPlayerId\":42,\"containerType\":\"RAID_INVENTORY\",\"slotCapacity\":30,\"capacityLevel\":0,\"updatedAt\":\"2026-07-15T00:00:00Z\"},"
			"\"slots\":[{"
				"\"schemaVersion\":1,"
				"\"itemInstanceId\":\"0190f7e4-b088-7608-a30d-0fcb9ceff8fd\","
				"\"itemTemplateId\":\"weapon_rifle_001\","
				"\"slotIndex\":2,"
				"\"quantity\":1,"
				"\"durability\":87.5,"
				"\"enhancementLevel\":2,"
				"\"finalRarityTag\":\"EPIC\","
				"\"bindState\":\"UNBOUND\","
				"\"instanceTags\":[\"Item.Type.Equipment\"],"
				"\"randomOptions\":[{\"statTag\":\"Stat.AttackPower\",\"baseValue\":10,\"rolledBonus\":3,\"enhancementBonus\":2,\"reforgeBonus\":1,\"finalValue\":16,\"score\":5}],"
				"\"generatedSkills\":[{\"skillTag\":\"Ability.Attack.Primary\",\"skillRarityTag\":\"Item.Rarity.Rare\",\"advanced\":true,\"slotIndex\":0}],"
				"\"createdAt\":\"2026-07-15T00:00:00Z\","
				"\"acquiredAt\":\"2026-07-15T00:00:00Z\","
				"\"updatedAt\":\"2026-07-15T00:01:00Z\","
				"\"metadata\":{\"event\":{\"season\":1}}"
			"}],"
			"\"upgrade\":{\"capacityLevel\":0,\"maxCapacityLevel\":3,\"nextCapacityLevel\":1,\"nextSlotCapacity\":40,\"nextUpgradeCost\":[]}"
		"},"
		"\"meta\":{\"requestId\":\"request-inventory-1\",\"serverTime\":\"2026-07-15T00:00:00Z\",\"apiVersion\":\"v1\"}"
		"}");

	FFrontierOnlineInventoryResponse ParsedInventoryResponse;
	FString InventoryParseError;
	TestTrue(
		TEXT("Inventory parser reads explicit item options and generated skills"),
		FFrontierOnlineHttpClient::ParseInventoryResponse(
			200,
			InventoryResponseBody,
			ParsedInventoryResponse,
			InventoryParseError));
	TestTrue(TEXT("Inventory parse error remains empty"), InventoryParseError.IsEmpty());
	TestEqual(TEXT("One occupied backend slot is parsed"), ParsedInventoryResponse.Data.Slots.Num(), 1);
	if (ParsedInventoryResponse.Data.Slots.Num() == 1)
	{
		const FFrontierOnlineItemDTO& ParsedItem = ParsedInventoryResponse.Data.Slots[0];
		TestEqual(TEXT("Explicit randomOptions are parsed"), ParsedItem.RandomOptions.Num(), 1);
		TestEqual(TEXT("Explicit generatedSkills are parsed"), ParsedItem.GeneratedSkills.Num(), 1);
		if (ParsedItem.RandomOptions.Num() == 1)
		{
			TestEqual(TEXT("Final option value is preserved"), ParsedItem.RandomOptions[0].FinalValue, 16.0);
		}
	}

	const FString CompactInventoryResponseBody = TEXT(
		"{"
		"\"success\":true,"
		"\"data\":{"
			"\"schemaVersion\":1,"
			"\"container\":{\"containerId\":\"3fa85f64-5717-4562-b3fc-2c963f66afa6\",\"ownerPlayerId\":1,\"containerType\":\"RAID_INVENTORY\",\"slotCapacity\":30,\"capacityLevel\":0,\"updatedAt\":\"2026-07-22T07:07:45.127\"},"
			"\"slots\":[{"
				"\"schemaVersion\":1,"
				"\"itemInstanceId\":\"9a4f9d2c-7a1b-43c5-8d6e-1f2a3b4c5d6e\","
				"\"itemTemplateId\":\"Item_Weapon_Sword_CommonSword\","
			
				"\"slotIndex\":0,"
				"\"quantity\":1,"
				"\"durability\":87.5,"
				"\"enhancementLevel\":0,"
				"\"finalRarityTag\":\"COMMON\","
				"\"bindState\":\"UNBOUND\","
				"\"instanceTags\":[\"raid-found\",\"insured\"],"
				"\"randomOptions\":[{\"optionId\":\"crit_chance\",\"value\":12,\"unit\":\"PERCENT\"}],"
				"\"generatedSkills\":[{\"skillId\":\"skill.bleed_shot\",\"level\":2,\"source\":\"ITEM_ROLL\"}],"
				"\"createdAt\":\"2026-07-22T07:07:45.127\","
				"\"acquiredAt\":\"2026-07-22T07:07:45.127\","
				"\"updatedAt\":\"2026-07-22T07:07:45.127\","
				"\"metadata\":{\"origin\":\"raid_extract\",\"lootSourceId\":\"loot-source-001\"}"
			"}],"
			"\"upgrade\":{\"capacityLevel\":0,\"maxCapacityLevel\":3,\"nextCapacityLevel\":0,\"nextSlotCapacity\":0,\"nextUpgradeCost\":[{\"currencyCode\":\"GOLD\",\"amount\":1000}]}"
		"},"
		"\"meta\":{\"requestId\":\"8f9c1d2e-4b6a-4f9d-9d2c-7a1b3c5d6e7f\",\"serverTime\":\"2026-07-22T07:07:45.127Z\",\"apiVersion\":\"v1\"}"
		"}");

	FFrontierOnlineInventoryResponse ParsedCompactInventoryResponse;
	FString CompactInventoryParseError;
	TestTrue(
		TEXT("Inventory parser accepts containerId and compact backend item payload"),
		FFrontierOnlineHttpClient::ParseInventoryResponse(
			200,
			CompactInventoryResponseBody,
			ParsedCompactInventoryResponse,
			CompactInventoryParseError));
	TestTrue(TEXT("Compact inventory parse error remains empty"), CompactInventoryParseError.IsEmpty());
	TestEqual(TEXT("Compact inventory containerId is preserved"), ParsedCompactInventoryResponse.Data.Container.InventoryId, FString(TEXT("3fa85f64-5717-4562-b3fc-2c963f66afa6")));
	TestEqual(TEXT("Compact inventory upgrade max level is parsed"), ParsedCompactInventoryResponse.Data.Upgrade.MaxCapacityLevel, 3);
	TestEqual(TEXT("Compact inventory upgrade cost is parsed"), ParsedCompactInventoryResponse.Data.Upgrade.NextUpgradeCost.Num(), 1);
	TestEqual(TEXT("Compact inventory slot is parsed"), ParsedCompactInventoryResponse.Data.Slots.Num(), 1);
	if (ParsedCompactInventoryResponse.Data.Slots.Num() == 1)
	{
		const FFrontierOnlineItemDTO& ParsedItem = ParsedCompactInventoryResponse.Data.Slots[0];
		TestEqual(TEXT("Compact randomOptions are parsed"), ParsedItem.RandomOptions.Num(), 1);
		TestEqual(TEXT("Compact generatedSkills are parsed"), ParsedItem.GeneratedSkills.Num(), 1);
		TestTrue(TEXT("Compact metadata object is preserved"), ParsedItem.MetadataJson.Contains(TEXT("\"origin\"")));
		if (ParsedItem.RandomOptions.Num() == 1)
		{
			TestEqual(TEXT("optionId is preserved"), ParsedItem.RandomOptions[0].OptionId, FString(TEXT("crit_chance")));
			TestEqual(TEXT("Compact option value becomes final value"), ParsedItem.RandomOptions[0].FinalValue, 12.0);
		}
		if (ParsedItem.GeneratedSkills.Num() == 1)
		{
			TestEqual(TEXT("skillId is preserved"), ParsedItem.GeneratedSkills[0].SkillId, FString(TEXT("skill.bleed_shot")));
			TestEqual(TEXT("Compact skill level is parsed"), ParsedItem.GeneratedSkills[0].Level, 2);
			TestEqual(TEXT("Missing compact slotIndex defaults to array index"), ParsedItem.GeneratedSkills[0].SlotIndex, 0);
		}
	}

	const FString MissingSkillIdInventoryResponseBody = CompactInventoryResponseBody.Replace(
		TEXT("\"generatedSkills\":[{\"skillId\":\"skill.bleed_shot\",\"level\":2,\"source\":\"ITEM_ROLL\"}]"),
		TEXT("\"generatedSkills\":[{\"level\":2,\"source\":\"ITEM_ROLL\"}]"));
	FFrontierOnlineInventoryResponse MissingSkillIdInventoryResponse;
	FString MissingSkillIdInventoryError;
	TestTrue(
		TEXT("Inventory parser ignores an unusable generated skill instead of rejecting the item snapshot"),
		FFrontierOnlineHttpClient::ParseInventoryResponse(
			200,
			MissingSkillIdInventoryResponseBody,
			MissingSkillIdInventoryResponse,
			MissingSkillIdInventoryError));
	TestTrue(TEXT("Ignored generated skill does not produce a parse error"), MissingSkillIdInventoryError.IsEmpty());
	if (MissingSkillIdInventoryResponse.Data.Slots.Num() == 1)
	{
		TestEqual(
			TEXT("Generated skill without an identifier is excluded"),
			MissingSkillIdInventoryResponse.Data.Slots[0].GeneratedSkills.Num(),
			0);
	}

	const FString StorageResponseBody = TEXT(
		"{"
		"\"success\":true,"
		"\"data\":{"
			"\"schemaVersion\":1,"
			"\"container\":{\"containerId\":\"0190f7e4-b088-7608-a30d-0fcb9ceff8fd\",\"ownerPlayerId\":42,\"containerType\":\"STORAGE\",\"slotCapacity\":80,\"capacityLevel\":0},"
			"\"slots\":[{"
				"\"schemaVersion\":1,"
				"\"itemInstanceId\":\"0190f7e4-b088-7608-a30d-0fcb9ceff8fd\","
				"\"itemTemplateId\":\"weapon_rifle_001\","
			
				"\"slotIndex\":0,"
				"\"quantity\":1,"
				"\"durability\":87.5,"
				"\"enhancementLevel\":2,"
				"\"finalRarityTag\":\"EPIC\","
				"\"bindState\":\"UNBOUND\","
				"\"instanceTags\":[\"Item.Type.Equipment\"],"
				"\"randomOptions\":[{\"statTag\":\"Stat.AttackPower\",\"baseValue\":10,\"rolledBonus\":3,\"finalValue\":13,\"score\":5}],"
				"\"generatedSkills\":[{\"skillTag\":\"Ability.Attack.Primary\",\"skillRarityTag\":\"Item.Rarity.Rare\",\"advanced\":true,\"slotIndex\":0}],"
				"\"createdAt\":\"2026-07-15T00:00:00Z\","
				"\"acquiredAt\":\"2026-07-15T00:00:00Z\","
				"\"updatedAt\":\"2026-07-15T00:01:00Z\","
				"\"metadata\":null"
			"}],"
			"\"upgrade\":{\"capacityLevel\":0,\"maxCapacityLevel\":3,\"nextCapacityLevel\":1,\"nextSlotCapacity\":100,\"nextUpgradeCost\":[{\"currencyCode\":\"GOLD\",\"amount\":1000}]}"
		"},"
		"\"meta\":{\"requestId\":\"request-storage-1\",\"serverTime\":\"2026-07-15T00:00:00Z\",\"apiVersion\":\"v1\"}"
		"}");

	FFrontierOnlineStorageResponse ParsedStorageResponse;
	FString StorageParseError;
	TestTrue(
		TEXT("Storage parser reads storage container"),
		FFrontierOnlineHttpClient::ParseStorageResponse(
			200,
			StorageResponseBody,
			ParsedStorageResponse,
			StorageParseError));
	TestTrue(TEXT("Storage parse error remains empty"), StorageParseError.IsEmpty());
	TestEqual(TEXT("Storage ID is preserved"), ParsedStorageResponse.Data.Container.StorageId, FString(TEXT("0190f7e4-b088-7608-a30d-0fcb9ceff8fd")));
	TestEqual(TEXT("Storage capacity is parsed"), ParsedStorageResponse.Data.Container.SlotCapacity, 80);
	TestEqual(TEXT("Storage upgrade cost is parsed"), ParsedStorageResponse.Data.Upgrade.NextUpgradeCost.Num(), 1);

	const FString EquipmentResponseBody = TEXT(
		"{"
		"\"success\":true,"
		"\"data\":{"
			"\"schemaVersion\":1,"
			"\"container\":{\"containerId\":\"0190f7e4-b088-7608-a30d-0fcb9ceff8fd\",\"ownerPlayerId\":42,\"containerType\":\"EQUIPMENT\",\"slotCapacity\":8,\"capacityLevel\":0},"
			"\"slots\":["
				"{\"slotType\":\"MAIN_WEAPON\",\"item\":{"
					"\"schemaVersion\":1,"
					"\"itemInstanceId\":\"0190f7e4-b088-7608-a30d-0fcb9ceff8fd\","
					"\"itemTemplateId\":\"weapon_rifle_001\","
					
					"\"durability\":87.5,"
					"\"enhancementLevel\":2,"
					"\"finalRarityTag\":\"EPIC\","
					"\"bindState\":\"UNBOUND\","
					"\"instanceTags\":[\"Item.Type.Equipment\"],"
					"\"randomOptions\":[{\"statTag\":\"Stat.AttackPower\",\"baseValue\":10,\"rolledBonus\":3,\"finalValue\":13,\"score\":5}],"
					"\"generatedSkills\":[{\"skillTag\":\"Ability.Attack.Primary\",\"skillRarityTag\":\"Item.Rarity.Rare\",\"advanced\":true,\"slotIndex\":0}],"
					"\"acquiredAt\":\"2026-07-15T00:00:00Z\","
					"\"updatedAt\":\"2026-07-15T00:01:00Z\","
					"\"metadata\":null"
				"}},"
				"{\"slotType\":\"SUB_WEAPON\",\"item\":null}"
			"]"
		"},"
		"\"meta\":{\"requestId\":\"request-equipment-1\",\"serverTime\":\"2026-07-15T00:00:00Z\",\"apiVersion\":\"v1\"}"
		"}");

	FFrontierOnlineEquipmentResponse ParsedEquipmentResponse;
	FString EquipmentParseError;
	TestTrue(
		TEXT("Equipment parser reads explicit item options and generated skills"),
		FFrontierOnlineHttpClient::ParseEquipmentResponse(200, EquipmentResponseBody, ParsedEquipmentResponse, EquipmentParseError));
	TestTrue(TEXT("Equipment parse error remains empty"), EquipmentParseError.IsEmpty());
	TestEqual(TEXT("Equipment slots are parsed by slotType"), ParsedEquipmentResponse.Data.Slots.Num(), 2);
	if (ParsedEquipmentResponse.Data.Slots.Num() == 2)
	{
		TestTrue(TEXT("Equipped item is present"), ParsedEquipmentResponse.Data.Slots[0].bHasItem);
		TestFalse(TEXT("Empty equipment slot is preserved"), ParsedEquipmentResponse.Data.Slots[1].bHasItem);
		TestEqual(TEXT("Equipment randomOptions are parsed"), ParsedEquipmentResponse.Data.Slots[0].Item.RandomOptions.Num(), 1);
		TestEqual(TEXT("Equipment generatedSkills are parsed"), ParsedEquipmentResponse.Data.Slots[0].Item.GeneratedSkills.Num(), 1);
	}

	const FString BootstrapResponseBody = TEXT(
		"{"
		"\"success\":true,"
		"\"data\":{"
			"\"player\":{\"playerId\":\"3\",\"steamId\":\"76561198000000000\",\"nickname\":\"FrontierPlayer\"},"
			"\"currencies\":[],"
			"\"inventory\":{"
				"\"schemaVersion\":1,"
				"\"container\":{\"containerId\":\"inventory-3\",\"ownerPlayerId\":3,\"containerType\":\"RAID_INVENTORY\",\"slotCapacity\":30,\"capacityLevel\":0},"
				"\"slots\":[]"
			"},"
			"\"storage\":{"
				"\"schemaVersion\":1,"
				"\"container\":{\"containerId\":\"storage-3\",\"ownerPlayerId\":3,\"containerType\":\"STORAGE\",\"slotCapacity\":80,\"capacityLevel\":0},"
				"\"slots\":[]"
			"},"
			"\"equipment\":{"
				"\"schemaVersion\":1,"
				"\"container\":{\"containerId\":\"equipment-3\",\"ownerPlayerId\":3,\"containerType\":\"EQUIPMENT\",\"slotCapacity\":8,\"capacityLevel\":0},"
				"\"slots\":[]"
			"},"
			"\"activeRaid\":null,"
			
		"},"
		"\"meta\":{\"requestId\":\"request-bootstrap-1\",\"serverTime\":\"2026-07-23T00:00:00Z\",\"apiVersion\":\"v1\"}"
		"}");

	FFrontierOnlineLobbyBootstrapResponse ParsedBootstrapResponse;
	FString BootstrapParseError;
	TestTrue(
		TEXT("Lobby bootstrap survives the FrontierOnline module boundary"),
		FFrontierOnlineHttpClient::ParseLobbyBootstrapResponse(
			200,
			BootstrapResponseBody,
			ParsedBootstrapResponse,
			BootstrapParseError));
	TestTrue(TEXT("Lobby bootstrap parse error remains empty"), BootstrapParseError.IsEmpty());
	TestEqual(TEXT("Bootstrap string player ID is preserved"), ParsedBootstrapResponse.Data.Player.PlayerIdString, FString(TEXT("3")));
	TestEqual(TEXT("Bootstrap numeric player ID remains available"), ParsedBootstrapResponse.Data.Player.PlayerId, static_cast<int64>(3));
	TestEqual(TEXT("Bootstrap inventory capacity survives module boundary"), ParsedBootstrapResponse.Data.Inventory.Container.SlotCapacity, 30);
	TestEqual(TEXT("Bootstrap storage capacity survives module boundary"), ParsedBootstrapResponse.Data.Storage.Container.SlotCapacity, 80);

	FFrontierOnlineLobbyBootstrapResponse FrontierOwnedBootstrapResponse;
	FString FrontierOwnedBootstrapError;
	TestTrue(
		TEXT("Frontier-owned bootstrap parser keeps the composite DTO inside one module"),
		FrontierLobbyBootstrapJson::Parse(
			200,
			BootstrapResponseBody,
			FrontierOwnedBootstrapResponse,
			FrontierOwnedBootstrapError));
	TestTrue(TEXT("Frontier-owned bootstrap parse error remains empty"), FrontierOwnedBootstrapError.IsEmpty());
	TestEqual(
		TEXT("Frontier-owned inventory capacity is preserved"),
		FrontierOwnedBootstrapResponse.Data.Inventory.Container.SlotCapacity,
		30);
	TestEqual(
		TEXT("Frontier-owned storage capacity is preserved"),
		FrontierOwnedBootstrapResponse.Data.Storage.Container.SlotCapacity,
		80);

	FFrontierOnlineLobbyBootstrapResponse InvalidBootstrapResponse;
	FString InvalidBootstrapError;
	const FString ZeroCapacityBootstrap = BootstrapResponseBody.Replace(
		TEXT("\"slotCapacity\":30"),
		TEXT("\"slotCapacity\":0"));
	TestFalse(
		TEXT("Lobby bootstrap rejects a zero backend inventory capacity before runtime mapping"),
		FFrontierOnlineHttpClient::ParseLobbyBootstrapResponse(
			200,
			ZeroCapacityBootstrap,
			InvalidBootstrapResponse,
			InvalidBootstrapError));
	TestTrue(
		TEXT("Zero capacity error identifies the inventory contract"),
		InvalidBootstrapError.Contains(TEXT("inventory.container.slotCapacity")));

	const FString RefreshResponseBody = TEXT(
		"{\"success\":true,\"data\":{"
		"\"session\":{\"sessionId\":\"session-rotated\",\"playerId\":5000000000,\"platform\":\"STEAM\",\"expiresAt\":\"2026-07-23T01:00:00Z\"},"
		"\"tokens\":{\"tokenType\":\"Bearer\",\"accessToken\":\"access-2\",\"accessTokenExpiresAt\":\"2026-07-23T00:15:00Z\",\"refreshToken\":\"refresh-2\",\"refreshTokenExpiresAt\":\"2026-08-23T00:00:00Z\"}"
		"},\"meta\":{\"requestId\":\"refresh-request\",\"serverTime\":\"2026-07-23T00:00:00Z\",\"apiVersion\":\"v1\"}}");
	FFrontierOnlineRefreshResponse ParsedRefresh;
	FString RefreshError;
	TestTrue(TEXT("Live refresh rotation response parses"),
		FFrontierOnlineHttpClient::ParseRefreshResponse(200, RefreshResponseBody, ParsedRefresh, RefreshError));
	TestEqual(TEXT("Refresh preserves int64 playerId"), ParsedRefresh.Session.PlayerId, static_cast<int64>(5000000000LL));
	TestEqual(TEXT("Refresh rotates access token"), ParsedRefresh.Tokens.AccessToken, FString(TEXT("access-2")));
	TestEqual(TEXT("Refresh rotates refresh token"), ParsedRefresh.Tokens.RefreshToken, FString(TEXT("refresh-2")));

	const FString LogoutResponseBody = TEXT(
		"{\"success\":true,"
		"\"data\":{\"revoked\":true,\"revokedSessionCount\":1},"
		"\"meta\":{\"requestId\":\"logout-request\",\"serverTime\":\"2026-07-27T00:00:00Z\",\"apiVersion\":\"v1\"}}");
	FFrontierOnlineLogoutResponse ParsedLogout;
	FString LogoutError;
	TestTrue(
		TEXT("Live logout response parses"),
		FFrontierOnlineHttpClient::ParseLogoutResponse(
			200,
			LogoutResponseBody,
			ParsedLogout,
			LogoutError));
	TestTrue(TEXT("Logout reports the session as revoked"), ParsedLogout.bRevoked);
	TestEqual(TEXT("Logout preserves revoked session count"), ParsedLogout.RevokedSessionCount, 1);
	TestTrue(TEXT("Logout parse error remains empty"), LogoutError.IsEmpty());

	const FString EntryResponseBody = TEXT(
		"{\"success\":true,\"data\":{"
		"\"raidSession\":{\"raidSessionId\":\"3fa85f64-5717-4562-b3fc-2c963f66afa6\",\"playerId\":5000000000,\"state\":\"LOCKED\",\"serverId\":\"frontier-ds-apne2-01\",\"createdAt\":\"2026-07-23T00:00:00Z\",\"startedAt\":null,\"completedAt\":null},"
		"\"joinToken\":\"join-secret\",\"joinTokenExpiresAt\":\"2026-07-23T00:05:00Z\","
		"\"connection\":{\"serverEndpoint\":\"127.0.0.1:7777\",\"transport\":\"udp\"}"
		"},\"meta\":{\"requestId\":\"entry-request\",\"serverTime\":\"2026-07-23T00:00:00Z\",\"apiVersion\":\"v1\"}}");
	FFrontierOnlineRaidEntryResponse ParsedEntry;
	FString EntryError;
	TestTrue(TEXT("Live Raid Entry response parses"),
		FFrontierOnlineHttpClient::ParseRaidEntryResponse(200, EntryResponseBody, ParsedEntry, EntryError));
	TestEqual(TEXT("Raid Entry playerId remains int64"), ParsedEntry.Data.RaidSession.PlayerId, static_cast<int64>(5000000000LL));
	TestEqual(TEXT("Raid Entry connection endpoint is preserved"), ParsedEntry.Data.ServerEndpoint, FString(TEXT("127.0.0.1:7777")));

	const FString JoinResponseBody = TEXT(
		"{\"success\":true,\"data\":{"
		"\"raidSession\":{\"raidSessionId\":\"3fa85f64-5717-4562-b3fc-2c963f66afa6\",\"playerId\":5000000000,\"state\":\"ACTIVE\",\"serverId\":\"frontier-ds-apne2-01\",\"createdAt\":\"2026-07-23T00:00:00Z\",\"startedAt\":\"2026-07-23T00:01:00Z\",\"completedAt\":null},"
		"\"playerId\":5000000000,\"steamId\":\"76561198000000000\","
		"\"loadoutManifest\":{\"inventorySlots\":[{\"slotIndex\":0,\"item\":null}],"
		"\"equipmentSlots\":[{\"slotType\":\"MAIN_WEAPON\",\"item\":null}]}"
		"},\"meta\":{\"requestId\":\"join-request\",\"serverTime\":\"2026-07-23T00:01:00Z\",\"apiVersion\":\"v1\"}}");
	FFrontierOnlineJoinAuthorizationResponse ParsedJoin;
	FString JoinError;
	TestTrue(TEXT("Live Join Authorization response parses"),
		FFrontierOnlineHttpClient::ParseJoinAuthorizationResponse(200, JoinResponseBody, ParsedJoin, JoinError));
	TestEqual(TEXT("Join response playerId remains int64"), ParsedJoin.PlayerId, static_cast<int64>(5000000000LL));
	TestEqual(TEXT("Null raid inventory slots are treated as empty"), ParsedJoin.LoadoutManifest.InventorySlots.Num(), 0);
	TestEqual(TEXT("Null raid equipment slots are treated as empty"), ParsedJoin.LoadoutManifest.EquipmentSlots.Num(), 0);

	const FString ResultResponseBody = TEXT(
		"{\"success\":true,\"data\":{"
		"\"result\":{\"raidSessionId\":\"3fa85f64-5717-4562-b3fc-2c963f66afa6\",\"playerId\":5000000000,\"outcome\":\"EXTRACTED\",\"reasonCode\":\"SUCCESS\",\"committedAt\":\"2026-07-23T00:10:00Z\"},"
		"\"inventory\":null,\"equipment\":null,\"retryAfterSeconds\":null"
		"},\"meta\":{\"requestId\":\"result-request\",\"serverTime\":\"2026-07-23T00:10:01Z\",\"apiVersion\":\"v1\"}}");
	FFrontierOnlineRaidResultResponse ParsedRaidResult;
	FString RaidResultError;
	TestTrue(TEXT("Live player Raid Result response parses"),
		FFrontierOnlineHttpClient::ParseRaidResultResponse(200, ResultResponseBody, ParsedRaidResult, RaidResultError));
	TestEqual(TEXT("Raid Result outcome is preserved"), ParsedRaidResult.Result.Outcome, FString(TEXT("EXTRACTED")));
	TestTrue(TEXT("Raid Result committedAt nullable state is preserved"), ParsedRaidResult.Result.bHasCommittedAt);

	const FString CommitResponseBody = TEXT(
		"{\"success\":true,\"data\":{"
		"\"result\":{\"raidSessionId\":\"3fa85f64-5717-4562-b3fc-2c963f66afa6\",\"playerId\":5000000000,\"outcome\":\"DEAD\",\"reasonCode\":\"HEALTH_ZERO\",\"committedAt\":\"2026-07-23T00:10:00Z\"}"
		"},\"meta\":{\"requestId\":\"commit-request\",\"serverTime\":\"2026-07-23T00:10:01Z\",\"apiVersion\":\"v1\"}}");
	FFrontierOnlineRaidCommitResponse ParsedCommit;
	FString CommitError;
	TestTrue(TEXT("Live internal Raid Result Commit response parses"),
		FFrontierOnlineHttpClient::ParseRaidCommitResponse(200, CommitResponseBody, false, ParsedCommit, CommitError));
	TestEqual(TEXT("Committed playerId remains int64"), ParsedCommit.Result.PlayerId, static_cast<int64>(5000000000LL));

	const FString ExtractCommitResponseBody = TEXT(
		"{\"success\":true,\"data\":{"
		"\"result\":{\"raidSessionId\":\"3fa85f64-5717-4562-b3fc-2c963f66afa6\",\"playerId\":5000000000,\"outcome\":\"EXTRACTED\",\"reasonCode\":\"SUCCESS\",\"committedAt\":\"2026-07-23T00:10:00Z\"},"
		"\"mintedItems\":[{\"raidItemId\":\"11111111-2222-3333-4444-555555555555\",\"itemInstanceId\":\"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\"}]"
		"},\"meta\":{\"requestId\":\"extract-request\",\"serverTime\":\"2026-07-23T00:10:01Z\",\"apiVersion\":\"v1\"}}");
	FFrontierOnlineRaidCommitResponse ParsedExtractCommit;
	FString ExtractCommitError;
	TestTrue(TEXT("Live internal Raid Extract response parses"),
		FFrontierOnlineHttpClient::ParseRaidCommitResponse(
			200,
			ExtractCommitResponseBody,
			true,
			ParsedExtractCommit,
			ExtractCommitError));
	TestEqual(TEXT("Raid Extract minted item mapping is preserved"), ParsedExtractCommit.MintedItems.Num(), 1);
	if (ParsedExtractCommit.MintedItems.Num() == 1)
	{
		TestEqual(
			TEXT("Minted item keeps its raid runtime identity"),
			ParsedExtractCommit.MintedItems[0].RaidItemId,
			FString(TEXT("11111111-2222-3333-4444-555555555555")));
	}

	const FString GrantResponseBody = TEXT(
		"{\"success\":true,\"data\":{\"previousLevel\":4,"
		"\"level\":{\"level\":5,\"totalExperience\":1000,\"currentLevelExperience\":0,\"nextLevelRequiredExperience\":500,\"maxLevel\":50,\"progressionVersion\":1,\"updatedAt\":\"2026-07-23T00:10:00Z\"},"
		"\"levelUps\":[5]},\"meta\":{\"requestId\":\"grant-request\",\"serverTime\":\"2026-07-23T00:10:01Z\",\"apiVersion\":\"v1\"}}");
	FFrontierOnlineExperienceGrantResponse ParsedGrant;
	FString GrantError;
	TestTrue(TEXT("Live Experience Grant response parses"),
		FFrontierOnlineHttpClient::ParseExperienceGrantResponse(200, GrantResponseBody, ParsedGrant, GrantError));
	TestEqual(TEXT("Experience Grant returns updated PlayerLevelDTO"), ParsedGrant.Level.Level, 5);
	TestEqual(TEXT("Experience Grant levelUps are preserved"), ParsedGrant.LevelUps.Num(), 1);

	const FString PartyResponseBody = TEXT(
		"{\"success\":true,\"data\":{\"partyId\":\"party-001\","
		"\"steamLobbyId\":\"109775241234567890\",\"leaderPlayerId\":5000000000,"
		"\"status\":\"ACTIVE\",\"members\":[{\"playerId\":5000000000,"
		"\"steamId\":\"76561198000000000\",\"isLeader\":true}]},"
		"\"meta\":{\"requestId\":\"party-request\",\"serverTime\":\"2026-08-10T00:00:00Z\"}}");
	FFrontierOnlinePartyResponse ParsedParty;
	FString PartyError;
	TestTrue(TEXT("Party envelope parses"),
		FFrontierOnlineHttpClient::ParsePartyResponse(200, PartyResponseBody, ParsedParty, PartyError));
	TestTrue(TEXT("Party response records non-null data"), ParsedParty.bHasData);
	TestEqual(TEXT("Party leader ID remains int64"), ParsedParty.Data.LeaderPlayerId, static_cast<int64>(5000000000LL));

	FFrontierOnlinePartyResponse NoParty;
	FString NoPartyError;
	TestTrue(TEXT("Current-party data:null parses as no active party"),
		FFrontierOnlineHttpClient::ParsePartyResponse(
			200,
			TEXT("{\"success\":true,\"data\":null,\"meta\":{}}"),
			NoParty,
			NoPartyError));
	TestFalse(TEXT("Null party data is distinguishable from a party DTO"), NoParty.bHasData);

	FFrontierOnlineLeavePartyResponse ParsedLeave;
	FString LeaveError;
	TestTrue(TEXT("Party leave envelope parses"),
		FFrontierOnlineHttpClient::ParseLeavePartyResponse(
			200,
			TEXT("{\"success\":true,\"data\":{\"partyId\":\"party-001\","
				"\"leftPlayerId\":5000000000,\"status\":\"ACTIVE\",\"remainingMemberCount\":1},\"meta\":{}}"),
			ParsedLeave,
			LeaveError));
	TestEqual(TEXT("Leaving player ID remains int64"), ParsedLeave.Data.LeftPlayerId, static_cast<int64>(5000000000LL));

	FFrontierOnlineMatchmakingTicketResponse ParsedCancelledTicket;
	FString CancelTicketError;
	TestTrue(TEXT("Minimal matchmaking cancellation envelope parses"),
		FFrontierOnlineHttpClient::ParseMatchmakingTicketResponse(
			200,
			TEXT("{\"success\":true,\"data\":{\"ticketId\":\"ticket-001\","
				"\"status\":\"CANCELLED\"},\"meta\":{}}"),
			ParsedCancelledTicket,
			CancelTicketError));
	TestEqual(TEXT("Cancellation status is preserved"), ParsedCancelledTicket.Data.Status, FString(TEXT("CANCELLED")));

	TestEqual(
		TEXT("Backend Blueprint component retains its original script module"),
		UFrontierBackendProtocolComponent::StaticClass()->GetOutermost()->GetName(),
		FString(TEXT("/Script/Frontier")));
	TestEqual(
		TEXT("Steam Blueprint subsystem retains its original script module"),
		UFrontierSteamSubsystem::StaticClass()->GetOutermost()->GetName(),
		FString(TEXT("/Script/Frontier")));

	return true;
}

#endif
