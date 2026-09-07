#include "Inventory/Persistence/FrontierItemPersistenceTypes.h"

#include "Frontier.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace FrontierItemPersistence
{
bool TryResolveGameplayTag(const FString& TagString, const TCHAR* FieldName, FGameplayTag& OutTag, FString& OutError)
{
	OutTag = FGameplayTag();
	if (TagString.IsEmpty())
	{
		OutError = FString::Printf(TEXT("%s is empty."), FieldName);
		return false;
	}

	OutTag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
	if (!OutTag.IsValid())
	{
		OutError = FString::Printf(TEXT("%s contains an unknown Gameplay Tag: %s"), FieldName, *TagString);
		return false;
	}

	return true;
}

bool TryParseItemInstanceId(const FString& ItemInstanceId, FGuid& OutGuid, FString& OutError)
{
	if (!FGuid::Parse(ItemInstanceId, OutGuid) || !OutGuid.IsValid())
	{
		OutError = TEXT("itemInstanceId is not a valid UUID.");
		return false;
	}

	return true;
}

FString FormatItemInstanceId(const FGuid& ItemInstanceId)
{
	return ItemInstanceId.ToString(EGuidFormats::DigitsWithHyphensLower);
}

bool TryDeserializeMetadata(const FString& MetadataJson, TSharedPtr<FJsonObject>& OutMetadataObject, FString& OutError)
{
	OutMetadataObject.Reset();
	if (MetadataJson.IsEmpty() || MetadataJson == TEXT("{}"))
	{
		return true;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MetadataJson);
	if (!FJsonSerializer::Deserialize(Reader, OutMetadataObject) || !OutMetadataObject.IsValid())
	{
		OutError = TEXT("metadata could not be parsed as a JSON object.");
		return false;
	}

	return true;
}

bool TryGetStringAlias(const TSharedPtr<FJsonObject>& Object, const TArray<const TCHAR*>& FieldNames, FString& OutValue)
{
	if (!Object.IsValid())
	{
		return false;
	}

	for (const TCHAR* FieldName : FieldNames)
	{
		if (Object->TryGetStringField(FieldName, OutValue))
		{
			return true;
		}
	}

	return false;
}

bool TryGetNumberAlias(const TSharedPtr<FJsonObject>& Object, const TArray<const TCHAR*>& FieldNames, double& OutValue)
{
	if (!Object.IsValid())
	{
		return false;
	}

	for (const TCHAR* FieldName : FieldNames)
	{
		if (Object->TryGetNumberField(FieldName, OutValue))
		{
			return FMath::IsFinite(OutValue);
		}
	}

	return false;
}

bool TryParseMetadataRandomOptions(
	const TSharedPtr<FJsonObject>& MetadataObject,
	TArray<FFrontierItemOptionDTO>& OutRandomOptions,
	FString& OutError)
{
	if (!MetadataObject.IsValid() || !MetadataObject->HasTypedField<EJson::Array>(TEXT("randomOptions")))
	{
		return true;
	}

	const TArray<TSharedPtr<FJsonValue>>* OptionsArray = nullptr;
	if (!MetadataObject->TryGetArrayField(TEXT("randomOptions"), OptionsArray) || !OptionsArray)
	{
		OutError = TEXT("metadata.randomOptions must be an array.");
		return false;
	}

	TSet<FString> SeenOptionIds;
	OutRandomOptions.Reset(OptionsArray->Num());
	for (const TSharedPtr<FJsonValue>& OptionValue : *OptionsArray)
	{
		if (!OptionValue.IsValid() || OptionValue->Type != EJson::Object)
		{
			OutError = TEXT("metadata.randomOptions contains a non-object entry.");
			return false;
		}

		const TSharedPtr<FJsonObject> OptionObject = OptionValue->AsObject();
		FFrontierItemOptionDTO& OptionDTO = OutRandomOptions.AddDefaulted_GetRef();
		if (!TryGetStringAlias(OptionObject, { TEXT("optionId"), TEXT("statTag"), TEXT("statId") }, OptionDTO.OptionId) || OptionDTO.OptionId.IsEmpty())
		{
			OutError = TEXT("metadata.randomOptions.optionId is required.");
			return false;
		}
		if (SeenOptionIds.Contains(OptionDTO.OptionId))
		{
			OutError = FString::Printf(TEXT("metadata.randomOptions contains duplicate optionId: %s"), *OptionDTO.OptionId);
			return false;
		}
		SeenOptionIds.Add(OptionDTO.OptionId);
		OptionObject->TryGetStringField(TEXT("unit"), OptionDTO.Unit);
		(void)TryGetNumberAlias(OptionObject, { TEXT("baseValue") }, OptionDTO.BaseValue);
		if (!TryGetNumberAlias(OptionObject, { TEXT("randomValue"), TEXT("rolledBonus"), TEXT("rolledRandomBonus"), TEXT("randomBonus") }, OptionDTO.RandomValue))
		{
			double RolledValue = 0.0;
			if (TryGetNumberAlias(OptionObject, { TEXT("rolledValue") }, RolledValue))
			{
				OptionDTO.RandomValue = RolledValue - OptionDTO.BaseValue;
			}
		}
		(void)TryGetNumberAlias(OptionObject, { TEXT("upgradeValue"), TEXT("enhancementBonus") }, OptionDTO.UpgradeValue);
		double ReforgeBonus = 0.0;
		if (TryGetNumberAlias(OptionObject, { TEXT("reforgeBonus") }, ReforgeBonus))
		{
			OptionDTO.UpgradeValue += ReforgeBonus;
		}
		if (!TryGetNumberAlias(OptionObject, { TEXT("finalValue"), TEXT("currentFinalValue"), TEXT("value") }, OptionDTO.FinalValue))
		{
			OptionDTO.FinalValue = OptionDTO.BaseValue + OptionDTO.RandomValue + OptionDTO.UpgradeValue;
		}
		OptionDTO.bHasNormalizedValue = TryGetNumberAlias(
			OptionObject,
			{ TEXT("normalizedValue"), TEXT("normalizedScore") },
			OptionDTO.NormalizedValue);
	}

	return true;
}

bool TryParseMetadataGeneratedSkills(
	const TSharedPtr<FJsonObject>& MetadataObject,
	TArray<FFrontierGeneratedItemSkillDTO>& OutGeneratedSkills,
	FString& OutError)
{
	if (!MetadataObject.IsValid() || !MetadataObject->HasTypedField<EJson::Array>(TEXT("generatedSkills")))
	{
		return true;
	}

	const TArray<TSharedPtr<FJsonValue>>* SkillsArray = nullptr;
	if (!MetadataObject->TryGetArrayField(TEXT("generatedSkills"), SkillsArray) || !SkillsArray)
	{
		OutError = TEXT("metadata.generatedSkills must be an array.");
		return false;
	}

	TSet<int32> SeenSkillSlots;
	OutGeneratedSkills.Reset(SkillsArray->Num());
	for (int32 Index = 0; Index < SkillsArray->Num(); ++Index)
	{
		const TSharedPtr<FJsonValue>& SkillValue = (*SkillsArray)[Index];
		if (!SkillValue.IsValid() || SkillValue->Type != EJson::Object)
		{
			OutError = TEXT("metadata.generatedSkills contains a non-object entry.");
			return false;
		}

		const TSharedPtr<FJsonObject> SkillObject = SkillValue->AsObject();
		FFrontierGeneratedItemSkillDTO& SkillDTO = OutGeneratedSkills.AddDefaulted_GetRef();
		(void)TryGetStringAlias(SkillObject, { TEXT("skillId"), TEXT("skillTemplateId"), TEXT("skillTag") }, SkillDTO.SkillId);
		if (SkillDTO.SkillId.IsEmpty())
		{
			OutError = TEXT("metadata.generatedSkills.skillId is required.");
			return false;
		}

		double SlotIndexNumber = static_cast<double>(Index);
		(void)TryGetNumberAlias(SkillObject, { TEXT("slotIndex") }, SlotIndexNumber);
		const int64 RoundedSlotIndex = FMath::RoundToInt64(SlotIndexNumber);
		if (!FMath::IsNearlyEqual(SlotIndexNumber, static_cast<double>(RoundedSlotIndex)))
		{
			OutError = FString::Printf(TEXT("metadata.generatedSkills.slotIndex must be an integer for skillId: %s"), *SkillDTO.SkillId);
			return false;
		}

		SkillDTO.SlotIndex = static_cast<int32>(RoundedSlotIndex);
		if (SkillDTO.SlotIndex < 0 || SeenSkillSlots.Contains(SkillDTO.SlotIndex))
		{
			OutError = FString::Printf(TEXT("metadata.generatedSkills contains an invalid or duplicate slotIndex: %d"), SkillDTO.SlotIndex);
			return false;
		}
		SeenSkillSlots.Add(SkillDTO.SlotIndex);

		double SkillLevelNumber = 1.0;
		(void)TryGetNumberAlias(SkillObject, { TEXT("skillLevel"), TEXT("level") }, SkillLevelNumber);
		const int64 RoundedSkillLevel = FMath::RoundToInt64(SkillLevelNumber);
		if (!FMath::IsNearlyEqual(SkillLevelNumber, static_cast<double>(RoundedSkillLevel))
			|| RoundedSkillLevel < TNumericLimits<int32>::Lowest()
			|| RoundedSkillLevel > TNumericLimits<int32>::Max())
		{
			OutError = FString::Printf(TEXT("metadata.generatedSkills.level must be an integer for skillId: %s"), *SkillDTO.SkillId);
			return false;
		}
		SkillDTO.Level = static_cast<int32>(RoundedSkillLevel);
	}

	return true;
}

bool TryApplyMetadataItemInstanceFields(
	const FString& MetadataJson,
	TArray<FFrontierItemOptionDTO>& InOutRandomOptions,
	TArray<FFrontierGeneratedItemSkillDTO>& InOutGeneratedSkills,
	FString& OutError)
{
	TSharedPtr<FJsonObject> MetadataObject;
	if (!TryDeserializeMetadata(MetadataJson, MetadataObject, OutError))
	{
		return false;
	}
	if (!MetadataObject.IsValid())
	{
		return true;
	}

	return (!InOutRandomOptions.IsEmpty() || TryParseMetadataRandomOptions(MetadataObject, InOutRandomOptions, OutError))
		&& (!InOutGeneratedSkills.IsEmpty() || TryParseMetadataGeneratedSkills(MetadataObject, InOutGeneratedSkills, OutError));
}
}

bool FFrontierItemPersistenceMapper::TryBuildRuntimeItem(
	const FFrontierItemPersistenceDTO& DTO,
	const FFrontierResolvedItemTemplateData& ResolvedItemTemplateData,
	FFrontierItemInstance& OutItemInstance,
	FString& OutError)
{
	using namespace FrontierItemPersistence;

	OutItemInstance = FFrontierItemInstance();
	OutError.Reset();

	if (ResolvedItemTemplateData.ItemTemplateId.IsNone())
	{
		OutError = TEXT("No ItemTemplateData was resolved for itemTemplateId.");
		return false;
	}
	if (DTO.ItemTemplateId.IsEmpty()
		|| ResolvedItemTemplateData.ItemTemplateId.ToString() != DTO.ItemTemplateId)
	{
		OutError = FString::Printf(
			TEXT("itemTemplateId does not match the resolved ItemTemplateData. DTO=%s Template=%s"),
			*DTO.ItemTemplateId,
			*ResolvedItemTemplateData.ItemTemplateId.ToString());
		return false;
	}
	if (!IsValidElementForCategory(ResolvedItemTemplateData.Common.Category, ResolvedItemTemplateData.Common.ElementalType))
	{
		OutError = FString::Printf(
			TEXT("ItemTemplateData has invalid category/elemental type. ItemTemplateId=%s Category=%d ElementalType=%d"),
			*ResolvedItemTemplateData.ItemTemplateId.ToString(),
			static_cast<int32>(ResolvedItemTemplateData.Common.Category),
			static_cast<int32>(ResolvedItemTemplateData.Common.ElementalType));
		return false;
	}
	if (DTO.Quantity <= 0)
	{
		OutError = TEXT("quantity must be greater than zero.");
		return false;
	}
	if (DTO.EnhancementLevel < 0)
	{
		OutError = TEXT("enhancementLevel cannot be negative.");
		return false;
	}
	if (DTO.Durability.IsSet() && DTO.Durability.GetValue() < 0.0)
	{
		OutError = TEXT("durability cannot be negative.");
		return false;
	}
	FGuid ParsedItemInstanceId;
	if (!TryParseItemInstanceId(DTO.ItemInstanceId, ParsedItemInstanceId, OutError))
	{
		return false;
	}

	EFrontierItemRarity ParsedRarity = EFrontierItemRarity::Common;
	if (!TryParseItemRarity(DTO.FinalRarityTag, ParsedRarity))
	{
		OutError = FString::Printf(TEXT("Unknown finalRarityTag: %s"), *DTO.FinalRarityTag);
		return false;
	}

	TArray<FFrontierItemOptionDTO> ResolvedRandomOptions = DTO.RandomOptions;
	TArray<FFrontierGeneratedItemSkillDTO> ResolvedGeneratedSkills = DTO.GeneratedSkills;
	if (!TryApplyMetadataItemInstanceFields(DTO.MetadataJson, ResolvedRandomOptions, ResolvedGeneratedSkills, OutError))
	{
		return false;
	}

	FFrontierItemInstance RuntimeItem;
	RuntimeItem.ItemInstanceId = ParsedItemInstanceId;
	RuntimeItem.ItemTemplateData = ResolvedItemTemplateData;
	RuntimeItem.Quantity = DTO.Quantity;
	RuntimeItem.EnhancementLevel = DTO.EnhancementLevel;
	RuntimeItem.Durability = DTO.Durability.IsSet()
		? static_cast<float>(DTO.Durability.GetValue())
		: 0.0f;
	RuntimeItem.BindState = DTO.BindState;
	RuntimeItem.FinalRarity = ParsedRarity;

	for (const FString& InstanceTagString : DTO.InstanceTags)
	{
		if (InstanceTagString.IsEmpty())
		{
			FRONTIER_LOG(Warning, TEXT("Empty instance tag skipped. ItemInstanceId=%s ItemTemplateId=%s"),
				*DTO.ItemInstanceId,
				*DTO.ItemTemplateId);
			continue;
		}
		RuntimeItem.InstanceTags.AddUnique(InstanceTagString);
	}

	TSet<FString> SeenOptionIds;
	for (const FFrontierItemOptionDTO& OptionDTO : ResolvedRandomOptions)
	{
		if (OptionDTO.OptionId.IsEmpty() || SeenOptionIds.Contains(OptionDTO.OptionId))
		{
			FRONTIER_LOG(Error, TEXT("Random option rejected by empty or duplicate optionId. ItemInstanceId=%s ItemTemplateId=%s OptionId=%s"),
				*DTO.ItemInstanceId,
				*DTO.ItemTemplateId,
				*OptionDTO.OptionId);
			continue;
		}
		SeenOptionIds.Add(OptionDTO.OptionId);

		FGameplayTag StatTag;
		FString StatTagError;
		if (!TryResolveGameplayTag(OptionDTO.OptionId, TEXT("randomOptions.optionId"), StatTag, StatTagError))
		{
			FRONTIER_LOG(Verbose, TEXT("Random option has no matching GameplayTag and remains UI-only. ItemInstanceId=%s ItemTemplateId=%s OptionId=%s"),
				*DTO.ItemInstanceId,
				*DTO.ItemTemplateId,
				*OptionDTO.OptionId);
		}
		if (!FMath::IsFinite(OptionDTO.BaseValue)
			|| !FMath::IsFinite(OptionDTO.RandomValue)
			|| !FMath::IsFinite(OptionDTO.UpgradeValue)
			|| !FMath::IsFinite(OptionDTO.FinalValue)
			|| !FMath::IsFinite(OptionDTO.NormalizedValue))
		{
			FRONTIER_LOG(Error, TEXT("Random option rejected by non-finite value. ItemInstanceId=%s ItemTemplateId=%s OptionId=%s"),
				*DTO.ItemInstanceId,
				*DTO.ItemTemplateId,
				*OptionDTO.OptionId);
			continue;
		}

		const float CalculatedValue = static_cast<float>(OptionDTO.BaseValue + OptionDTO.RandomValue + OptionDTO.UpgradeValue);
		if (!FMath::IsNearlyEqual(CalculatedValue, static_cast<float>(OptionDTO.FinalValue)))
		{
			FRONTIER_LOG(Warning, TEXT("Backend random option finalValue differs from local debug calculation. ItemInstanceId=%s OptionId=%s Calculated=%.4f Final=%.4f"),
				*DTO.ItemInstanceId,
				*OptionDTO.OptionId,
				CalculatedValue,
				static_cast<float>(OptionDTO.FinalValue));
		}

		FFrontierRuntimeStatData& RuntimeGeneratedStat = RuntimeItem.RuntimeGeneratedStats.AddDefaulted_GetRef();
		RuntimeGeneratedStat.OptionId = OptionDTO.OptionId;
		RuntimeGeneratedStat.StatTag = StatTag;
		RuntimeGeneratedStat.Unit = OptionDTO.Unit;
		RuntimeGeneratedStat.BaseValue = static_cast<float>(OptionDTO.BaseValue);
		RuntimeGeneratedStat.RandomValue = static_cast<float>(OptionDTO.RandomValue);
		RuntimeGeneratedStat.UpgradeValue = static_cast<float>(OptionDTO.UpgradeValue);
		RuntimeGeneratedStat.FinalValue = static_cast<float>(OptionDTO.FinalValue);
		RuntimeGeneratedStat.NormalizedValue = static_cast<float>(OptionDTO.NormalizedValue);
		RuntimeGeneratedStat.bHasNormalizedValue = OptionDTO.bHasNormalizedValue;

	}

	TSet<int32> SeenSkillSlots;
	TSet<FString> SeenSkillIds;
	for (const FFrontierGeneratedItemSkillDTO& SkillDTO : ResolvedGeneratedSkills)
	{
		const FString& SkillLookupId = SkillDTO.SkillId;
		if (SkillLookupId.IsEmpty())
		{
			FRONTIER_LOG(Error, TEXT("Generated skill rejected by empty SkillId. ItemInstanceId=%s ItemTemplateId=%s SlotIndex=%d"),
				*DTO.ItemInstanceId,
				*DTO.ItemTemplateId,
				SkillDTO.SlotIndex);
			continue;
		}
		if (SkillDTO.SlotIndex < 0 || SeenSkillSlots.Contains(SkillDTO.SlotIndex))
		{
			FRONTIER_LOG(Error, TEXT("Generated skill rejected by invalid or duplicate SlotIndex. ItemInstanceId=%s ItemTemplateId=%s SkillId=%s SlotIndex=%d"),
				*DTO.ItemInstanceId,
				*DTO.ItemTemplateId,
				*SkillLookupId,
				SkillDTO.SlotIndex);
			continue;
		}
		if (SkillDTO.Level < 1)
		{
			FRONTIER_LOG(Error, TEXT("Generated skill rejected by invalid level. ItemInstanceId=%s ItemTemplateId=%s SkillId=%s Level=%d"),
				*DTO.ItemInstanceId,
				*DTO.ItemTemplateId,
				*SkillLookupId,
				SkillDTO.Level);
			continue;
		}
		if (SeenSkillIds.Contains(SkillLookupId))
		{
			FRONTIER_LOG(Error, TEXT("Generated skill rejected by duplicate SkillId. ItemInstanceId=%s ItemTemplateId=%s SkillId=%s"),
				*DTO.ItemInstanceId,
				*DTO.ItemTemplateId,
				*SkillLookupId);
			continue;
		}

		SeenSkillSlots.Add(SkillDTO.SlotIndex);
		SeenSkillIds.Add(SkillLookupId);

		FFrontierRuntimeSkillData& RuntimeGeneratedSkill = RuntimeItem.RuntimeGeneratedSkills.AddDefaulted_GetRef();
		RuntimeGeneratedSkill.SkillTemplateId = SkillLookupId;
		if (!SkillDTO.SkillTag.IsEmpty())
		{
			RuntimeGeneratedSkill.SkillTag = FGameplayTag::RequestGameplayTag(
				FName(*SkillDTO.SkillTag),
				false);
		}
		RuntimeGeneratedSkill.SkillRarity = SkillDTO.Rarity;
		RuntimeGeneratedSkill.SkillElementalType = SkillDTO.ElementalType;
		RuntimeGeneratedSkill.SkillLevel = SkillDTO.Level;
		RuntimeGeneratedSkill.SlotIndex = SkillDTO.SlotIndex;

	}

	OutItemInstance = MoveTemp(RuntimeItem);
	return true;
}

bool FFrontierItemPersistenceMapper::TryUpdateDTOFromRuntimeItem(
	const FFrontierItemInstance& ItemInstance,
	FFrontierItemPersistenceDTO& InOutDTO,
	FString& OutError)
{
	using namespace FrontierItemPersistence;

	OutError.Reset();
	if (!ItemInstance.IsValid() || !ItemInstance.ItemInstanceId.IsValid())
	{
		OutError = TEXT("Runtime item is invalid or has no persistent itemInstanceId.");
		return false;
	}

	FGuid DTOItemInstanceId;
	if (!TryParseItemInstanceId(InOutDTO.ItemInstanceId, DTOItemInstanceId, OutError))
	{
		return false;
	}
	if (DTOItemInstanceId != ItemInstance.ItemInstanceId)
	{
		OutError = TEXT("Runtime itemInstanceId does not match the existing ItemDTO.");
		return false;
	}

	const FString RuntimeTemplateId = ItemInstance.GetTemplateId().ToString();
	if (RuntimeTemplateId.IsEmpty() || RuntimeTemplateId != InOutDTO.ItemTemplateId)
	{
		OutError = TEXT("Runtime itemTemplateId does not match the existing ItemDTO.");
		return false;
	}
	if (ItemInstance.Quantity <= 0 || ItemInstance.EnhancementLevel < 0)
	{
		OutError = TEXT("Runtime item contains invalid persistent values.");
		return false;
	}
	if (InOutDTO.Durability.IsSet() && ItemInstance.Durability < 0.0f)
	{
		OutError = TEXT("Runtime durability cannot be negative.");
		return false;
	}
	TSet<FString> RuntimeOptionIds;
	for (const FString& InstanceTag : ItemInstance.InstanceTags)
	{
		if (InstanceTag.IsEmpty())
		{
			OutError = TEXT("Runtime item contains an invalid instance tag.");
			return false;
		}
	}
	for (const FFrontierRuntimeStatData& RuntimeStat : ItemInstance.RuntimeGeneratedStats)
	{
		const FString OptionId = !RuntimeStat.OptionId.IsEmpty() ? RuntimeStat.OptionId : RuntimeStat.StatTag.ToString();
		if (OptionId.IsEmpty() || RuntimeOptionIds.Contains(OptionId))
		{
			OutError = TEXT("Runtime item contains an invalid or duplicate random option ID.");
			return false;
		}
		RuntimeOptionIds.Add(OptionId);

		if (!FMath::IsFinite(RuntimeStat.BaseValue)
			|| !FMath::IsFinite(RuntimeStat.RandomValue)
			|| !FMath::IsFinite(RuntimeStat.UpgradeValue)
			|| !FMath::IsFinite(RuntimeStat.FinalValue)
			|| !FMath::IsFinite(RuntimeStat.NormalizedValue))
		{
			OutError = TEXT("Runtime item contains a non-finite persistent stat value.");
			return false;
		}
	}

	TSet<int32> RuntimeSkillSlots;
	for (const FFrontierRuntimeSkillData& RuntimeSkill : ItemInstance.RuntimeGeneratedSkills)
	{
		if ((RuntimeSkill.SkillTemplateId.IsEmpty() && !RuntimeSkill.SkillTag.IsValid())
			|| RuntimeSkill.SkillLevel < 1
			|| RuntimeSkill.SlotIndex < 0
			|| RuntimeSkillSlots.Contains(RuntimeSkill.SlotIndex))
		{
			OutError = TEXT("Runtime item contains an invalid or duplicate generated skill slot.");
			return false;
		}
		RuntimeSkillSlots.Add(RuntimeSkill.SlotIndex);
	}

	FFrontierItemPersistenceDTO UpdatedDTO = InOutDTO;
	UpdatedDTO.ItemInstanceId = FormatItemInstanceId(ItemInstance.ItemInstanceId);
	UpdatedDTO.Quantity = ItemInstance.Quantity;
	if (UpdatedDTO.Durability.IsSet())
	{
		UpdatedDTO.Durability = static_cast<double>(ItemInstance.Durability);
	}
	UpdatedDTO.EnhancementLevel = ItemInstance.EnhancementLevel;
	UpdatedDTO.FinalRarityTag = ConvertItemRarityToBackendString(ItemInstance.GetDisplayRarity());
	UpdatedDTO.BindState = ItemInstance.BindState;

	UpdatedDTO.InstanceTags.Reset(ItemInstance.InstanceTags.Num());
	for (const FString& InstanceTag : ItemInstance.InstanceTags)
	{
		UpdatedDTO.InstanceTags.Add(InstanceTag);
	}

	const int32 RuntimeStatCount = ItemInstance.RuntimeGeneratedStats.Num();
	UpdatedDTO.RandomOptions.Reset(RuntimeStatCount);
	for (const FFrontierRuntimeStatData& RuntimeStat : ItemInstance.RuntimeGeneratedStats)
	{
		FFrontierItemOptionDTO& OptionDTO = UpdatedDTO.RandomOptions.AddDefaulted_GetRef();
		OptionDTO.OptionId = !RuntimeStat.OptionId.IsEmpty() ? RuntimeStat.OptionId : RuntimeStat.StatTag.ToString();
		OptionDTO.Unit = RuntimeStat.Unit;
		OptionDTO.BaseValue = RuntimeStat.BaseValue;
		OptionDTO.RandomValue = RuntimeStat.RandomValue;
		OptionDTO.UpgradeValue = RuntimeStat.UpgradeValue;
		OptionDTO.FinalValue = RuntimeStat.FinalValue;
		OptionDTO.NormalizedValue = RuntimeStat.NormalizedValue;
		OptionDTO.bHasNormalizedValue = RuntimeStat.bHasNormalizedValue;
	}

	UpdatedDTO.GeneratedSkills.Reset(ItemInstance.RuntimeGeneratedSkills.Num());
	for (const FFrontierRuntimeSkillData& RuntimeSkill : ItemInstance.RuntimeGeneratedSkills)
	{
		FFrontierGeneratedItemSkillDTO& SkillDTO = UpdatedDTO.GeneratedSkills.AddDefaulted_GetRef();
		SkillDTO.SkillId = RuntimeSkill.SkillTemplateId;
		SkillDTO.Level = RuntimeSkill.SkillLevel;
		SkillDTO.SlotIndex = RuntimeSkill.SlotIndex;
	}

	InOutDTO = MoveTemp(UpdatedDTO);
	return true;
}
