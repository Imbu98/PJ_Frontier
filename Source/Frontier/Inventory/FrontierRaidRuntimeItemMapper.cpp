#include "Inventory/FrontierRaidRuntimeItemMapper.h"

#include "Dom/JsonObject.h"
#include "Inventory/Items/FrontierItemCatalogSubsystem.h"
#include "Inventory/Persistence/FrontierItemPersistenceTypes.h"
#include "Raid/FrontierOnlineRaidTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
TArray<TSharedPtr<FJsonValue>> ParseArray(const FString& Json, bool& bOutParsed)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json.IsEmpty() ? TEXT("[]") : Json);
	bOutParsed = FJsonSerializer::Deserialize(Reader, Values);
	return Values;
}

bool ReadNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, double& OutValue)
{
	return Object.IsValid() && Object->TryGetNumberField(Field, OutValue);
}

bool ParseOptions(
	const FString& Json,
	TArray<FFrontierItemOptionDTO>& OutOptions,
	FString& OutError)
{
	bool bParsed = false;
	const TArray<TSharedPtr<FJsonValue>> Values = ParseArray(Json, bParsed);
	if (!bParsed)
	{
		OutError = TEXT("Raid randomOptions is not a valid JSON array.");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : Values)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		FString OptionId;
		if (!Object.IsValid()
			|| (!Object->TryGetStringField(TEXT("optionId"), OptionId)
				&& !Object->TryGetStringField(TEXT("statTag"), OptionId))
			|| OptionId.IsEmpty())
		{
			OutError = TEXT("Raid randomOptions contains an invalid optionId.");
			return false;
		}

		FFrontierItemOptionDTO& Option = OutOptions.AddDefaulted_GetRef();
		Option.OptionId = MoveTemp(OptionId);
		Object->TryGetStringField(TEXT("unit"), Option.Unit);
		ReadNumber(Object, TEXT("baseValue"), Option.BaseValue);
		ReadNumber(Object, TEXT("randomValue"), Option.RandomValue);
		ReadNumber(Object, TEXT("upgradeValue"), Option.UpgradeValue);
		if (!ReadNumber(Object, TEXT("finalValue"), Option.FinalValue))
		{
			if (ReadNumber(Object, TEXT("value"), Option.FinalValue))
			{
				Option.BaseValue = Option.FinalValue;
			}
			else
			{
				Option.FinalValue = Option.BaseValue + Option.RandomValue + Option.UpgradeValue;
			}
		}
		if (ReadNumber(Object, TEXT("normalizedValue"), Option.NormalizedValue))
		{
			Option.bHasNormalizedValue = true;
		}
		else
		{
			Option.bHasNormalizedValue = ReadNumber(Object, TEXT("normalizedScore"), Option.NormalizedValue);
		}
	}
	return true;
}

bool ParseSkills(
	const FString& Json,
	TArray<FFrontierGeneratedItemSkillDTO>& OutSkills,
	FString& OutError)
{
	bool bParsed = false;
	const TArray<TSharedPtr<FJsonValue>> Values = ParseArray(Json, bParsed);
	if (!bParsed)
	{
		OutError = TEXT("Raid generatedSkills is not a valid JSON array.");
		return false;
	}
	for (int32 Index = 0; Index < Values.Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> Object = Values[Index].IsValid() ? Values[Index]->AsObject() : nullptr;
		bool bGenerationFailed = false;
		if (Object.IsValid())
		{
			Object->TryGetBoolField(TEXT("generationFailed"), bGenerationFailed);
		}
		if (bGenerationFailed)
		{
			continue;
		}

		FString SkillId;
		if (!Object.IsValid()
			|| ((!Object->TryGetStringField(TEXT("skillTemplateId"), SkillId)
				&& !Object->TryGetStringField(TEXT("skillId"), SkillId)
				&& !Object->TryGetStringField(TEXT("skillTag"), SkillId))
				|| SkillId.IsEmpty()))
		{
			OutError = TEXT("Raid generatedSkills contains an invalid skillTemplateId or skillTag.");
			return false;
		}

		FFrontierGeneratedItemSkillDTO& Skill = OutSkills.AddDefaulted_GetRef();
		Skill.SkillId = MoveTemp(SkillId);
		Object->TryGetStringField(TEXT("skillTag"), Skill.SkillTag);
		Object->TryGetStringField(TEXT("rarity"), Skill.Rarity);
		Object->TryGetStringField(TEXT("elementalType"), Skill.ElementalType);
		double Number = 0.0;
		Skill.Level = (ReadNumber(Object, TEXT("skillLevel"), Number)
			|| ReadNumber(Object, TEXT("level"), Number))
			? FMath::Max(1, FMath::RoundToInt(Number))
			: 1;
		Skill.SlotIndex = ReadNumber(Object, TEXT("slotIndex"), Number)
			? FMath::RoundToInt(Number)
			: Index;
	}
	return true;
}
}

bool FFrontierRaidRuntimeItemMapper::TryBuildRuntimeItem(
	const FFrontierOnlineRaidRuntimeItemDTO& RaidItem,
	const UFrontierItemCatalogSubsystem& ItemCatalog,
	const bool bRequireLootSourceId,
	FFrontierItemInstance& OutItem,
	FString& OutError)
{
	OutError.Reset();
	FGuid RaidItemId;
	if (!FGuid::Parse(RaidItem.RaidItemId, RaidItemId) || !RaidItemId.IsValid())
	{
		OutError = TEXT("Raid item contains an invalid raidItemId.");
		return false;
	}

	FGuid OriginItemId;
	if (RaidItem.bHasOriginItemInstanceId
		&& (!FGuid::Parse(RaidItem.OriginItemInstanceId, OriginItemId) || !OriginItemId.IsValid()))
	{
		OutError = TEXT("Raid item contains an invalid originItemInstanceId.");
		return false;
	}
	if (bRequireLootSourceId && (!RaidItem.bHasLootSourceId || RaidItem.LootSourceId.IsEmpty()))
	{
		OutError = TEXT("New raid loot is missing its backend-issued lootSourceId.");
		return false;
	}
	if (RaidItem.Quantity <= 0 || RaidItem.ItemTemplateId.IsEmpty())
	{
		OutError = TEXT("Raid item contains an invalid quantity or itemTemplateId.");
		return false;
	}

	const FFrontierResolvedItemTemplateData* TemplateData =
		ItemCatalog.ResolveItemTemplateData(FName(*RaidItem.ItemTemplateId));
	if (!TemplateData)
	{
		OutError = FString::Printf(TEXT("Raid item template is unavailable: %s"), *RaidItem.ItemTemplateId);
		return false;
	}

	FFrontierItemPersistenceDTO Persistence;
	Persistence.ItemInstanceId = (OriginItemId.IsValid() ? OriginItemId : RaidItemId)
		.ToString(EGuidFormats::DigitsWithHyphensLower);
	Persistence.ItemTemplateId = RaidItem.ItemTemplateId;
	Persistence.Quantity = RaidItem.Quantity;
	if (RaidItem.bHasDurability)
	{
		Persistence.Durability = RaidItem.Durability;
	}
	Persistence.EnhancementLevel = RaidItem.EnhancementLevel;
	Persistence.FinalRarityTag = RaidItem.FinalRarityTag;
	Persistence.BindState = EFrontierItemBindState::Unbound;
	if (!ParseOptions(RaidItem.RandomOptionsJson, Persistence.RandomOptions, OutError)
		|| !ParseSkills(RaidItem.GeneratedSkillsJson, Persistence.GeneratedSkills, OutError)
		|| !FFrontierItemPersistenceMapper::TryBuildRuntimeItem(
			Persistence,
			*TemplateData,
			OutItem,
			OutError))
	{
		return false;
	}

	OutItem.RaidItemId = RaidItemId;
	OutItem.OriginItemInstanceId = OriginItemId;
	OutItem.RaidLootSourceId = RaidItem.bHasLootSourceId ? RaidItem.LootSourceId : FString();
	return true;
}
