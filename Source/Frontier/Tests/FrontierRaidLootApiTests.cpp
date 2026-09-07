#if WITH_DEV_AUTOMATION_TESTS

#include "Loot/FrontierRaidLootPoolPolicyDataAsset.h"
#include "Misc/AutomationTest.h"
#include "Progression/FrontierRaidLootTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierRaidLootBatchJsonTest,
	"Frontier.Backend.RaidLoot.BatchJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierRaidLootBatchJsonTest::RunTest(const FString& Parameters)
{
	FFrontierRaidLootBatchRequest Request;
	Request.RaidServerId = TEXT("raid-server-1");
	Request.MapId = TEXT("Map.Factory");
	Request.GenerationReason = TEXT("RAID_INITIALIZE");
	Request.LootRequests.Add({TEXT("Loot.Mutant"), 3});
	FString Body;
	FString Error;
	TestTrue(TEXT("Initial request serializes"), FFrontierRaidLootJson::SerializeBatchRequest(Request, true, Body, Error));
	TestTrue(
		TEXT("Initial request contains mapId"),
		Body.Contains(TEXT("\"mapId\"")) && Body.Contains(TEXT("\"Map.Factory\"")));

	const FString ResponseBody = TEXT(R"JSON(
	{
	  "success": true,
	  "data": {
	    "lootBatchId": "batch-1",
	    "matchId": "match-1",
	    "lootPools": [{
	      "lootTableId": "Loot.Mutant",
	      "entries": [
	        {"lootEntryId":"entry-empty","items":[]},
	        {"lootEntryId":"entry-item","items":[{
	          "raidItemId":"11111111-1111-4111-8111-111111111111",
	          "lootSourceId":"source-1",
	          "itemTemplateId":"weapon_axe_001",
	          "quantity":1,
	          "durability":90.0,
	          "enhancementLevel":0,
	          "finalRarityTag":"EPIC",
	          "randomOptions":[{"optionId":"Stat.AttackPower","value":15}],
	          "generatedSkills":[{"skillId":"Skill.ThrowAxe","level":1,"slotIndex":0}]
	        }]}
	      ]
	    }]
	  },
	  "meta":{"requestId":"request-1"}
	})JSON");
	FFrontierRaidLootBatchResponse Parsed;
	TestTrue(TEXT("Batch response parses"), FFrontierRaidLootJson::ParseBatchResponse(ResponseBody, Parsed, Error));
	TestTrue(TEXT("Batch response succeeds"), Parsed.bSuccess);
	TestEqual(TEXT("Empty no-drop entry is preserved"), Parsed.Data.LootPools[0].Entries[0].Items.Num(), 0);
	TestEqual(TEXT("Backend lootSourceId is preserved"), Parsed.Data.LootPools[0].Entries[1].Items[0].LootSourceId, FString(TEXT("source-1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierRaidLootPoolPolicyTest,
	"Frontier.Backend.RaidLoot.PoolPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierRaidLootPoolPolicyTest::RunTest(const FString& Parameters)
{
	UFrontierRaidLootPoolPolicyDataAsset* PolicyData = NewObject<UFrontierRaidLootPoolPolicyDataAsset>();
	FFrontierRaidLootPoolPolicy& MutantPolicy = PolicyData->Policies.AddDefaulted_GetRef();
	MutantPolicy.LootTableId = TEXT("NORMAL_MUTANT");
	MutantPolicy.InitialSpawnWaves = 10;
	MutantPolicy.RefillThreshold = 10;
	MutantPolicy.RefillRequestCount = 25;

	FString Error;
	TestTrue(TEXT("Valid policy data passes validation"), PolicyData->ValidatePolicies(Error));
	int32 RequestedCount = 0;
	TestTrue(
		TEXT("Initial request count calculation succeeds"),
		MutantPolicy.TryCalculateInitialRequestCount(5, RequestedCount, Error));
	TestEqual(TEXT("Five mutants reserve ten waves"), RequestedCount, 50);

	FFrontierRaidLootPoolPolicy& DuplicatePolicy = PolicyData->Policies.AddDefaulted_GetRef();
	DuplicatePolicy.LootTableId = MutantPolicy.LootTableId;
	TestFalse(TEXT("Duplicate LootTableId is rejected"), PolicyData->ValidatePolicies(Error));
	return true;
}

#endif
