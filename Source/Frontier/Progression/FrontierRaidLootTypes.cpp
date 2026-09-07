#include "Progression/FrontierRaidLootTypes.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
bool SerializeRaidLootObject(const TSharedRef<FJsonObject>& Object, FString& OutJson)
{
	OutJson.Reset();
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	return FJsonSerializer::Serialize(Object, Writer);
}

bool SerializeArray(const TArray<TSharedPtr<FJsonValue>>& Values, FString& OutJson)
{
	OutJson.Reset();
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	return FJsonSerializer::Serialize(Values, Writer);
}

bool ReadRequiredString(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Field,
	FString& OutValue,
	FString& OutError)
{
	if (!Object.IsValid() || !Object->TryGetStringField(Field, OutValue) || OutValue.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Raid loot response is missing %s."), Field);
		return false;
	}
	return true;
}

bool ReadRequiredInt(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Field,
	int32& OutValue,
	FString& OutError)
{
	double Number = 0.0;
	if (!Object.IsValid() || !Object->TryGetNumberField(Field, Number))
	{
		OutError = FString::Printf(TEXT("Raid loot response is missing %s."), Field);
		return false;
	}
	OutValue = FMath::RoundToInt(Number);
	return true;
}

bool ParseRuntimeItem(
	const TSharedPtr<FJsonObject>& Object,
	FFrontierOnlineRaidRuntimeItemDTO& OutItem,
	FString& OutError)
{
	if (!ReadRequiredString(Object, TEXT("raidItemId"), OutItem.RaidItemId, OutError)
		|| !ReadRequiredString(Object, TEXT("lootSourceId"), OutItem.LootSourceId, OutError)
		|| !ReadRequiredString(Object, TEXT("itemTemplateId"), OutItem.ItemTemplateId, OutError)
		|| !ReadRequiredInt(Object, TEXT("quantity"), OutItem.Quantity, OutError)
		|| !ReadRequiredInt(Object, TEXT("enhancementLevel"), OutItem.EnhancementLevel, OutError)
		|| !ReadRequiredString(Object, TEXT("finalRarityTag"), OutItem.FinalRarityTag, OutError))
	{
		return false;
	}

	OutItem.bHasLootSourceId = true;
	if (OutItem.Quantity <= 0 || OutItem.EnhancementLevel < 0)
	{
		OutError = TEXT("Raid loot item has an invalid quantity or enhancementLevel.");
		return false;
	}

	double Durability = 0.0;
	OutItem.bHasDurability = Object->TryGetNumberField(TEXT("durability"), Durability);
	OutItem.Durability = OutItem.bHasDurability ? Durability : 0.0;

	const TArray<TSharedPtr<FJsonValue>>* RandomOptions = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* GeneratedSkills = nullptr;
	if (!Object->TryGetArrayField(TEXT("randomOptions"), RandomOptions) || !RandomOptions
		|| !Object->TryGetArrayField(TEXT("generatedSkills"), GeneratedSkills) || !GeneratedSkills
		|| !SerializeArray(*RandomOptions, OutItem.RandomOptionsJson)
		|| !SerializeArray(*GeneratedSkills, OutItem.GeneratedSkillsJson))
	{
		OutError = TEXT("Raid loot item is missing serializable randomOptions or generatedSkills arrays.");
		return false;
	}
	return true;
}

void ParseMeta(const TSharedPtr<FJsonObject>& Root, FString& OutRequestId)
{
	const TSharedPtr<FJsonObject>* Meta = nullptr;
	if (Root.IsValid() && Root->TryGetObjectField(TEXT("meta"), Meta) && Meta && Meta->IsValid())
	{
		(*Meta)->TryGetStringField(TEXT("requestId"), OutRequestId);
	}
}
}

bool FFrontierRaidLootJson::SerializeBatchRequest(
	const FFrontierRaidLootBatchRequest& Request,
	const bool bIncludeMapId,
	FString& OutBody,
	FString& OutError)
{
	OutError.Reset();
	if (Request.RaidServerId.IsEmpty() || Request.GenerationReason.IsEmpty()
		|| (bIncludeMapId && Request.MapId.IsEmpty()) || Request.LootRequests.IsEmpty())
	{
		OutError = TEXT("Raid loot request requires raidServerId, generationReason, requests, and mapId for initial generation.");
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> RequestValues;
	TSet<FString> SeenTableIds;
	for (const FFrontierRaidLootRequestEntry& Entry : Request.LootRequests)
	{
		if (Entry.LootTableId.IsEmpty() || Entry.RequestedCount <= 0 || SeenTableIds.Contains(Entry.LootTableId))
		{
			OutError = TEXT("Raid loot requests require unique non-empty lootTableIds and positive requestedCount values.");
			return false;
		}
		SeenTableIds.Add(Entry.LootTableId);
		TSharedRef<FJsonObject> EntryObject = MakeShared<FJsonObject>();
		EntryObject->SetStringField(TEXT("lootTableId"), Entry.LootTableId);
		EntryObject->SetNumberField(TEXT("requestedCount"), Entry.RequestedCount);
		RequestValues.Add(MakeShared<FJsonValueObject>(EntryObject));
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("raidServerId"), Request.RaidServerId);
	if (bIncludeMapId)
	{
		Root->SetStringField(TEXT("mapId"), Request.MapId);
	}
	Root->SetStringField(TEXT("generationReason"), Request.GenerationReason);
	Root->SetArrayField(TEXT("lootRequests"), MoveTemp(RequestValues));
	if (!SerializeRaidLootObject(Root, OutBody))
	{
		OutError = TEXT("Raid loot request JSON serialization failed.");
		return false;
	}
	return true;
}

bool FFrontierRaidLootJson::ParseBatchResponse(
	const FString& Body,
	FFrontierRaidLootBatchResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Raid loot response is not valid JSON.");
		return false;
	}
	ParseMeta(Root, OutResponse.RequestId);
	if (!Root->TryGetBoolField(TEXT("success"), OutResponse.bSuccess) || !OutResponse.bSuccess)
	{
		OutError = TEXT("Raid loot response did not contain success=true.");
		return false;
	}

	const TSharedPtr<FJsonObject>* Data = nullptr;
	if (!Root->TryGetObjectField(TEXT("data"), Data) || !Data || !Data->IsValid()
		|| !ReadRequiredString(*Data, TEXT("lootBatchId"), OutResponse.Data.LootBatchId, OutError)
		|| !ReadRequiredString(*Data, TEXT("matchId"), OutResponse.Data.MatchId, OutError))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Pools = nullptr;
	if (!(*Data)->TryGetArrayField(TEXT("lootPools"), Pools) || !Pools)
	{
		OutError = TEXT("Raid loot response is missing lootPools.");
		return false;
	}
	TSet<FString> SeenPools;
	for (const TSharedPtr<FJsonValue>& PoolValue : *Pools)
	{
		const TSharedPtr<FJsonObject> PoolObject = PoolValue.IsValid() ? PoolValue->AsObject() : nullptr;
		FFrontierRaidLootPoolDTO& Pool = OutResponse.Data.LootPools.AddDefaulted_GetRef();
		if (!ReadRequiredString(PoolObject, TEXT("lootTableId"), Pool.LootTableId, OutError)
			|| SeenPools.Contains(Pool.LootTableId))
		{
			OutError = SeenPools.Contains(Pool.LootTableId)
				? TEXT("Raid loot response contains a duplicate lootTableId pool.")
				: OutError;
			return false;
		}
		SeenPools.Add(Pool.LootTableId);

		const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
		if (!PoolObject->TryGetArrayField(TEXT("entries"), Entries) || !Entries)
		{
			OutError = TEXT("Raid loot pool is missing entries.");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& EntryValue : *Entries)
		{
			const TSharedPtr<FJsonObject> EntryObject = EntryValue.IsValid() ? EntryValue->AsObject() : nullptr;
			FFrontierRaidLootEntryDTO& Entry = Pool.Entries.AddDefaulted_GetRef();
			if (!ReadRequiredString(EntryObject, TEXT("lootEntryId"), Entry.LootEntryId, OutError))
			{
				return false;
			}
			const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
			if (!EntryObject->TryGetArrayField(TEXT("items"), Items) || !Items)
			{
				OutError = TEXT("Raid loot entry is missing its items array.");
				return false;
			}
			for (const TSharedPtr<FJsonValue>& ItemValue : *Items)
			{
				FFrontierOnlineRaidRuntimeItemDTO& Item = Entry.Items.AddDefaulted_GetRef();
				if (!ParseRuntimeItem(ItemValue.IsValid() ? ItemValue->AsObject() : nullptr, Item, OutError))
				{
					return false;
				}
			}
		}
	}
	return true;
}

bool FFrontierRaidLootJson::SerializeReadyRequest(
	const FFrontierRaidServerReadyRequest& Request,
	FString& OutBody,
	FString& OutError)
{
	OutError.Reset();
	if (Request.RaidServerId.IsEmpty() || Request.MatchId.IsEmpty() || Request.MapId.IsEmpty()
		|| Request.ServerAddress.IsEmpty() || Request.Port <= 0 || !Request.bLootReady)
	{
		OutError = TEXT("Raid server ready requires server, match, map, address, port, and lootReady=true.");
		return false;
	}
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("raidServerId"), Request.RaidServerId);
	Root->SetStringField(TEXT("matchId"), Request.MatchId);
	Root->SetStringField(TEXT("mapId"), Request.MapId);
	Root->SetStringField(TEXT("serverAddress"), Request.ServerAddress);
	Root->SetNumberField(TEXT("port"), Request.Port);
	Root->SetBoolField(TEXT("lootReady"), Request.bLootReady);
	if (!SerializeRaidLootObject(Root, OutBody))
	{
		OutError = TEXT("Raid server ready JSON serialization failed.");
		return false;
	}
	return true;
}

bool FFrontierRaidLootJson::ParseReadyResponse(
	const FString& Body,
	FFrontierRaidServerReadyResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Raid server ready response is not valid JSON.");
		return false;
	}
	ParseMeta(Root, OutResponse.RequestId);
	if (!Root->TryGetBoolField(TEXT("success"), OutResponse.bSuccess) || !OutResponse.bSuccess)
	{
		OutError = TEXT("Raid server ready response did not contain success=true.");
		return false;
	}
	const TSharedPtr<FJsonObject>* Data = nullptr;
	if (!Root->TryGetObjectField(TEXT("data"), Data) || !Data || !Data->IsValid()
		|| !ReadRequiredString(*Data, TEXT("raidServerId"), OutResponse.RaidServerId, OutError)
		|| !ReadRequiredString(*Data, TEXT("matchId"), OutResponse.MatchId, OutError)
		|| !ReadRequiredString(*Data, TEXT("status"), OutResponse.Status, OutError)
		|| !(*Data)->TryGetBoolField(TEXT("accepted"), OutResponse.bAccepted))
	{
		OutError = OutError.IsEmpty()
			? TEXT("Raid server ready response is missing accepted.")
			: OutError;
		return false;
	}
	return true;
}

bool FFrontierRaidLootJson::SerializeFailureRequest(
	const FFrontierRaidServerFailureRequest& Request,
	FString& OutBody,
	FString& OutError)
{
	OutError.Reset();
	if (Request.RaidServerId.IsEmpty() || Request.MatchId.IsEmpty() || Request.MapId.IsEmpty()
		|| Request.ErrorCode.IsEmpty() || Request.Message.IsEmpty())
	{
		OutError = TEXT("Raid server failure requires server, match, map, errorCode, and message.");
		return false;
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("raidServerId"), Request.RaidServerId);
	Root->SetStringField(TEXT("matchId"), Request.MatchId);
	Root->SetStringField(TEXT("mapId"), Request.MapId);
	Root->SetStringField(TEXT("errorCode"), Request.ErrorCode);
	Root->SetStringField(TEXT("message"), Request.Message);
	if (!SerializeRaidLootObject(Root, OutBody))
	{
		OutError = TEXT("Raid server failure JSON serialization failed.");
		return false;
	}
	return true;
}
