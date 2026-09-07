#include "FrontierOnlineHttpClient.h"

#include "FrontierOnline.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
FString DescribeJsonFields(const TSharedPtr<FJsonObject>& Object)
{
	if (!Object.IsValid())
	{
		return TEXT("<none>");
	}

	TArray<FString> Fields;
	Object->Values.GetKeys(Fields);
	Fields.Sort();
	return Fields.IsEmpty() ? TEXT("<none>") : FString::Join(Fields, TEXT(","));
}

FString ExtractServerMessage(const TSharedPtr<FJsonObject>& RootObject)
{
	if (!RootObject.IsValid())
	{
		return FString();
	}

	FString Message;
	if (RootObject->TryGetStringField(TEXT("message"), Message))
	{
		return Message;
	}

	const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
	if (RootObject->TryGetObjectField(TEXT("error"), ErrorObject) && ErrorObject && ErrorObject->IsValid())
	{
		(*ErrorObject)->TryGetStringField(TEXT("message"), Message);
	}
	return Message;
}

FString BuildHttpFailureMessage(const int32 HttpStatus, const TSharedPtr<FJsonObject>& RootObject)
{
	FString Message;
	switch (HttpStatus)
	{
	case 400:
		Message = TEXT("잘못된 Steam 인증 티켓입니다.");
		break;
	case 401:
		Message = TEXT("Steam 인증이 거부되었습니다. 인증 티켓이 만료되었거나 서버가 요구하는 티켓 형식과 다릅니다.");
		break;
	case 500:
		Message = TEXT("Steam 연동 처리 중 서버 오류가 발생했습니다.");
		break;
	default:
		Message = FString::Printf(TEXT("Steam 로그인 요청에 실패했습니다. HTTP %d"), HttpStatus);
		break;
	}

	const FString ServerMessage = ExtractServerMessage(RootObject);
	if (!ServerMessage.IsEmpty())
	{
		Message += FString::Printf(TEXT(" (%s)"), *ServerMessage);
	}
	return Message;
}

bool ParseNullableString(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, bool& OutHasValue, FString& OutValue)
{
	OutHasValue = false;
	OutValue.Reset();
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		return true;
	}

	const TSharedPtr<FJsonValue> FieldValue = Object->TryGetField(FieldName);
	if (!FieldValue.IsValid() || FieldValue->Type == EJson::Null)
	{
		return true;
	}

	if (FieldValue->Type != EJson::String)
	{
		return false;
	}

	OutHasValue = true;
	OutValue = FieldValue->AsString();
	return true;
}

bool ParseNullableDouble(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, bool& OutHasValue, double& OutValue)
{
	OutHasValue = false;
	OutValue = 0.0;
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		return true;
	}

	const TSharedPtr<FJsonValue> FieldValue = Object->TryGetField(FieldName);
	if (!FieldValue.IsValid() || FieldValue->Type == EJson::Null)
	{
		return true;
	}

	if (FieldValue->Type != EJson::Number)
	{
		return false;
	}

	OutHasValue = true;
	OutValue = FieldValue->AsNumber();
	return FMath::IsFinite(OutValue);
}

bool ParseNullableInt32(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, bool& OutHasValue, int32& OutValue)
{
	double ParsedValue = 0.0;
	if (!ParseNullableDouble(Object, FieldName, OutHasValue, ParsedValue))
	{
		return false;
	}
	if (!OutHasValue)
	{
		OutValue = 0;
		return true;
	}

	const int64 RoundedValue = FMath::RoundToInt64(ParsedValue);
	if (!FMath::IsNearlyEqual(ParsedValue, static_cast<double>(RoundedValue))
		|| RoundedValue < TNumericLimits<int32>::Lowest()
		|| RoundedValue > TNumericLimits<int32>::Max())
	{
		return false;
	}

	OutValue = static_cast<int32>(RoundedValue);
	return true;
}

bool ParseRequiredString(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, FString& OutValue, FString& OutError)
{
	if (!Object.IsValid() || !Object->TryGetStringField(FieldName, OutValue))
	{
		OutError = FString::Printf(TEXT("Missing or invalid string field: %s"), FieldName);
		return false;
	}
	return true;
}

bool ParseRequiredBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, bool& OutValue, FString& OutError)
{
	if (!Object.IsValid() || !Object->TryGetBoolField(FieldName, OutValue))
	{
		OutError = FString::Printf(TEXT("Missing or invalid bool field: %s"), FieldName);
		return false;
	}
	return true;
}

bool ParseRequiredInt32(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue, FString& OutError)
{
	double NumberValue = 0.0;
	if (!Object.IsValid() || !Object->TryGetNumberField(FieldName, NumberValue))
	{
		OutError = FString::Printf(TEXT("Missing or invalid numeric field: %s"), FieldName);
		return false;
	}

	const int64 RoundedValue = FMath::RoundToInt64(NumberValue);
	if (!FMath::IsFinite(NumberValue)
		|| !FMath::IsNearlyEqual(NumberValue, static_cast<double>(RoundedValue))
		|| RoundedValue < TNumericLimits<int32>::Lowest()
		|| RoundedValue > TNumericLimits<int32>::Max())
	{
		OutError = FString::Printf(TEXT("Numeric field is outside int32 range: %s"), FieldName);
		return false;
	}

	OutValue = static_cast<int32>(RoundedValue);
	return true;
}

bool ParseRequiredInt64(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int64& OutValue, FString& OutError)
{
	double NumberValue = 0.0;
	if (!Object.IsValid() || !Object->TryGetNumberField(FieldName, NumberValue))
	{
		OutError = FString::Printf(TEXT("Missing or invalid numeric field: %s"), FieldName);
		return false;
	}

	const int64 RoundedValue = FMath::RoundToInt64(NumberValue);
	if (!FMath::IsFinite(NumberValue) || !FMath::IsNearlyEqual(NumberValue, static_cast<double>(RoundedValue)))
	{
		OutError = FString::Printf(TEXT("Numeric field is outside int64 range: %s"), FieldName);
		return false;
	}

	OutValue = RoundedValue;
	return true;
}

bool ParseRequiredContainerId(
	const TSharedPtr<FJsonObject>& Object,
	FString& OutValue,
	FString& OutError)
{
	if (Object.IsValid()
		&& (Object->TryGetStringField(TEXT("containerId"), OutValue)
			|| Object->TryGetStringField(TEXT("inventoryId"), OutValue)
			|| Object->TryGetStringField(TEXT("storageId"), OutValue)
			|| Object->TryGetStringField(TEXT("equipmentId"), OutValue)))
	{
		return true;
	}

	OutError = TEXT("Missing or invalid containerId field.");
	return false;
}

bool ParseRequiredIdentifier(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FString& OutString,
	int64& OutNumericId,
	FString& OutError)
{
	OutString.Reset();
	OutNumericId = 0;
	const TSharedPtr<FJsonValue> Value = Object.IsValid() ? Object->TryGetField(FieldName) : nullptr;
	if (!Value.IsValid())
	{
		OutError = FString::Printf(TEXT("Missing identifier field: %s"), FieldName);
		return false;
	}

	if (Value->Type == EJson::String)
	{
		OutString = Value->AsString();
		if (OutString.IsNumeric())
		{
			const int64 NumericValue = FCString::Atoi64(*OutString);
			if (NumericValue > 0)
			{
				OutNumericId = NumericValue;
			}
		}
	}
	else if (Value->Type == EJson::Number)
	{
		const double Number = Value->AsNumber();
		const int64 Rounded = FMath::RoundToInt64(Number);
		if (!FMath::IsFinite(Number) || !FMath::IsNearlyEqual(Number, static_cast<double>(Rounded)) || Rounded <= 0)
		{
			OutError = FString::Printf(TEXT("Invalid numeric identifier field: %s"), FieldName);
			return false;
		}
		OutString = LexToString(Rounded);
		OutNumericId = Rounded;
	}
	else
	{
		OutError = FString::Printf(TEXT("Identifier field must be a string or integer: %s"), FieldName);
		return false;
	}

	if (OutString.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Identifier field is empty: %s"), FieldName);
		return false;
	}
	return true;
}

bool ParsePlayerLevelDataObject(
	const TSharedPtr<FJsonObject>& Object,
	FFrontierOnlinePlayerLevelDTO& OutLevel,
	FString& OutError)
{
	if (!ParseRequiredInt32(Object, TEXT("level"), OutLevel.Level, OutError)
		|| !ParseRequiredInt64(Object, TEXT("totalExperience"), OutLevel.TotalExperience, OutError)
		|| !ParseRequiredInt64(Object, TEXT("currentLevelExperience"), OutLevel.CurrentLevelExperience, OutError)
		|| !ParseRequiredInt32(Object, TEXT("maxLevel"), OutLevel.MaxLevel, OutError))
	{
		return false;
	}

	OutLevel.bHasNextLevelRequiredExperience = false;
	OutLevel.NextLevelRequiredExperience = 0;
	const TSharedPtr<FJsonValue> NextLevelValue = Object->TryGetField(TEXT("nextLevelRequiredExperience"));
	if (NextLevelValue.IsValid() && NextLevelValue->Type != EJson::Null)
	{
		if (NextLevelValue->Type != EJson::Number)
		{
			OutError = TEXT("nextLevelRequiredExperience must be an integer or null.");
			return false;
		}
		const double Number = NextLevelValue->AsNumber();
		const int64 Rounded = FMath::RoundToInt64(Number);
		if (!FMath::IsFinite(Number) || !FMath::IsNearlyEqual(Number, static_cast<double>(Rounded)) || Rounded < 0)
		{
			OutError = TEXT("nextLevelRequiredExperience is invalid.");
			return false;
		}
		OutLevel.bHasNextLevelRequiredExperience = true;
		OutLevel.NextLevelRequiredExperience = Rounded;
	}

	Object->TryGetStringField(TEXT("updatedAt"), OutLevel.UpdatedAt);
	if (OutLevel.Level <= 0 || OutLevel.MaxLevel <= 0 || OutLevel.Level > OutLevel.MaxLevel
		|| OutLevel.TotalExperience < 0 || OutLevel.CurrentLevelExperience < 0)
	{
		OutError = TEXT("PlayerLevelDTO contains invalid progression values.");
		return false;
	}
	return true;
}

void ParseResponseMeta(const TSharedPtr<FJsonObject>& RootObject, FString& OutRequestId, FString& OutServerTime)
{
	const TSharedPtr<FJsonObject>* MetaObject = nullptr;
	if (RootObject.IsValid() && RootObject->TryGetObjectField(TEXT("meta"), MetaObject) && MetaObject && MetaObject->IsValid())
	{
		(*MetaObject)->TryGetStringField(TEXT("requestId"), OutRequestId);
		(*MetaObject)->TryGetStringField(TEXT("serverTime"), OutServerTime);
	}
}

void ParseErrorFields(const TSharedPtr<FJsonObject>& RootObject, FString& OutErrorCode, FString& OutMessage, bool& bOutRetryable)
{
	bOutRetryable = false;
	if (!RootObject.IsValid())
	{
		return;
	}

	RootObject->TryGetStringField(TEXT("message"), OutMessage);
	const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
	if (RootObject->TryGetObjectField(TEXT("error"), ErrorObject) && ErrorObject && ErrorObject->IsValid())
	{
		(*ErrorObject)->TryGetStringField(TEXT("code"), OutErrorCode);
		(*ErrorObject)->TryGetStringField(TEXT("message"), OutMessage);
		(*ErrorObject)->TryGetBoolField(TEXT("retryable"), bOutRetryable);
	}
}

bool SerializeJsonObject(const TSharedPtr<FJsonObject>& Object, FString& OutJson)
{
	OutJson.Reset();
	if (!Object.IsValid())
	{
		return false;
	}

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	return FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
}

bool SerializeJsonArray(
	const TArray<TSharedPtr<FJsonValue>>& Values,
	FString& OutJson)
{
	OutJson.Reset();
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	return FJsonSerializer::Serialize(Values, Writer);
}

bool ParsePartyDataObject(
	const TSharedPtr<FJsonObject>& Object,
	FFrontierOnlinePartyDTO& OutParty,
	FString& OutError)
{
	if (!ParseRequiredString(Object, TEXT("partyId"), OutParty.PartyId, OutError)
		|| !ParseRequiredString(Object, TEXT("steamLobbyId"), OutParty.SteamLobbyId, OutError)
		|| !ParseRequiredInt64(Object, TEXT("leaderPlayerId"), OutParty.LeaderPlayerId, OutError)
		|| !ParseRequiredString(Object, TEXT("status"), OutParty.Status, OutError))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
	if (!Object->TryGetArrayField(TEXT("members"), Members) || !Members)
	{
		OutError = TEXT("Party response has no members array.");
		return false;
	}

	OutParty.Members.Reset();
	OutParty.Members.Reserve(Members->Num());
	TSet<int64> SeenPlayerIds;
	for (const TSharedPtr<FJsonValue>& MemberValue : *Members)
	{
		if (!MemberValue.IsValid() || MemberValue->Type != EJson::Object)
		{
			OutError = TEXT("Party members contains a non-object entry.");
			return false;
		}

		const TSharedPtr<FJsonObject> MemberObject = MemberValue->AsObject();
		FFrontierOnlinePartyMemberDTO& Member = OutParty.Members.AddDefaulted_GetRef();
		if (!ParseRequiredInt64(MemberObject, TEXT("playerId"), Member.PlayerId, OutError)
			|| !ParseRequiredString(MemberObject, TEXT("steamId"), Member.SteamId, OutError)
			|| !ParseRequiredBool(MemberObject, TEXT("isLeader"), Member.bIsLeader, OutError))
		{
			return false;
		}
		if (Member.PlayerId <= 0)
		{
			OutError = TEXT("Party response contains a non-positive playerId.");
			return false;
		}
		if (SeenPlayerIds.Contains(Member.PlayerId))
		{
			OutError = FString::Printf(
				TEXT("Party response contains duplicate playerId: %lld"),
				static_cast<long long>(Member.PlayerId));
			return false;
		}
		SeenPlayerIds.Add(Member.PlayerId);
	}

	if (OutParty.PartyId.IsEmpty() || OutParty.SteamLobbyId.IsEmpty()
		|| OutParty.LeaderPlayerId <= 0 || OutParty.Members.IsEmpty())
	{
		OutError = TEXT("Party response contains an empty required value.");
		return false;
	}
	return true;
}

bool ParseInventoryHeader(const TSharedPtr<FJsonObject>& Object, FFrontierOnlineInventoryHeader& OutHeader, FString& OutError)
{
	if (!ParseRequiredContainerId(Object, OutHeader.InventoryId, OutError)
		|| !ParseRequiredInt64(Object, TEXT("ownerPlayerId"), OutHeader.OwnerPlayerId, OutError)
		|| !ParseRequiredInt32(Object, TEXT("slotCapacity"), OutHeader.SlotCapacity, OutError)
		|| !ParseRequiredInt32(Object, TEXT("capacityLevel"), OutHeader.CapacityLevel, OutError))
	{
		return false;
	}
	Object->TryGetStringField(TEXT("updatedAt"), OutHeader.UpdatedAt);
	if (OutHeader.SlotCapacity <= 0)
	{
		OutError = TEXT("inventory.container.slotCapacity must be greater than zero.");
		return false;
	}
	return true;
}

bool ParseStorageHeader(const TSharedPtr<FJsonObject>& Object, FFrontierOnlineStorageHeader& OutHeader, FString& OutError)
{
	if (!ParseRequiredContainerId(Object, OutHeader.StorageId, OutError)
		|| !ParseRequiredInt64(Object, TEXT("ownerPlayerId"), OutHeader.OwnerPlayerId, OutError)
		|| !ParseRequiredInt32(Object, TEXT("slotCapacity"), OutHeader.SlotCapacity, OutError)
		|| !ParseRequiredInt32(Object, TEXT("capacityLevel"), OutHeader.CapacityLevel, OutError))
	{
		return false;
	}
	Object->TryGetStringField(TEXT("updatedAt"), OutHeader.UpdatedAt);
	if (OutHeader.SlotCapacity <= 0)
	{
		OutError = TEXT("storage.container.slotCapacity must be greater than zero.");
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

bool ParseItemOptions(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	TArray<FFrontierOnlineItemOption>& OutOptions,
	FString& OutError)
{
	OutOptions.Reset();
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		return true;
	}

	const TArray<TSharedPtr<FJsonValue>>* OptionsArray = nullptr;
	if (!Object->TryGetArrayField(FieldName, OptionsArray) || !OptionsArray)
	{
		OutError = FString::Printf(TEXT("%s must be an array."), FieldName);
		return false;
	}

	TSet<FString> SeenOptionIds;
	OutOptions.Reserve(OptionsArray->Num());
	for (const TSharedPtr<FJsonValue>& OptionValue : *OptionsArray)
	{
		if (!OptionValue.IsValid() || OptionValue->Type != EJson::Object)
		{
			OutError = FString::Printf(TEXT("%s contains a non-object entry."), FieldName);
			return false;
		}

		const TSharedPtr<FJsonObject> OptionObject = OptionValue->AsObject();
		FFrontierOnlineItemOption& Option = OutOptions.AddDefaulted_GetRef();
		if (!TryGetStringAlias(OptionObject, { TEXT("optionId"), TEXT("statTag"), TEXT("statId") }, Option.OptionId) || Option.OptionId.IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s.optionId is required."), FieldName);
			return false;
		}
		if (SeenOptionIds.Contains(Option.OptionId))
		{
			OutError = FString::Printf(TEXT("%s contains duplicate optionId: %s"), FieldName, *Option.OptionId);
			return false;
		}
		SeenOptionIds.Add(Option.OptionId);
		OptionObject->TryGetStringField(TEXT("unit"), Option.Unit);

		(void)TryGetNumberAlias(OptionObject, { TEXT("baseValue") }, Option.BaseValue);
		if (!TryGetNumberAlias(OptionObject, { TEXT("randomValue"), TEXT("rolledBonus"), TEXT("rolledRandomBonus"), TEXT("randomBonus") }, Option.RandomValue))
		{
			double RolledValue = 0.0;
			if (TryGetNumberAlias(OptionObject, { TEXT("rolledValue") }, RolledValue))
			{
				Option.RandomValue = RolledValue - Option.BaseValue;
			}
		}
		(void)TryGetNumberAlias(OptionObject, { TEXT("upgradeValue"), TEXT("enhancementBonus") }, Option.UpgradeValue);
		double ReforgeBonus = 0.0;
		if (TryGetNumberAlias(OptionObject, { TEXT("reforgeBonus") }, ReforgeBonus))
		{
			Option.UpgradeValue += ReforgeBonus;
		}
		if (!TryGetNumberAlias(OptionObject, { TEXT("finalValue"), TEXT("currentFinalValue"), TEXT("value") }, Option.FinalValue))
		{
			Option.FinalValue = Option.BaseValue + Option.RandomValue + Option.UpgradeValue;
		}
		Option.bHasNormalizedValue = TryGetNumberAlias(
			OptionObject,
			{ TEXT("normalizedValue"), TEXT("normalizedScore") },
			Option.NormalizedValue);
	}

	return true;
}

bool ParseGeneratedItemSkills(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	TArray<FFrontierOnlineGeneratedItemSkill>& OutSkills,
	FString& OutError)
{
	OutSkills.Reset();
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		return true;
	}

	const TArray<TSharedPtr<FJsonValue>>* SkillsArray = nullptr;
	if (!Object->TryGetArrayField(FieldName, SkillsArray) || !SkillsArray)
	{
		OutError = FString::Printf(TEXT("%s must be an array."), FieldName);
		return false;
	}

	TSet<int32> SeenSkillSlots;
	OutSkills.Reserve(SkillsArray->Num());
	for (int32 Index = 0; Index < SkillsArray->Num(); ++Index)
	{
		const TSharedPtr<FJsonValue>& SkillValue = (*SkillsArray)[Index];
		if (!SkillValue.IsValid() || SkillValue->Type != EJson::Object)
		{
			OutError = FString::Printf(TEXT("%s contains a non-object entry."), FieldName);
			return false;
		}

		const TSharedPtr<FJsonObject> SkillObject = SkillValue->AsObject();
		FFrontierOnlineGeneratedItemSkill Skill;
		(void)TryGetStringAlias(SkillObject, { TEXT("skillId"), TEXT("skillTemplateId"), TEXT("skillTag") }, Skill.SkillId);
		if (Skill.SkillId.IsEmpty())
		{
			continue;
		}

		double SlotIndexNumber = static_cast<double>(Index);
		(void)TryGetNumberAlias(SkillObject, { TEXT("slotIndex") }, SlotIndexNumber);
		const int64 RoundedSlotIndex = FMath::RoundToInt64(SlotIndexNumber);
		if (!FMath::IsNearlyEqual(SlotIndexNumber, static_cast<double>(RoundedSlotIndex))
			|| RoundedSlotIndex < 0
			|| RoundedSlotIndex > 2
			|| SeenSkillSlots.Contains(static_cast<int32>(RoundedSlotIndex)))
		{
			OutError = FString::Printf(TEXT("%s contains an invalid or duplicate slotIndex: %lld"), FieldName, RoundedSlotIndex);
			return false;
		}

		Skill.SlotIndex = static_cast<int32>(RoundedSlotIndex);
		SeenSkillSlots.Add(Skill.SlotIndex);
		double SkillLevelNumber = 1.0;
		(void)TryGetNumberAlias(SkillObject, { TEXT("skillLevel"), TEXT("level") }, SkillLevelNumber);
		const int64 RoundedSkillLevel = FMath::RoundToInt64(SkillLevelNumber);
		if (!FMath::IsNearlyEqual(SkillLevelNumber, static_cast<double>(RoundedSkillLevel))
			|| RoundedSkillLevel < TNumericLimits<int32>::Lowest()
			|| RoundedSkillLevel > TNumericLimits<int32>::Max())
		{
			OutError = FString::Printf(TEXT("%s.level must be an integer for skillId: %s"), FieldName, *Skill.SkillId);
			return false;
		}
		Skill.Level = static_cast<int32>(RoundedSkillLevel);
		OutSkills.Add(MoveTemp(Skill));
	}

	return true;
}

bool ParseOnlineItemDTO(const TSharedPtr<FJsonObject>& Object, FFrontierOnlineItemDTO& OutItem, FString& OutError, const TCHAR* Context)
{
	if (!Object.IsValid())
	{
		OutError = FString::Printf(TEXT("%s item entry is not an object."), Context);
		return false;
	}

	if (!ParseRequiredString(Object, TEXT("itemInstanceId"), OutItem.ItemInstanceId, OutError)
		|| !ParseRequiredString(Object, TEXT("itemTemplateId"), OutItem.ItemTemplateId, OutError)
		|| !ParseRequiredInt32(Object, TEXT("enhancementLevel"), OutItem.EnhancementLevel, OutError)
		|| !ParseRequiredString(Object, TEXT("finalRarityTag"), OutItem.FinalRarityTag, OutError)
		|| !ParseRequiredString(Object, TEXT("bindState"), OutItem.BindState, OutError))
	{
		return false;
	}

	if (!TryGetStringAlias(Object, { TEXT("rarity"), TEXT("finalRarityTag"), TEXT("finalRarity") }, OutItem.FinalRarityTag)
		|| OutItem.FinalRarityTag.IsEmpty())
	{
		OutError = TEXT("rarity/finalRarityTag is required.");
		return false;
	}

	if (!ParseNullableDouble(Object, TEXT("durability"), OutItem.bHasDurability, OutItem.Durability))
	{
		OutError = TEXT("durability must be a number or null.");
		return false;
	}
	if (OutItem.bHasDurability && OutItem.Durability < 0.0)
	{
		OutError = TEXT("durability cannot be negative.");
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* TagsArray = nullptr;
	if (Object->TryGetArrayField(TEXT("instanceTags"), TagsArray) && TagsArray)
	{
		for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
		{
			if (!TagValue.IsValid() || TagValue->Type != EJson::String)
			{
				OutError = FString::Printf(TEXT("%s item instanceTags must contain only strings."), Context);
				return false;
			}
			OutItem.InstanceTags.Add(TagValue->AsString());
		}
	}

	const TCHAR* StatsFieldName = Object->HasField(TEXT("generatedStats")) ? TEXT("generatedStats") : TEXT("randomOptions");
	if (!ParseItemOptions(Object, StatsFieldName, OutItem.RandomOptions, OutError)
		|| !ParseGeneratedItemSkills(Object, TEXT("generatedSkills"), OutItem.GeneratedSkills, OutError))
	{
		return false;
	}

	Object->TryGetStringField(TEXT("createdAt"), OutItem.CreatedAt);
	Object->TryGetStringField(TEXT("acquiredAt"), OutItem.AcquiredAt);
	Object->TryGetStringField(TEXT("updatedAt"), OutItem.UpdatedAt);

	const TSharedPtr<FJsonObject>* MetadataObject = nullptr;
	if (Object->TryGetObjectField(TEXT("metadata"), MetadataObject) && MetadataObject && MetadataObject->IsValid())
	{
		OutItem.bHasMetadata = true;
		if (!SerializeJsonObject(*MetadataObject, OutItem.MetadataJson))
		{
			OutError = FString::Printf(TEXT("%s item metadata object could not be serialized."), Context);
			return false;
		}
	}
	else
	{
		bool bHasMetadataString = false;
		if (!ParseNullableString(Object, TEXT("metadata"), bHasMetadataString, OutItem.MetadataJson))
		{
			OutError = FString::Printf(TEXT("%s item metadata must be an object, string, or null."), Context);
			return false;
		}
		OutItem.bHasMetadata = bHasMetadataString;
	}

	return true;
}

bool ParseInventoryItem(const TSharedPtr<FJsonObject>& Object, FFrontierOnlineItemDTO& OutItem, FString& OutError)
{
	if (!ParseOnlineItemDTO(Object, OutItem, OutError, TEXT("inventory")))
	{
		return false;
	}

	if (!ParseRequiredInt32(Object, TEXT("slotIndex"), OutItem.SlotIndex, OutError)
		|| !ParseRequiredInt32(Object, TEXT("quantity"), OutItem.Quantity, OutError))
	{
		return false;
	}
	if (OutItem.Quantity <= 0)
	{
		OutError = TEXT("inventory item quantity must be greater than zero.");
		return false;
	}

	return true;
}

bool ParseEquipmentHeader(const TSharedPtr<FJsonObject>& Object, FFrontierOnlineEquipmentHeader& OutHeader, FString& OutError)
{
	if (!ParseRequiredContainerId(Object, OutHeader.EquipmentId, OutError)
		|| !ParseRequiredInt64(Object, TEXT("ownerPlayerId"), OutHeader.OwnerPlayerId, OutError))
	{
		return false;
	}
	return true;
}

bool ParseEquipmentSlot(const TSharedPtr<FJsonObject>& Object, FFrontierOnlineEquipmentSlot& OutSlot, FString& OutError)
{
	if (!Object.IsValid())
	{
		OutError = TEXT("equipment slot entry is not an object.");
		return false;
	}

	if (!ParseRequiredString(Object, TEXT("slotType"), OutSlot.SlotType, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonValue> ItemValue = Object->TryGetField(TEXT("item"));
	if (!ItemValue.IsValid() || ItemValue->Type == EJson::Null)
	{
		OutSlot.bHasItem = false;
		return true;
	}
	if (ItemValue->Type != EJson::Object)
	{
		OutError = FString::Printf(TEXT("equipment slot item must be an object or null. SlotType=%s"), *OutSlot.SlotType);
		return false;
	}

	OutSlot.bHasItem = true;
	return ParseOnlineItemDTO(ItemValue->AsObject(), OutSlot.Item, OutError, TEXT("equipment"));
}

bool ParseInventoryUpgrade(const TSharedPtr<FJsonObject>& Object, FFrontierOnlineInventoryUpgrade& OutUpgrade, FString& OutError)
{
	if (!Object.IsValid())
	{
		return true;
	}

	bool bHasCapacityLevel = false;
	if (!ParseNullableInt32(Object, TEXT("capacityLevel"), bHasCapacityLevel, OutUpgrade.CapacityLevel))
	{
		OutError = TEXT("upgrade.capacityLevel must be an integer or null.");
		return false;
	}
	bool bHasMaxCapacityLevel = false;
	if (!ParseNullableInt32(Object, TEXT("maxCapacityLevel"), bHasMaxCapacityLevel, OutUpgrade.MaxCapacityLevel))
	{
		OutError = TEXT("upgrade.maxCapacityLevel must be an integer or null.");
		return false;
	}
	if (!ParseNullableInt32(Object, TEXT("nextCapacityLevel"), OutUpgrade.bHasNextCapacityLevel, OutUpgrade.NextCapacityLevel))
	{
		OutError = TEXT("upgrade.nextCapacityLevel must be an integer or null.");
		return false;
	}
	if (!ParseNullableInt32(Object, TEXT("nextSlotCapacity"), OutUpgrade.bHasNextSlotCapacity, OutUpgrade.NextSlotCapacity))
	{
		OutError = TEXT("upgrade.nextSlotCapacity must be an integer or null.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* CostArray = nullptr;
	if (Object->TryGetArrayField(TEXT("nextUpgradeCost"), CostArray) && CostArray)
	{
		OutUpgrade.NextUpgradeCost.Reset(CostArray->Num());
		for (const TSharedPtr<FJsonValue>& CostValue : *CostArray)
		{
			if (!CostValue.IsValid() || CostValue->Type != EJson::Object)
			{
				OutError = TEXT("upgrade.nextUpgradeCost contains a non-object value.");
				return false;
			}

			FFrontierOnlineInventoryCurrencyCost& ParsedCost = OutUpgrade.NextUpgradeCost.AddDefaulted_GetRef();
			if (!ParseRequiredString(CostValue->AsObject(), TEXT("currencyCode"), ParsedCost.CurrencyCode, OutError)
				|| !ParseRequiredInt64(CostValue->AsObject(), TEXT("amount"), ParsedCost.Amount, OutError))
			{
				return false;
			}
			if (ParsedCost.Amount < 0)
			{
				OutError = TEXT("upgrade.nextUpgradeCost.amount cannot be negative.");
				return false;
			}
		}
	}
	return true;
}

bool ParseStorageUpgrade(const TSharedPtr<FJsonObject>& Object, FFrontierOnlineStorageUpgrade& OutUpgrade, FString& OutError)
{
	if (!Object.IsValid())
	{
		return true;
	}

	if (!ParseRequiredInt32(Object, TEXT("capacityLevel"), OutUpgrade.CapacityLevel, OutError)
		|| !ParseRequiredInt32(Object, TEXT("maxCapacityLevel"), OutUpgrade.MaxCapacityLevel, OutError))
	{
		return false;
	}
	if (!ParseNullableInt32(Object, TEXT("nextCapacityLevel"), OutUpgrade.bHasNextCapacityLevel, OutUpgrade.NextCapacityLevel))
	{
		OutError = TEXT("upgrade.nextCapacityLevel must be an integer or null.");
		return false;
	}
	if (!ParseNullableInt32(Object, TEXT("nextSlotCapacity"), OutUpgrade.bHasNextSlotCapacity, OutUpgrade.NextSlotCapacity))
	{
		OutError = TEXT("upgrade.nextSlotCapacity must be an integer or null.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* CostArray = nullptr;
	if (Object->TryGetArrayField(TEXT("nextUpgradeCost"), CostArray) && CostArray)
	{
		OutUpgrade.NextUpgradeCost.Reset(CostArray->Num());
		for (const TSharedPtr<FJsonValue>& CostValue : *CostArray)
		{
			if (!CostValue.IsValid() || CostValue->Type != EJson::Object)
			{
				OutError = TEXT("upgrade.nextUpgradeCost contains a non-object value.");
				return false;
			}

			FFrontierOnlineStorageCurrencyCost& ParsedCost = OutUpgrade.NextUpgradeCost.AddDefaulted_GetRef();
			if (!ParseRequiredString(CostValue->AsObject(), TEXT("currencyCode"), ParsedCost.CurrencyCode, OutError)
				|| !ParseRequiredInt64(CostValue->AsObject(), TEXT("amount"), ParsedCost.Amount, OutError))
			{
				return false;
			}
			if (ParsedCost.Amount < 0)
			{
				OutError = TEXT("upgrade.nextUpgradeCost.amount cannot be negative.");
				return false;
			}
		}
	}

	return true;
}

bool ParseCurrencyDTO(const TSharedPtr<FJsonObject>& Object, FFrontierOnlineCurrencyDTO& OutCurrency, FString& OutError)
{
	if (!ParseRequiredString(Object, TEXT("currencyCode"), OutCurrency.CurrencyCode, OutError)
		|| !ParseRequiredInt64(Object, TEXT("balance"), OutCurrency.Balance, OutError))
	{
		return false;
	}
	if (OutCurrency.Balance < 0)
	{
		OutError = TEXT("currency.balance cannot be negative.");
		return false;
	}
	return true;
}

bool ParseRaidSessionDTO(const TSharedPtr<FJsonObject>& Object, FFrontierOnlineRaidSessionDTO& OutRaidSession, FString& OutError)
{
	if (!ParseRequiredString(Object, TEXT("raidSessionId"), OutRaidSession.RaidSessionId, OutError)
		|| !ParseRequiredInt64(Object, TEXT("playerId"), OutRaidSession.PlayerId, OutError)
		|| !ParseRequiredString(Object, TEXT("state"), OutRaidSession.State, OutError)
		|| !ParseRequiredString(Object, TEXT("createdAt"), OutRaidSession.CreatedAt, OutError))
	{
		return false;
	}
	if (!ParseNullableString(Object, TEXT("serverId"), OutRaidSession.bHasServerId, OutRaidSession.ServerId))
	{
		OutError = TEXT("activeRaid.serverId must be a string or null.");
		return false;
	}
	if (!ParseNullableString(Object, TEXT("startedAt"), OutRaidSession.bHasStartedAt, OutRaidSession.StartedAt))
	{
		OutError = TEXT("activeRaid.startedAt must be a string or null.");
		return false;
	}
	if (!ParseNullableString(Object, TEXT("completedAt"), OutRaidSession.bHasCompletedAt, OutRaidSession.CompletedAt))
	{
		OutError = TEXT("activeRaid.completedAt must be a string or null.");
		return false;
	}
	return true;
}

bool ParseInventoryDataObject(const TSharedPtr<FJsonObject>& DataObject, FFrontierOnlineInventoryData& OutData, FString& OutError)
{
	if (!DataObject.IsValid())
	{
		OutError = TEXT("Inventory response has no data object.");
		return false;
	}

	const TSharedPtr<FJsonObject>* ContainerObject = nullptr;
	if (!DataObject->TryGetObjectField(TEXT("container"), ContainerObject) || !ContainerObject || !ContainerObject->IsValid())
	{
		OutError = TEXT("Inventory response has no data.container object.");
		return false;
	}
	if (!ParseInventoryHeader(*ContainerObject, OutData.Container, OutError))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* SlotsArray = nullptr;
	if (!DataObject->TryGetArrayField(TEXT("slots"), SlotsArray) || !SlotsArray)
	{
		OutError = TEXT("Inventory response has no data.slots array.");
		return false;
	}

	TSet<int32> SeenSlotIndices;
	for (const TSharedPtr<FJsonValue>& SlotValue : *SlotsArray)
	{
		if (!SlotValue.IsValid() || SlotValue->Type != EJson::Object)
		{
			OutError = TEXT("Inventory data.slots contains a non-object value.");
			return false;
		}

		FFrontierOnlineItemDTO& ParsedItem = OutData.Slots.AddDefaulted_GetRef();
		if (!ParseInventoryItem(SlotValue->AsObject(), ParsedItem, OutError))
		{
			return false;
		}
		if (ParsedItem.SlotIndex < 0 || ParsedItem.SlotIndex >= OutData.Container.SlotCapacity)
		{
			OutError = FString::Printf(
				TEXT("Inventory slotIndex is out of range. SlotIndex=%d SlotCapacity=%d"),
				ParsedItem.SlotIndex,
				OutData.Container.SlotCapacity);
			return false;
		}
		if (SeenSlotIndices.Contains(ParsedItem.SlotIndex))
		{
			OutError = FString::Printf(TEXT("Inventory response contains duplicate slotIndex: %d"), ParsedItem.SlotIndex);
			return false;
		}
		SeenSlotIndices.Add(ParsedItem.SlotIndex);
	}

	const TSharedPtr<FJsonObject>* UpgradeObject = nullptr;
	if (DataObject->TryGetObjectField(TEXT("upgrade"), UpgradeObject) && UpgradeObject && UpgradeObject->IsValid())
	{
		if (!ParseInventoryUpgrade(*UpgradeObject, OutData.Upgrade, OutError))
		{
			return false;
		}
	}

	return true;
}

bool ParseStorageDataObject(const TSharedPtr<FJsonObject>& DataObject, FFrontierOnlineStorageData& OutData, FString& OutError)
{
	if (!DataObject.IsValid())
	{
		OutError = TEXT("Storage response has no data object.");
		return false;
	}

	const TSharedPtr<FJsonObject>* ContainerObject = nullptr;
	if (!DataObject->TryGetObjectField(TEXT("container"), ContainerObject) || !ContainerObject || !ContainerObject->IsValid())
	{
		OutError = TEXT("Storage response has no data.container object.");
		return false;
	}
	if (!ParseStorageHeader(*ContainerObject, OutData.Container, OutError))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* SlotsArray = nullptr;
	if (!DataObject->TryGetArrayField(TEXT("slots"), SlotsArray) || !SlotsArray)
	{
		OutError = TEXT("Storage response has no data.slots array.");
		return false;
	}

	TSet<int32> SeenSlotIndices;
	for (const TSharedPtr<FJsonValue>& SlotValue : *SlotsArray)
	{
		if (!SlotValue.IsValid() || SlotValue->Type != EJson::Object)
		{
			OutError = TEXT("Storage data.slots contains a non-object value.");
			return false;
		}

		FFrontierOnlineItemDTO& ParsedItem = OutData.Slots.AddDefaulted_GetRef();
		if (!ParseInventoryItem(SlotValue->AsObject(), ParsedItem, OutError))
		{
			return false;
		}
		if (ParsedItem.SlotIndex < 0 || ParsedItem.SlotIndex >= OutData.Container.SlotCapacity)
		{
			OutError = FString::Printf(
				TEXT("Storage slotIndex is out of range. SlotIndex=%d SlotCapacity=%d"),
				ParsedItem.SlotIndex,
				OutData.Container.SlotCapacity);
			return false;
		}
		if (SeenSlotIndices.Contains(ParsedItem.SlotIndex))
		{
			OutError = FString::Printf(TEXT("Storage response contains duplicate slotIndex: %d"), ParsedItem.SlotIndex);
			return false;
		}
		SeenSlotIndices.Add(ParsedItem.SlotIndex);
	}

	const TSharedPtr<FJsonObject>* UpgradeObject = nullptr;
	if (DataObject->TryGetObjectField(TEXT("upgrade"), UpgradeObject) && UpgradeObject && UpgradeObject->IsValid())
	{
		if (!ParseStorageUpgrade(*UpgradeObject, OutData.Upgrade, OutError))
		{
			return false;
		}
	}

	return true;
}

bool ParseEquipmentDataObject(const TSharedPtr<FJsonObject>& DataObject, FFrontierOnlineEquipmentData& OutData, FString& OutError)
{
	if (!DataObject.IsValid())
	{
		OutError = TEXT("Equipment response has no data object.");
		return false;
	}

	const TSharedPtr<FJsonObject>* ContainerObject = nullptr;
	if (!DataObject->TryGetObjectField(TEXT("container"), ContainerObject) || !ContainerObject || !ContainerObject->IsValid())
	{
		OutError = TEXT("Equipment response has no data.container object.");
		return false;
	}
	if (!ParseEquipmentHeader(*ContainerObject, OutData.Container, OutError))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* SlotsArray = nullptr;
	if (!DataObject->TryGetArrayField(TEXT("slots"), SlotsArray) || !SlotsArray)
	{
		OutError = TEXT("Equipment response has no data.slots array.");
		return false;
	}

	TSet<FString> SeenSlots;
	for (const TSharedPtr<FJsonValue>& SlotValue : *SlotsArray)
	{
		if (!SlotValue.IsValid() || SlotValue->Type != EJson::Object)
		{
			OutError = TEXT("Equipment data.slots contains a non-object value.");
			return false;
		}

		FFrontierOnlineEquipmentSlot& ParsedSlot = OutData.Slots.AddDefaulted_GetRef();
		if (!ParseEquipmentSlot(SlotValue->AsObject(), ParsedSlot, OutError))
		{
			return false;
		}
		if (ParsedSlot.SlotType.IsEmpty())
		{
			OutError = TEXT("Equipment slotType is empty.");
			return false;
		}
		if (SeenSlots.Contains(ParsedSlot.SlotType))
		{
			OutError = FString::Printf(TEXT("Equipment response contains duplicate slotType: %s"), *ParsedSlot.SlotType);
			return false;
		}
		SeenSlots.Add(ParsedSlot.SlotType);
	}

	return true;
}

bool ParseLobbyBootstrapDataObject(const TSharedPtr<FJsonObject>& DataObject, FFrontierOnlineLobbyBootstrapData& OutData, FString& OutError)
{
	if (!DataObject.IsValid())
	{
		OutError = TEXT("Lobby bootstrap response has no data object.");
		return false;
	}

	const TSharedPtr<FJsonObject>* PlayerObject = nullptr;
	if (!DataObject->TryGetObjectField(TEXT("player"), PlayerObject) || !PlayerObject || !PlayerObject->IsValid())
	{
		OutError = TEXT("Lobby bootstrap response has no data.player object.");
		return false;
	}
	if (!ParseRequiredIdentifier(*PlayerObject, TEXT("playerId"), OutData.Player.PlayerIdString, OutData.Player.PlayerId, OutError))
	{
		return false;
	}
	(*PlayerObject)->TryGetStringField(TEXT("steamId"), OutData.Player.SteamId);
	(*PlayerObject)->TryGetStringField(TEXT("nickname"), OutData.Player.Nickname);

	const TArray<TSharedPtr<FJsonValue>>* CurrenciesArray = nullptr;
	if (!DataObject->TryGetArrayField(TEXT("currencies"), CurrenciesArray) || !CurrenciesArray)
	{
		OutError = TEXT("Lobby bootstrap response has no data.currencies array.");
		return false;
	}
	OutData.Currencies.Reset(CurrenciesArray->Num());
	for (const TSharedPtr<FJsonValue>& CurrencyValue : *CurrenciesArray)
	{
		if (!CurrencyValue.IsValid() || CurrencyValue->Type != EJson::Object)
		{
			OutError = TEXT("Lobby bootstrap data.currencies contains a non-object value.");
			return false;
		}

		FFrontierOnlineCurrencyDTO& ParsedCurrency = OutData.Currencies.AddDefaulted_GetRef();
		if (!ParseCurrencyDTO(CurrencyValue->AsObject(), ParsedCurrency, OutError))
		{
			return false;
		}
	}

	const TSharedPtr<FJsonObject>* InventoryObject = nullptr;
	if (!DataObject->TryGetObjectField(TEXT("inventory"), InventoryObject) || !InventoryObject || !InventoryObject->IsValid())
	{
		OutError = TEXT("Lobby bootstrap response has no data.inventory object.");
		return false;
	}
	if (!ParseInventoryDataObject(*InventoryObject, OutData.Inventory, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* StorageObject = nullptr;
	if (!DataObject->TryGetObjectField(TEXT("storage"), StorageObject) || !StorageObject || !StorageObject->IsValid())
	{
		OutError = TEXT("Lobby bootstrap response has no data.storage object.");
		return false;
	}
	if (!ParseStorageDataObject(*StorageObject, OutData.Storage, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* EquipmentObject = nullptr;
	if (!DataObject->TryGetObjectField(TEXT("equipment"), EquipmentObject) || !EquipmentObject || !EquipmentObject->IsValid())
	{
		OutError = TEXT("Lobby bootstrap response has no data.equipment object.");
		return false;
	}
	if (!ParseEquipmentDataObject(*EquipmentObject, OutData.Equipment, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonValue> ActiveRaidValue = DataObject->TryGetField(TEXT("activeRaid"));
	OutData.bHasActiveRaid = false;
	OutData.ActiveRaid = FFrontierOnlineRaidSessionDTO();
	if (ActiveRaidValue.IsValid() && ActiveRaidValue->Type != EJson::Null)
	{
		if (ActiveRaidValue->Type != EJson::Object)
		{
			OutError = TEXT("Lobby bootstrap data.activeRaid must be an object or null.");
			return false;
		}

		OutData.bHasActiveRaid = true;
		if (!ParseRaidSessionDTO(ActiveRaidValue->AsObject(), OutData.ActiveRaid, OutError))
		{
			return false;
		}
	}

	return true;
}
}

FFrontierOnlineHttpClient::FFrontierOnlineHttpClient(FFrontierOnlineConfig InConfig)
	: Config(MoveTemp(InConfig))
{
}

bool FFrontierOnlineHttpClient::LoginWithSteamTicket(
	const FString& SteamTicket,
	FFrontierOnlineSteamLoginCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion)
	{
		OutError = TEXT("Steam login completion callback is not bound.");
		return false;
	}
	if (SteamTicket.IsEmpty())
	{
		OutError = TEXT("Steam 인증 티켓이 비어 있습니다.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("steamTicket"), SteamTicket);
	RequestBody->SetStringField(TEXT("steamIdentity"), Config.SteamIdentity);
	RequestBody->SetStringField(TEXT("clientVersion"), Config.ClientVersion);
	RequestBody->SetStringField(TEXT("platform"), Config.Platform);

	FString RequestBodyString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	if (!FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer))
	{
		OutError = TEXT("Steam 인증 요청 데이터를 생성하지 못했습니다.");
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(Config.SteamAuthEndpoint));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetContentAsString(RequestBodyString);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleSteamLoginResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Steam 로그인 요청을 서버로 보내지 못했습니다.");
		return false;
	}

	return true;
}

bool ParseRaidRuntimeItemDTO(
	const TSharedPtr<FJsonObject>& Object,
	FFrontierOnlineRaidRuntimeItemDTO& OutItem,
	FString& OutError)
{
	if (!ParseRequiredString(Object, TEXT("raidItemId"), OutItem.RaidItemId, OutError)
		|| !ParseRequiredString(Object, TEXT("itemTemplateId"), OutItem.ItemTemplateId, OutError)
		|| !ParseRequiredInt32(Object, TEXT("quantity"), OutItem.Quantity, OutError)
		|| !ParseRequiredInt32(Object, TEXT("enhancementLevel"), OutItem.EnhancementLevel, OutError)
		|| !ParseRequiredString(Object, TEXT("finalRarityTag"), OutItem.FinalRarityTag, OutError))
	{
		return false;
	}
	if (!ParseNullableString(
		Object,
		TEXT("originItemInstanceId"),
		OutItem.bHasOriginItemInstanceId,
		OutItem.OriginItemInstanceId)
		|| !ParseNullableDouble(Object, TEXT("durability"), OutItem.bHasDurability, OutItem.Durability)
		|| !ParseNullableString(Object, TEXT("lootSourceId"), OutItem.bHasLootSourceId, OutItem.LootSourceId))
	{
		OutError = TEXT("Raid runtime item contains an invalid nullable field.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* RandomOptions = nullptr;
	if (Object->TryGetArrayField(TEXT("randomOptions"), RandomOptions) && RandomOptions)
	{
		if (!SerializeJsonArray(*RandomOptions, OutItem.RandomOptionsJson))
		{
			OutError = TEXT("Raid runtime item randomOptions could not be preserved.");
			return false;
		}
	}
	else
	{
		OutItem.RandomOptionsJson = TEXT("[]");
	}

	const TArray<TSharedPtr<FJsonValue>>* GeneratedSkills = nullptr;
	if (Object->TryGetArrayField(TEXT("generatedSkills"), GeneratedSkills) && GeneratedSkills)
	{
		if (!SerializeJsonArray(*GeneratedSkills, OutItem.GeneratedSkillsJson))
		{
			OutError = TEXT("Raid runtime item generatedSkills could not be preserved.");
			return false;
		}
	}
	else
	{
		OutItem.GeneratedSkillsJson = TEXT("[]");
	}

	if (OutItem.RaidItemId.IsEmpty() || OutItem.ItemTemplateId.IsEmpty() || OutItem.Quantity <= 0
		|| OutItem.EnhancementLevel < 0)
	{
		OutError = TEXT("Raid runtime item contains invalid identity, quantity, or enhancement values.");
		return false;
	}
	return true;
}

bool ParseRaidLoadoutManifestDTO(
	const TSharedPtr<FJsonObject>& Object,
	FFrontierOnlineRaidLoadoutManifestDTO& OutManifest,
	FString& OutError)
{
	if (!Object.IsValid())
	{
		OutError = TEXT("Join authorization has no loadoutManifest object.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* InventorySlots = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* EquipmentSlots = nullptr;
	if (!Object->TryGetArrayField(TEXT("inventorySlots"), InventorySlots) || !InventorySlots
		|| !Object->TryGetArrayField(TEXT("equipmentSlots"), EquipmentSlots) || !EquipmentSlots)
	{
		OutError = TEXT("Join authorization loadoutManifest is missing slot arrays.");
		return false;
	}

	OutManifest.InventorySlots.Reset(InventorySlots->Num());
	for (const TSharedPtr<FJsonValue>& Value : *InventorySlots)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			OutError = TEXT("loadoutManifest.inventorySlots contains a non-object value.");
			return false;
		}
		const TSharedPtr<FJsonObject> SlotObject = Value->AsObject();
		int32 SlotIndex = INDEX_NONE;
		if (!ParseRequiredInt32(SlotObject, TEXT("slotIndex"), SlotIndex, OutError))
		{
			return false;
		}
		const TSharedPtr<FJsonValue>* ItemValue = SlotObject->Values.Find(TEXT("item"));
		if (!ItemValue || !ItemValue->IsValid())
		{
			OutError = FString::Printf(
				TEXT("loadoutManifest.inventorySlots[%d] is missing the item field."),
				SlotIndex);
			return false;
		}
		if ((*ItemValue)->Type == EJson::Null)
		{
			continue;
		}
		if ((*ItemValue)->Type != EJson::Object)
		{
			OutError = FString::Printf(
				TEXT("loadoutManifest.inventorySlots[%d].item must be an object or null."),
				SlotIndex);
			return false;
		}

		FFrontierOnlineRaidInventorySlotDTO& Slot = OutManifest.InventorySlots.AddDefaulted_GetRef();
		Slot.SlotIndex = SlotIndex;
		if (!ParseRaidRuntimeItemDTO((*ItemValue)->AsObject(), Slot.Item, OutError))
		{
			return false;
		}
	}

	OutManifest.EquipmentSlots.Reset(EquipmentSlots->Num());
	for (const TSharedPtr<FJsonValue>& Value : *EquipmentSlots)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			OutError = TEXT("loadoutManifest.equipmentSlots contains a non-object value.");
			return false;
		}
		const TSharedPtr<FJsonObject> SlotObject = Value->AsObject();
		FString SlotType;
		if (!ParseRequiredString(SlotObject, TEXT("slotType"), SlotType, OutError))
		{
			return false;
		}
		const TSharedPtr<FJsonValue>* ItemValue = SlotObject->Values.Find(TEXT("item"));
		if (!ItemValue || !ItemValue->IsValid())
		{
			OutError = FString::Printf(
				TEXT("loadoutManifest.equipmentSlots[%s] is missing the item field."),
				*SlotType);
			return false;
		}
		if ((*ItemValue)->Type == EJson::Null)
		{
			continue;
		}
		if ((*ItemValue)->Type != EJson::Object)
		{
			OutError = FString::Printf(
				TEXT("loadoutManifest.equipmentSlots[%s].item must be an object or null."),
				*SlotType);
			return false;
		}

		FFrontierOnlineRaidEquipmentSlotDTO& Slot = OutManifest.EquipmentSlots.AddDefaulted_GetRef();
		Slot.SlotType = MoveTemp(SlotType);
		if (!ParseRaidRuntimeItemDTO((*ItemValue)->AsObject(), Slot.Item, OutError))
		{
			return false;
		}
	}
	return true;
}

bool ParseRaidResultDTO(
	const TSharedPtr<FJsonObject>& Object,
	FFrontierOnlineRaidResultDTO& OutResult,
	FString& OutError)
{
	if (!ParseRequiredString(Object, TEXT("raidSessionId"), OutResult.RaidSessionId, OutError)
		|| !ParseRequiredInt64(Object, TEXT("playerId"), OutResult.PlayerId, OutError)
		|| !ParseRequiredString(Object, TEXT("outcome"), OutResult.Outcome, OutError)
		|| !ParseNullableString(Object, TEXT("reasonCode"), OutResult.bHasReasonCode, OutResult.ReasonCode)
		|| !ParseNullableString(Object, TEXT("committedAt"), OutResult.bHasCommittedAt, OutResult.CommittedAt))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Raid result contains an invalid nullable field.");
		}
		return false;
	}
	return OutResult.PlayerId > 0;
}

bool FFrontierOnlineHttpClient::RefreshSession(
	const FString& RefreshToken,
	const FString& SessionId,
	FFrontierOnlineRefreshCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion || RefreshToken.IsEmpty() || SessionId.IsEmpty())
	{
		OutError = TEXT("Refresh request requires a callback, refresh token, and session ID.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("refreshToken"), RefreshToken);
	Body->SetStringField(TEXT("sessionId"), SessionId);
	FString BodyString;
	if (!SerializeJsonObject(Body, BodyString))
	{
		OutError = TEXT("Refresh request body could not be serialized.");
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(TEXT("/v1/auth/refresh")));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetHeader(TEXT("X-Request-ID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->SetContentAsString(BodyString);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleRefreshResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Refresh request could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::LogoutSession(
	const FString& AccessToken,
	const FString& RefreshToken,
	const bool bAllSessions,
	FFrontierOnlineLogoutCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion || AccessToken.IsEmpty() || RefreshToken.IsEmpty())
	{
		OutError = TEXT("Logout requires a callback, access token, and refresh token.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("refreshToken"), RefreshToken);
	Body->SetBoolField(TEXT("allSessions"), bAllSessions);
	FString BodyString;
	if (!SerializeJsonObject(Body, BodyString))
	{
		OutError = TEXT("Logout request body could not be serialized.");
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(TEXT("/v1/auth/logout")));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetHeader(TEXT("X-Request-ID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->SetHeader(TEXT("Idempotency-Key"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->SetContentAsString(BodyString);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleLogoutResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Logout request could not be sent to the backend.");
		return false;
	}
	return true;
}

bool ParseUpgradeItem(const TSharedPtr<FJsonObject>& Object, FFrontierOnlineItemDTO& OutItem, FString& OutError)
{
	OutItem = FFrontierOnlineItemDTO();
	if (!Object.IsValid()
		|| !ParseRequiredString(Object, TEXT("itemInstanceId"), OutItem.ItemInstanceId, OutError)
		|| !ParseRequiredString(Object, TEXT("itemTemplateId"), OutItem.ItemTemplateId, OutError)
		|| !ParseRequiredInt32(Object, TEXT("enhancementLevel"), OutItem.EnhancementLevel, OutError))
	{
		return false;
	}

	double Quantity = 1.0;
	if (TryGetNumberAlias(Object, { TEXT("quantity") }, Quantity))
	{
		const int64 RoundedQuantity = FMath::RoundToInt64(Quantity);
		if (!FMath::IsNearlyEqual(Quantity, static_cast<double>(RoundedQuantity))
			|| RoundedQuantity <= 0
			|| RoundedQuantity > TNumericLimits<int32>::Max())
		{
			OutError = TEXT("Upgrade response item.quantity must be a positive integer.");
			return false;
		}
		OutItem.Quantity = static_cast<int32>(RoundedQuantity);
	}

	if (!ParseNullableDouble(Object, TEXT("durability"), OutItem.bHasDurability, OutItem.Durability))
	{
		OutError = TEXT("Upgrade response item.durability must be a number or null.");
		return false;
	}
	(void)TryGetStringAlias(Object, { TEXT("finalRarityTag"), TEXT("rarity"), TEXT("finalRarity") }, OutItem.FinalRarityTag);
	Object->TryGetStringField(TEXT("bindState"), OutItem.BindState);

	const TArray<TSharedPtr<FJsonValue>>* TagsArray = nullptr;
	if (Object->TryGetArrayField(TEXT("instanceTags"), TagsArray) && TagsArray)
	{
		for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
		{
			if (!TagValue.IsValid() || TagValue->Type != EJson::String)
			{
				OutError = TEXT("Upgrade response item.instanceTags must contain only strings.");
				return false;
			}
			OutItem.InstanceTags.Add(TagValue->AsString());
		}
	}

	const TCHAR* StatsFieldName = Object->HasField(TEXT("generatedStats")) ? TEXT("generatedStats") : TEXT("randomOptions");
	if (!ParseItemOptions(Object, StatsFieldName, OutItem.RandomOptions, OutError)
		|| !ParseGeneratedItemSkills(Object, TEXT("generatedSkills"), OutItem.GeneratedSkills, OutError))
	{
		return false;
	}

	Object->TryGetStringField(TEXT("createdAt"), OutItem.CreatedAt);
	Object->TryGetStringField(TEXT("acquiredAt"), OutItem.AcquiredAt);
	Object->TryGetStringField(TEXT("updatedAt"), OutItem.UpdatedAt);
	return true;
}

bool FFrontierOnlineHttpClient::UpdateMyProfile(
	const FString& AccessToken,
	const FString& DisplayName,
	const FString& Locale,
	const FString& IdempotencyKey,
	FFrontierOnlineProfileCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion || AccessToken.IsEmpty() || DisplayName.IsEmpty() || Locale.IsEmpty() || IdempotencyKey.IsEmpty())
	{
		OutError = TEXT("Profile update requires a callback, access token, display name, locale, and idempotency key.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("displayName"), DisplayName);
	Body->SetStringField(TEXT("locale"), Locale);
	FString BodyString;
	if (!SerializeJsonObject(Body, BodyString))
	{
		OutError = TEXT("Profile update request body could not be serialized.");
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(TEXT("/v1/me/profile")));
	Request->SetVerb(TEXT("PATCH"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetHeader(TEXT("X-Request-ID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->SetHeader(TEXT("Idempotency-Key"), IdempotencyKey);
	Request->SetContentAsString(BodyString);
	UE_LOG(LogFrontierOnline, Log, TEXT("[Nickname] Sending profile update. Method=PATCH Endpoint=/v1/me/profile Body=%s"), *BodyString);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleProfileResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Profile update request could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::GetMyProfile(
	const FString& AccessToken,
	FFrontierOnlineProfileCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion || AccessToken.IsEmpty())
	{
		OutError = TEXT("Profile query requires a callback and access token.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(TEXT("/v1/me/profile")));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetHeader(TEXT("X-Request-ID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleProfileResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Profile query request could not be sent to the backend.");
		return false;
	}

	return true;
}

bool FFrontierOnlineHttpClient::CreateParty(
	const FString& AccessToken,
	const FString& SteamLobbyId,
	const FString& IdempotencyKey,
	FFrontierOnlinePartyCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion || AccessToken.IsEmpty() || SteamLobbyId.IsEmpty() || IdempotencyKey.IsEmpty())
	{
		OutError = TEXT("Party creation requires a callback, access token, Steam lobby ID, and idempotency key.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("steamLobbyId"), SteamLobbyId);
	FString BodyString;
	if (!SerializeJsonObject(Body, BodyString))
	{
		OutError = TEXT("Party creation request body could not be serialized.");
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(TEXT("/v1/parties")));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetHeader(TEXT("X-Request-ID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->SetHeader(TEXT("Idempotency-Key"), IdempotencyKey);
	Request->SetContentAsString(BodyString);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandlePartyResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Party creation request could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::JoinParty(
	const FString& AccessToken,
	const FString& PartyId,
	const FString& SteamLobbyId,
	const FString& IdempotencyKey,
	FFrontierOnlinePartyCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion || AccessToken.IsEmpty() || PartyId.IsEmpty()
		|| SteamLobbyId.IsEmpty() || IdempotencyKey.IsEmpty())
	{
		OutError = TEXT("Party join requires a callback, access token, party ID, Steam lobby ID, and idempotency key.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("steamLobbyId"), SteamLobbyId);
	FString BodyString;
	if (!SerializeJsonObject(Body, BodyString))
	{
		OutError = TEXT("Party join request body could not be serialized.");
		return false;
	}

	const FString Endpoint = FString::Printf(
		TEXT("/v1/parties/%s/members"),
		*FGenericPlatformHttp::UrlEncode(PartyId));
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(Endpoint));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetHeader(TEXT("X-Request-ID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->SetHeader(TEXT("Idempotency-Key"), IdempotencyKey);
	Request->SetContentAsString(BodyString);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandlePartyResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Party join request could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::GetParty(
	const FString& AccessToken,
	const FString& PartyId,
	FFrontierOnlinePartyCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress || !Completion || AccessToken.IsEmpty() || PartyId.IsEmpty())
	{
		OutError = bRequestInProgress
			? TEXT("A backend request is already in progress.")
			: TEXT("Party lookup requires a callback, access token, and party ID.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(FString::Printf(
		TEXT("/v1/parties/%s"), *FGenericPlatformHttp::UrlEncode(PartyId))));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetHeader(TEXT("X-Request-ID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->OnProcessRequestComplete().BindSP(
		AsShared(), &FFrontierOnlineHttpClient::HandlePartyResponse, MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Party lookup could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::GetMyParty(
	const FString& AccessToken,
	FFrontierOnlinePartyCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress || !Completion || AccessToken.IsEmpty())
	{
		OutError = bRequestInProgress
			? TEXT("A backend request is already in progress.")
			: TEXT("Current party lookup requires a callback and access token.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(TEXT("/v1/parties/me")));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetHeader(TEXT("X-Request-ID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->OnProcessRequestComplete().BindSP(
		AsShared(), &FFrontierOnlineHttpClient::HandlePartyResponse, MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Current party lookup could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::LeaveParty(
	const FString& AccessToken,
	const FString& PartyId,
	FFrontierOnlineLeavePartyCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress || !Completion || AccessToken.IsEmpty() || PartyId.IsEmpty())
	{
		OutError = bRequestInProgress
			? TEXT("A backend request is already in progress.")
			: TEXT("Party leave requires a callback, access token, and party ID.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(FString::Printf(
		TEXT("/v1/parties/%s/members/me"), *FGenericPlatformHttp::UrlEncode(PartyId))));
	Request->SetVerb(TEXT("DELETE"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetHeader(TEXT("X-Request-ID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->OnProcessRequestComplete().BindSP(
		AsShared(), &FFrontierOnlineHttpClient::HandleLeavePartyResponse, MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Party leave request could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::DisbandParty(
	const FString& AccessToken,
	const FString& PartyId,
	FFrontierOnlinePartyCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress || !Completion || AccessToken.IsEmpty() || PartyId.IsEmpty())
	{
		OutError = bRequestInProgress
			? TEXT("A backend request is already in progress.")
			: TEXT("Party disband requires a callback, access token, and party ID.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(FString::Printf(
		TEXT("/v1/parties/%s"), *FGenericPlatformHttp::UrlEncode(PartyId))));
	Request->SetVerb(TEXT("DELETE"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetHeader(TEXT("X-Request-ID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->OnProcessRequestComplete().BindSP(
		AsShared(), &FFrontierOnlineHttpClient::HandlePartyResponse, MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Party disband request could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::GetInventory(
	const FString& AccessToken,
	FFrontierOnlineInventoryCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion)
	{
		OutError = TEXT("Inventory completion callback is not bound.");
		return false;
	}
	if (AccessToken.IsEmpty())
	{
		OutError = TEXT("Access token is empty.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(TEXT("/v1/me/inventory")));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleInventoryResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Inventory request could not be sent to the backend.");
		return false;
	}

	return true;
}

bool FFrontierOnlineHttpClient::GetStorage(
	const FString& AccessToken,
	FFrontierOnlineStorageCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion)
	{
		OutError = TEXT("Storage completion callback is not bound.");
		return false;
	}
	if (AccessToken.IsEmpty())
	{
		OutError = TEXT("Access token is empty.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(TEXT("/v1/me/storage")));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleStorageResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Storage request could not be sent to the backend.");
		return false;
	}

	return true;
}

bool FFrontierOnlineHttpClient::DepositStorageItem(
	const FString& AccessToken,
	const FString& ItemInstanceId,
	const TOptional<int32>& TargetSlotIndex,
	const TOptional<int32>& Quantity,
	FFrontierOnlineStorageTransferCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion)
	{
		OutError = TEXT("Storage deposit completion callback is not bound.");
		return false;
	}
	if (AccessToken.IsEmpty() || ItemInstanceId.IsEmpty())
	{
		OutError = TEXT("Storage deposit request is missing access token or itemInstanceId.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("itemInstanceId"), ItemInstanceId);
	if (TargetSlotIndex.IsSet())
	{
		RequestBody->SetNumberField(TEXT("targetSlotIndex"), TargetSlotIndex.GetValue());
	}
	if (Quantity.IsSet())
	{
		RequestBody->SetNumberField(TEXT("quantity"), Quantity.GetValue());
	}

	FString RequestBodyString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	if (!FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer))
	{
		OutError = TEXT("Storage deposit request body could not be serialized.");
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(TEXT("/v1/me/storage/deposit")));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetContentAsString(RequestBodyString);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleStorageTransferResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Storage deposit request could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::WithdrawStorageItem(
	const FString& AccessToken,
	const FString& ItemInstanceId,
	const TOptional<int32>& TargetSlotIndex,
	const TOptional<int32>& Quantity,
	FFrontierOnlineStorageTransferCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion)
	{
		OutError = TEXT("Storage withdraw completion callback is not bound.");
		return false;
	}
	if (AccessToken.IsEmpty() || ItemInstanceId.IsEmpty())
	{
		OutError = TEXT("Storage withdraw request is missing access token or itemInstanceId.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("itemInstanceId"), ItemInstanceId);
	if (TargetSlotIndex.IsSet())
	{
		RequestBody->SetNumberField(TEXT("targetSlotIndex"), TargetSlotIndex.GetValue());
	}
	if (Quantity.IsSet())
	{
		RequestBody->SetNumberField(TEXT("quantity"), Quantity.GetValue());
	}

	FString RequestBodyString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	if (!FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer))
	{
		OutError = TEXT("Storage withdraw request body could not be serialized.");
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(TEXT("/v1/me/storage/withdraw")));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetContentAsString(RequestBodyString);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleStorageTransferResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Storage withdraw request could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::MoveStorageItem(
	const FString& AccessToken,
	const FString& ItemInstanceId,
	const int32 TargetSlotIndex,
	const TOptional<int32>& Quantity,
	FFrontierOnlineStorageCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion)
	{
		OutError = TEXT("Storage move completion callback is not bound.");
		return false;
	}
	if (AccessToken.IsEmpty() || ItemInstanceId.IsEmpty())
	{
		OutError = TEXT("Storage move request is missing access token or itemInstanceId.");
		return false;
	}
	if (TargetSlotIndex < 0)
	{
		OutError = TEXT("Storage move request contains an invalid target slot.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetNumberField(TEXT("targetSlotIndex"), TargetSlotIndex);
	if (Quantity.IsSet())
	{
		RequestBody->SetNumberField(TEXT("quantity"), Quantity.GetValue());
	}
	else
	{
		RequestBody->SetField(TEXT("quantity"), MakeShared<FJsonValueNull>());
	}

	FString RequestBodyString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	if (!FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer))
	{
		OutError = TEXT("Storage move request body could not be serialized.");
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(FString::Printf(TEXT("/v1/me/storage/items/%s/move"), *ItemInstanceId)));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetContentAsString(RequestBodyString);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleStorageResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Storage move request could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::GetLobbyBootstrap(
	const FString& AccessToken,
	FFrontierOnlineLobbyBootstrapCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion)
	{
		OutError = TEXT("Lobby bootstrap completion callback is not bound.");
		return false;
	}
	if (AccessToken.IsEmpty())
	{
		OutError = TEXT("Access token is empty.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(TEXT("/v1/me/bootstrap")));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleLobbyBootstrapResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Lobby bootstrap request could not be sent to the backend.");
		return false;
	}

	return true;
}

bool FFrontierOnlineHttpClient::GetPlayerLevel(
	const FString& AccessToken,
	FFrontierOnlinePlayerLevelCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion)
	{
		OutError = TEXT("Player level completion callback is not bound.");
		return false;
	}
	if (AccessToken.IsEmpty())
	{
		OutError = TEXT("Access token is empty.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(TEXT("/v1/me/level")));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetHeader(TEXT("X-Request-ID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandlePlayerLevelResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Player level request could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::CreateMatchmakingTicket(
	const FString& AccessToken,
	const FFrontierOnlineCreateMatchmakingTicketRequest& TicketRequest,
	FFrontierOnlineMatchmakingTicketCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	const bool bValidMode = TicketRequest.MatchMode == TEXT("SOLO")
		|| TicketRequest.MatchMode == TEXT("PARTY");
	if (bRequestInProgress || !Completion || AccessToken.IsEmpty() || !bValidMode
		|| TicketRequest.RaidDefinitionId.IsEmpty() || TicketRequest.MapId.IsEmpty()
		|| TicketRequest.Region.IsEmpty()
		|| (TicketRequest.MatchMode == TEXT("SOLO") && TicketRequest.bHasPartyId))
	{
		OutError = bRequestInProgress
			? TEXT("A backend request is already in progress.")
			: TEXT("Matchmaking ticket request contains invalid token, mode, party, raid, map, or region data.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("matchMode"), TicketRequest.MatchMode);
	Body->SetStringField(TEXT("raidDefinitionId"), TicketRequest.RaidDefinitionId);
	Body->SetStringField(TEXT("mapId"), TicketRequest.MapId);
	Body->SetStringField(TEXT("region"), TicketRequest.Region);
	if (TicketRequest.bHasPartyId)
	{
		Body->SetStringField(TEXT("partyId"), TicketRequest.PartyId);
	}
	else
	{
		Body->SetField(TEXT("partyId"), MakeShared<FJsonValueNull>());
	}
	FString BodyString;
	if (!SerializeJsonObject(Body, BodyString))
	{
		OutError = TEXT("Matchmaking ticket request body could not be serialized.");
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(TEXT("/v1/matchmaking/tickets")));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetHeader(TEXT("X-Request-ID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->SetContentAsString(BodyString);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleMatchmakingTicketResponse,
		MoveTemp(Completion));
	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Matchmaking ticket request could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::GetMatchmakingTicket(
	const FString& AccessToken,
	const FString& TicketId,
	FFrontierOnlineMatchmakingTicketCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress || !Completion || AccessToken.IsEmpty() || TicketId.IsEmpty())
	{
		OutError = bRequestInProgress
			? TEXT("A backend request is already in progress.")
			: TEXT("Matchmaking ticket lookup requires a token, ticketId, and callback.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(FString::Printf(
		TEXT("/v1/matchmaking/tickets/%s"),
		*FGenericPlatformHttp::UrlEncode(TicketId))));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetHeader(TEXT("X-Request-ID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleMatchmakingTicketResponse,
		MoveTemp(Completion));
	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Matchmaking ticket lookup could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::CancelMatchmakingTicket(
	const FString& AccessToken,
	const FString& TicketId,
	FFrontierOnlineMatchmakingTicketCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress || !Completion || AccessToken.IsEmpty() || TicketId.IsEmpty())
	{
		OutError = bRequestInProgress
			? TEXT("A backend request is already in progress.")
			: TEXT("Matchmaking cancellation requires a token, ticketId, and callback.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(FString::Printf(
		TEXT("/v1/matchmaking/tickets/%s"), *FGenericPlatformHttp::UrlEncode(TicketId))));
	Request->SetVerb(TEXT("DELETE"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetHeader(TEXT("X-Request-ID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->OnProcessRequestComplete().BindSP(
		AsShared(), &FFrontierOnlineHttpClient::HandleMatchmakingTicketResponse, MoveTemp(Completion));
	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Matchmaking cancellation could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::CreateRaidEntry(
	const FString& AccessToken,
	const FFrontierOnlineCreateRaidEntryRequest& EntryRequest,
	FFrontierOnlineRaidEntryCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion || AccessToken.IsEmpty() || EntryRequest.RaidDefinitionId.IsEmpty()
		|| EntryRequest.Region.IsEmpty() || EntryRequest.MapId.IsEmpty())
	{
		OutError = TEXT("Raid entry request contains an invalid callback, token, raid definition, region, or map ID.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("raidDefinitionId"), EntryRequest.RaidDefinitionId);
	Body->SetStringField(TEXT("region"), EntryRequest.Region);
	Body->SetStringField(TEXT("mapId"), EntryRequest.MapId);
	if (EntryRequest.bHasPartyId)
	{
		Body->SetStringField(TEXT("partyId"), EntryRequest.PartyId);
	}
	else
	{
		Body->SetField(TEXT("partyId"), MakeShared<FJsonValueNull>());
	}
	if (EntryRequest.bHasTicketId)
	{
		Body->SetStringField(TEXT("ticketId"), EntryRequest.TicketId);
	}
	else
	{
		Body->SetField(TEXT("ticketId"), MakeShared<FJsonValueNull>());
	}
	if (EntryRequest.bHasMatchId)
	{
		Body->SetStringField(TEXT("matchId"), EntryRequest.MatchId);
	}
	else
	{
		Body->SetField(TEXT("matchId"), MakeShared<FJsonValueNull>());
	}
	FString BodyString;
	if (!SerializeJsonObject(Body, BodyString))
	{
		OutError = TEXT("Raid entry request body could not be serialized.");
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(TEXT("/v1/raids/entries")));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetHeader(TEXT("X-Request-ID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->SetContentAsString(BodyString);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleRaidEntryResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Raid entry request could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::GetRaidResult(
	const FString& AccessToken,
	const FString& RaidSessionId,
	const bool bIncludeSnapshot,
	FFrontierOnlineRaidResultCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion || AccessToken.IsEmpty() || RaidSessionId.IsEmpty())
	{
		OutError = TEXT("Raid result request requires a callback, access token, and raid session ID.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	const FString Path = FString::Printf(
		TEXT("/v1/me/raids/%s/result?includeSnapshot=%s"),
		*FGenericPlatformHttp::UrlEncode(RaidSessionId),
		bIncludeSnapshot ? TEXT("true") : TEXT("false"));
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(Path));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetHeader(TEXT("X-Request-ID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleRaidResultResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Raid result request could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::MoveInventoryItem(
	const FString& AccessToken,
	const FString& ItemInstanceId,
	const int32 TargetSlotIndex,
	const TOptional<int32>& Quantity,
	FFrontierOnlineInventoryCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion)
	{
		OutError = TEXT("Inventory move completion callback is not bound.");
		return false;
	}
	if (AccessToken.IsEmpty() || ItemInstanceId.IsEmpty())
	{
		OutError = TEXT("Inventory move request is missing access token or itemInstanceId.");
		return false;
	}
	if (TargetSlotIndex < 0)
	{
		OutError = TEXT("Inventory move request contains an invalid target slot.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetNumberField(TEXT("targetSlotIndex"), TargetSlotIndex);
	if (Quantity.IsSet())
	{
		RequestBody->SetNumberField(TEXT("quantity"), Quantity.GetValue());
	}
	else
	{
		RequestBody->SetField(TEXT("quantity"), MakeShared<FJsonValueNull>());
	}

	FString RequestBodyString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	if (!FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer))
	{
		OutError = TEXT("Inventory move request body could not be serialized.");
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(FString::Printf(TEXT("/v1/me/inventory/items/%s/move"), *ItemInstanceId)));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetContentAsString(RequestBodyString);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleInventoryResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Inventory move request could not be sent to the backend.");
		return false;
	}

	return true;
}

bool FFrontierOnlineHttpClient::UpgradeInventoryItem(
	const FString& AccessToken,
	const FString& ItemInstanceId,
	const int32 ExpectedEnhancementLevel,
	const FString& IdempotencyKey,
	FFrontierOnlineItemUpgradeCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion)
	{
		OutError = TEXT("Item upgrade completion callback is not bound.");
		return false;
	}
	if (AccessToken.IsEmpty() || ItemInstanceId.IsEmpty() || IdempotencyKey.IsEmpty())
	{
		OutError = TEXT("Item upgrade request requires an access token, itemInstanceId, and idempotency key.");
		return false;
	}
	if (ExpectedEnhancementLevel < 0)
	{
		OutError = TEXT("Item upgrade expectedEnhancementLevel cannot be negative.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetNumberField(TEXT("expectedEnhancementLevel"), ExpectedEnhancementLevel);
	FString RequestBodyString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	if (!FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer))
	{
		OutError = TEXT("Item upgrade request body could not be serialized.");
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(FString::Printf(TEXT("/v1/me/inventory/items/%s/upgrade"), *ItemInstanceId)));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("Idempotency-Key"), IdempotencyKey);
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetHeader(TEXT("X-Request-ID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Request->SetContentAsString(RequestBodyString);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleItemUpgradeResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Item upgrade request could not be sent to the backend.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::GetEquipment(
	const FString& AccessToken,
	FFrontierOnlineEquipmentCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion)
	{
		OutError = TEXT("Equipment completion callback is not bound.");
		return false;
	}
	if (AccessToken.IsEmpty())
	{
		OutError = TEXT("Access token is empty.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(TEXT("/v1/me/equipment")));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleEquipmentResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Equipment request could not be sent to the backend.");
		return false;
	}

	return true;
}

bool FFrontierOnlineHttpClient::EquipItem(
	const FString& AccessToken,
	const FString& ItemInstanceId,
	const FString& SlotType,
	const bool bReplaceExisting,
	const TOptional<int32>& ReplacementTargetInventorySlot,
	FFrontierOnlineEquipmentChangeCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion)
	{
		OutError = TEXT("Equipment equip completion callback is not bound.");
		return false;
	}
	if (AccessToken.IsEmpty() || ItemInstanceId.IsEmpty() || SlotType.IsEmpty())
	{
		OutError = TEXT("Equipment equip request is missing access token, itemInstanceId, or slotType.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("itemInstanceId"), ItemInstanceId);
	RequestBody->SetStringField(TEXT("slotType"), SlotType);
	RequestBody->SetBoolField(TEXT("replaceExisting"), bReplaceExisting);
	if (ReplacementTargetInventorySlot.IsSet())
	{
		RequestBody->SetNumberField(TEXT("replacementTargetInventorySlot"), ReplacementTargetInventorySlot.GetValue());
	}
	else
	{
		RequestBody->SetField(TEXT("replacementTargetInventorySlot"), MakeShared<FJsonValueNull>());
	}

	FString RequestBodyString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	if (!FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer))
	{
		OutError = TEXT("Equipment equip request body could not be serialized.");
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(TEXT("/v1/me/equipment/equip")));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetContentAsString(RequestBodyString);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleEquipmentChangeResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Equipment equip request could not be sent to the backend.");
		return false;
	}

	return true;
}

bool FFrontierOnlineHttpClient::UnequipItem(
	const FString& AccessToken,
	const FString& SlotType,
	const TOptional<int32>& TargetInventorySlotIndex,
	FFrontierOnlineEquipmentChangeCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (bRequestInProgress)
	{
		OutError = TEXT("A backend request is already in progress.");
		return false;
	}
	if (!Completion)
	{
		OutError = TEXT("Equipment unequip completion callback is not bound.");
		return false;
	}
	if (AccessToken.IsEmpty() || SlotType.IsEmpty())
	{
		OutError = TEXT("Equipment unequip request is missing access token or slotType.");
		return false;
	}
	if (!Config.IsValid(OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("slotType"), SlotType);
	if (TargetInventorySlotIndex.IsSet())
	{
		RequestBody->SetNumberField(TEXT("targetInventorySlotIndex"), TargetInventorySlotIndex.GetValue());
	}
	else
	{
		RequestBody->SetField(TEXT("targetInventorySlotIndex"), MakeShared<FJsonValueNull>());
	}

	FString RequestBodyString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	if (!FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer))
	{
		OutError = TEXT("Equipment unequip request body could not be serialized.");
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.BuildUrl(TEXT("/v1/me/equipment/unequip")));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetHeader(TEXT("X-Client-Version"), Config.ClientVersion);
	Request->SetHeader(TEXT("X-Platform"), Config.Platform);
	Request->SetContentAsString(RequestBodyString);
	Request->OnProcessRequestComplete().BindSP(
		AsShared(),
		&FFrontierOnlineHttpClient::HandleEquipmentChangeResponse,
		MoveTemp(Completion));

	bRequestInProgress = true;
	if (!Request->ProcessRequest())
	{
		bRequestInProgress = false;
		Request->OnProcessRequestComplete().Unbind();
		OutError = TEXT("Equipment unequip request could not be sent to the backend.");
		return false;
	}

	return true;
}

void FFrontierOnlineHttpClient::HandleSteamLoginResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	FFrontierOnlineSteamLoginCompletion Completion)
{
	bRequestInProgress = false;

	FFrontierOnlineSteamLoginResponse ParsedResponse;
	ParsedResponse.bTransportSucceeded = bWasSuccessful && Response.IsValid();
	if (!ParsedResponse.bTransportSucceeded)
	{
		ParsedResponse.Message = TEXT("백엔드 서버에 연결할 수 없습니다.");
		Completion(ParsedResponse);
		return;
	}

	ParsedResponse.HttpStatus = Response->GetResponseCode();
	FString ParseError;
	if (!ParseSteamLoginResponse(ParsedResponse.HttpStatus, Response->GetContentAsString(), ParsedResponse, ParseError))
	{
		ParsedResponse.Message = MoveTemp(ParseError);
	}

	Completion(ParsedResponse);
}

void FFrontierOnlineHttpClient::HandleRefreshResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	FFrontierOnlineRefreshCompletion Completion)
{
	bRequestInProgress = false;
	FFrontierOnlineRefreshResponse ParsedResponse;
	ParsedResponse.bTransportSucceeded = bWasSuccessful && Response.IsValid();
	if (!ParsedResponse.bTransportSucceeded)
	{
		ParsedResponse.Message = TEXT("Session refresh failed before receiving a response.");
		Completion(ParsedResponse);
		return;
	}

	ParsedResponse.HttpStatus = Response->GetResponseCode();
	FString ParseError;
	if (!ParseRefreshResponse(ParsedResponse.HttpStatus, Response->GetContentAsString(), ParsedResponse, ParseError))
	{
		ParsedResponse.Message = MoveTemp(ParseError);
	}
	Completion(ParsedResponse);
}

void FFrontierOnlineHttpClient::HandleLogoutResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	FFrontierOnlineLogoutCompletion Completion)
{
	bRequestInProgress = false;
	FFrontierOnlineLogoutResponse ParsedResponse;
	ParsedResponse.bTransportSucceeded = bWasSuccessful && Response.IsValid();
	if (!ParsedResponse.bTransportSucceeded)
	{
		ParsedResponse.Message = TEXT("Logout failed before receiving a response.");
		Completion(ParsedResponse);
		return;
	}

	ParsedResponse.HttpStatus = Response->GetResponseCode();
	FString ParseError;
	if (!ParseLogoutResponse(
		ParsedResponse.HttpStatus,
		Response->GetContentAsString(),
		ParsedResponse,
		ParseError))
	{
		ParsedResponse.Message = MoveTemp(ParseError);
	}
	Completion(ParsedResponse);
}

void FFrontierOnlineHttpClient::HandleProfileResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	FFrontierOnlineProfileCompletion Completion)
{
	bRequestInProgress = false;

	FFrontierOnlineProfileResponse ParsedResponse;
	ParsedResponse.bTransportSucceeded = bWasSuccessful && Response.IsValid();
	UE_LOG(LogFrontierOnline, Log, TEXT("[Profile] HTTP callback. WasSuccessful=%d ResponseValid=%d"),
		bWasSuccessful ? 1 : 0,
		Response.IsValid() ? 1 : 0);
	if (!ParsedResponse.bTransportSucceeded)
	{
		ParsedResponse.Message = TEXT("Profile request failed before receiving a response.");
		Completion(ParsedResponse);
		return;
	}

	ParsedResponse.HttpStatus = Response->GetResponseCode();
	const FString ResponseBody = Response->GetContentAsString();
	UE_LOG(LogFrontierOnline, Log, TEXT("[Profile] HTTP response. Status=%d BodyLength=%d Body=%s"),
		ParsedResponse.HttpStatus,
		ResponseBody.Len(),
		*ResponseBody);
	FString ParseError;
	if (!ParseProfileResponse(
		ParsedResponse.HttpStatus,
		ResponseBody,
		ParsedResponse,
		ParseError))
	{
		ParsedResponse.Message = MoveTemp(ParseError);
		UE_LOG(LogFrontierOnline, Warning, TEXT("[Profile] Profile response parsing failed. Error=%s"), *ParsedResponse.Message);
	}
	else
	{
		UE_LOG(LogFrontierOnline, Log, TEXT("[Profile] Profile response parsed. Success=%d PlayerId=%lld DisplayNameLength=%d Locale=%s"),
			ParsedResponse.bSuccess ? 1 : 0,
			ParsedResponse.PlayerId,
			ParsedResponse.DisplayName.Len(),
			*ParsedResponse.Locale);
	}
	Completion(ParsedResponse);
}

void FFrontierOnlineHttpClient::HandlePartyResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	FFrontierOnlinePartyCompletion Completion)
{
	bRequestInProgress = false;

	FFrontierOnlinePartyResponse ParsedResponse;
	ParsedResponse.bTransportSucceeded = bWasSuccessful && Response.IsValid();
	if (!ParsedResponse.bTransportSucceeded)
	{
		ParsedResponse.Message = TEXT("Party backend request failed before receiving a response.");
		Completion(ParsedResponse);
		return;
	}

	ParsedResponse.HttpStatus = Response->GetResponseCode();
	FString ParseError;
	if (!ParsePartyResponse(
		ParsedResponse.HttpStatus,
		Response->GetContentAsString(),
		ParsedResponse,
		ParseError))
	{
		ParsedResponse.bSuccess = false;
		ParsedResponse.Message = MoveTemp(ParseError);
	}
	Completion(ParsedResponse);
}

void FFrontierOnlineHttpClient::HandleLeavePartyResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	FFrontierOnlineLeavePartyCompletion Completion)
{
	bRequestInProgress = false;

	FFrontierOnlineLeavePartyResponse ParsedResponse;
	ParsedResponse.bTransportSucceeded = bWasSuccessful && Response.IsValid();
	if (!ParsedResponse.bTransportSucceeded)
	{
		ParsedResponse.Message = TEXT("Party leave request failed before receiving a response.");
		Completion(ParsedResponse);
		return;
	}

	ParsedResponse.HttpStatus = Response->GetResponseCode();
	FString ParseError;
	if (!ParseLeavePartyResponse(
		ParsedResponse.HttpStatus,
		Response->GetContentAsString(),
		ParsedResponse,
		ParseError))
	{
		ParsedResponse.bSuccess = false;
		ParsedResponse.Message = MoveTemp(ParseError);
	}
	Completion(ParsedResponse);
}

void FFrontierOnlineHttpClient::HandleInventoryResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	FFrontierOnlineInventoryCompletion Completion)
{
	bRequestInProgress = false;

	FFrontierOnlineInventoryResponse ParsedResponse;
	ParsedResponse.bTransportSucceeded = bWasSuccessful && Response.IsValid();
	if (!ParsedResponse.bTransportSucceeded)
	{
		ParsedResponse.Message = TEXT("Inventory backend request failed before receiving a response.");
		Completion(ParsedResponse);
		return;
	}

	ParsedResponse.HttpStatus = Response->GetResponseCode();

	FString ParseError;
	if (!ParseInventoryResponse(
		ParsedResponse.HttpStatus,
		Response->GetContentAsString(),
		ParsedResponse,
		ParseError))
	{
		ParsedResponse.Message = MoveTemp(ParseError);
	}

	Completion(ParsedResponse);
}

void FFrontierOnlineHttpClient::HandleItemUpgradeResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	FFrontierOnlineItemUpgradeCompletion Completion)
{
	bRequestInProgress = false;

	FFrontierOnlineItemUpgradeResponse ParsedResponse;
	ParsedResponse.bTransportSucceeded = bWasSuccessful && Response.IsValid();
	if (!ParsedResponse.bTransportSucceeded)
	{
		ParsedResponse.bRetryable = true;
		ParsedResponse.Message = TEXT("Item upgrade request failed before receiving a response.");
		Completion(ParsedResponse);
		return;
	}

	ParsedResponse.HttpStatus = Response->GetResponseCode();
	FString ParseError;
	if (!ParseItemUpgradeResponse(
		ParsedResponse.HttpStatus,
		Response->GetContentAsString(),
		ParsedResponse,
		ParseError))
	{
		ParsedResponse.bSuccess = false;
		ParsedResponse.Message = MoveTemp(ParseError);
	}
	Completion(ParsedResponse);
}

void FFrontierOnlineHttpClient::HandleStorageResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	FFrontierOnlineStorageCompletion Completion)
{
	bRequestInProgress = false;

	FFrontierOnlineStorageResponse ParsedResponse;
	ParsedResponse.bTransportSucceeded = bWasSuccessful && Response.IsValid();
	if (!ParsedResponse.bTransportSucceeded)
	{
		ParsedResponse.Message = TEXT("Storage backend request failed before receiving a response.");
		Completion(ParsedResponse);
		return;
	}

	ParsedResponse.HttpStatus = Response->GetResponseCode();

	FString ParseError;
	if (!ParseStorageResponse(
		ParsedResponse.HttpStatus,
		Response->GetContentAsString(),
		ParsedResponse,
		ParseError))
	{
		ParsedResponse.Message = MoveTemp(ParseError);
	}

	Completion(ParsedResponse);
}

void FFrontierOnlineHttpClient::HandleStorageTransferResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	FFrontierOnlineStorageTransferCompletion Completion)
{
	bRequestInProgress = false;

	FFrontierOnlineStorageTransferResponse ParsedResponse;
	ParsedResponse.bTransportSucceeded = bWasSuccessful && Response.IsValid();
	if (!ParsedResponse.bTransportSucceeded)
	{
		ParsedResponse.Message = TEXT("Storage transfer backend request failed before receiving a response.");
		Completion(ParsedResponse);
		return;
	}

	ParsedResponse.HttpStatus = Response->GetResponseCode();
	FString ParseError;
	if (!ParseStorageTransferResponse(
		ParsedResponse.HttpStatus,
		Response->GetContentAsString(),
		ParsedResponse,
		ParseError))
	{
		ParsedResponse.Message = MoveTemp(ParseError);
	}

	Completion(ParsedResponse);
}

void FFrontierOnlineHttpClient::HandleLobbyBootstrapResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	FFrontierOnlineLobbyBootstrapCompletion Completion)
{
	bRequestInProgress = false;

	const bool bTransportSucceeded = bWasSuccessful && Response.IsValid();
	if (!bTransportSucceeded)
	{
		Completion(false, 0, FString());
		return;
	}

	Completion(true, Response->GetResponseCode(), Response->GetContentAsString());
}

void FFrontierOnlineHttpClient::HandlePlayerLevelResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	FFrontierOnlinePlayerLevelCompletion Completion)
{
	bRequestInProgress = false;

	FFrontierOnlinePlayerLevelResponse ParsedResponse;
	ParsedResponse.bTransportSucceeded = bWasSuccessful && Response.IsValid();
	if (!ParsedResponse.bTransportSucceeded)
	{
		ParsedResponse.Message = TEXT("Player level backend request failed before receiving a response.");
		Completion(ParsedResponse);
		return;
	}

	ParsedResponse.HttpStatus = Response->GetResponseCode();
	FString ParseError;
	if (!ParsePlayerLevelResponse(
		ParsedResponse.HttpStatus,
		Response->GetContentAsString(),
		ParsedResponse,
		ParseError))
	{
		ParsedResponse.Message = MoveTemp(ParseError);
	}
	Completion(ParsedResponse);
}

void FFrontierOnlineHttpClient::HandleMatchmakingTicketResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	FFrontierOnlineMatchmakingTicketCompletion Completion)
{
	bRequestInProgress = false;
	FFrontierOnlineMatchmakingTicketResponse ParsedResponse;
	ParsedResponse.bTransportSucceeded = bWasSuccessful && Response.IsValid();
	if (!ParsedResponse.bTransportSucceeded)
	{
		ParsedResponse.Message = TEXT("Matchmaking request failed before receiving a response.");
		Completion(ParsedResponse);
		return;
	}
	ParsedResponse.HttpStatus = Response->GetResponseCode();
	FString ParseError;
	if (!ParseMatchmakingTicketResponse(
		ParsedResponse.HttpStatus,
		Response->GetContentAsString(),
		ParsedResponse,
		ParseError))
	{
		ParsedResponse.Message = MoveTemp(ParseError);
	}
	Completion(ParsedResponse);
}

void FFrontierOnlineHttpClient::HandleRaidEntryResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	FFrontierOnlineRaidEntryCompletion Completion)
{
	bRequestInProgress = false;
	FFrontierOnlineRaidEntryResponse ParsedResponse;
	ParsedResponse.bTransportSucceeded = bWasSuccessful && Response.IsValid();
	if (!ParsedResponse.bTransportSucceeded)
	{
		ParsedResponse.Message = TEXT("Raid entry request failed before receiving a response.");
		Completion(ParsedResponse);
		return;
	}

	ParsedResponse.HttpStatus = Response->GetResponseCode();
	FString ParseError;
	if (!ParseRaidEntryResponse(ParsedResponse.HttpStatus, Response->GetContentAsString(), ParsedResponse, ParseError))
	{
		ParsedResponse.Message = MoveTemp(ParseError);
	}
	Completion(ParsedResponse);
}

void FFrontierOnlineHttpClient::HandleRaidResultResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	FFrontierOnlineRaidResultCompletion Completion)
{
	bRequestInProgress = false;
	FFrontierOnlineRaidResultResponse ParsedResponse;
	ParsedResponse.bTransportSucceeded = bWasSuccessful && Response.IsValid();
	if (!ParsedResponse.bTransportSucceeded)
	{
		ParsedResponse.Message = TEXT("Raid result request failed before receiving a response.");
		Completion(ParsedResponse);
		return;
	}

	ParsedResponse.HttpStatus = Response->GetResponseCode();
	FString ParseError;
	if (!ParseRaidResultResponse(ParsedResponse.HttpStatus, Response->GetContentAsString(), ParsedResponse, ParseError))
	{
		ParsedResponse.Message = MoveTemp(ParseError);
	}
	Completion(ParsedResponse);
}

void FFrontierOnlineHttpClient::HandleEquipmentResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	FFrontierOnlineEquipmentCompletion Completion)
{
	bRequestInProgress = false;

	FFrontierOnlineEquipmentResponse ParsedResponse;
	ParsedResponse.bTransportSucceeded = bWasSuccessful && Response.IsValid();
	if (!ParsedResponse.bTransportSucceeded)
	{
		ParsedResponse.Message = TEXT("Equipment backend request failed before receiving a response.");
		Completion(ParsedResponse);
		return;
	}

	ParsedResponse.HttpStatus = Response->GetResponseCode();
	FString ParseError;
	if (!ParseEquipmentResponse(
		ParsedResponse.HttpStatus,
		Response->GetContentAsString(),
		ParsedResponse,
		ParseError))
	{
		ParsedResponse.Message = MoveTemp(ParseError);
	}

	Completion(ParsedResponse);
}

void FFrontierOnlineHttpClient::HandleEquipmentChangeResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	const bool bWasSuccessful,
	FFrontierOnlineEquipmentChangeCompletion Completion)
{
	bRequestInProgress = false;

	FFrontierOnlineEquipmentChangeResponse ParsedResponse;
	ParsedResponse.bTransportSucceeded = bWasSuccessful && Response.IsValid();
	if (!ParsedResponse.bTransportSucceeded)
	{
		ParsedResponse.Message = TEXT("Equipment mutation backend request failed before receiving a response.");
		Completion(ParsedResponse);
		return;
	}

	ParsedResponse.HttpStatus = Response->GetResponseCode();
	FString ParseError;
	if (!ParseEquipmentChangeResponse(
		ParsedResponse.HttpStatus,
		Response->GetContentAsString(),
		ParsedResponse,
		ParseError))
	{
		ParsedResponse.Message = MoveTemp(ParseError);
	}

	Completion(ParsedResponse);
}

bool FFrontierOnlineHttpClient::ParseSteamLoginResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineSteamLoginResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineSteamLoginResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Steam 로그인 응답을 해석하지 못했습니다.");
		return false;
	}

	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		OutError = BuildHttpFailureMessage(HttpStatus, RootObject);
		return false;
	}

	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess))
	{
		OutError = TEXT("Steam 로그인 응답에 성공 여부가 없습니다.");
		return false;
	}
	RootObject->TryGetStringField(TEXT("message"), OutResponse.Message);
	if (!OutResponse.bSuccess)
	{
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Steam login was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* DataObject = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), DataObject) || !DataObject || !DataObject->IsValid())
	{
		OutError = TEXT("Steam 로그인 응답에 사용자 데이터가 없습니다.");
		return false;
	}

	const TSharedPtr<FJsonObject>* SessionObject = nullptr;
	if (!(*DataObject)->TryGetObjectField(TEXT("session"), SessionObject) || !SessionObject || !SessionObject->IsValid())
	{
		OutError = TEXT("Steam 로그인 응답에 세션 데이터가 없습니다.");
		return false;
	}

	(*SessionObject)->TryGetStringField(TEXT("sessionId"), OutResponse.Session.SessionId);
	if (!ParseRequiredIdentifier(
		*SessionObject,
		TEXT("playerId"),
		OutResponse.Session.PlayerIdString,
		OutResponse.Session.PlayerId,
		OutError))
	{
		return false;
	}
	(*SessionObject)->TryGetStringField(TEXT("platform"), OutResponse.Session.Platform);
	(*SessionObject)->TryGetStringField(TEXT("expiresAt"), OutResponse.Session.ExpiresAt);

	const TSharedPtr<FJsonObject>* TokensObject = nullptr;
	if (!(*DataObject)->TryGetObjectField(TEXT("tokens"), TokensObject) || !TokensObject || !TokensObject->IsValid())
	{
		OutError = TEXT("Steam 로그인 응답에 토큰 데이터가 없습니다.");
		return false;
	}

	(*TokensObject)->TryGetStringField(TEXT("tokenType"), OutResponse.Tokens.TokenType);
	(*TokensObject)->TryGetStringField(TEXT("accessToken"), OutResponse.Tokens.AccessToken);
	(*TokensObject)->TryGetStringField(TEXT("accessTokenExpiresAt"), OutResponse.Tokens.AccessTokenExpiresAt);
	(*TokensObject)->TryGetStringField(TEXT("refreshToken"), OutResponse.Tokens.RefreshToken);
	(*TokensObject)->TryGetStringField(TEXT("refreshTokenExpiresAt"), OutResponse.Tokens.RefreshTokenExpiresAt);

	const TSharedPtr<FJsonObject>* PlayerObject = nullptr;
	if (!(*DataObject)->TryGetObjectField(TEXT("player"), PlayerObject) || !PlayerObject || !PlayerObject->IsValid())
	{
		OutError = TEXT("Steam 로그인 응답에 플레이어 데이터가 없습니다.");
		return false;
	}

	if (!ParseRequiredIdentifier(
		*PlayerObject,
		TEXT("playerId"),
		OutResponse.Player.PlayerIdString,
		OutResponse.Player.PlayerId,
		OutError))
	{
		return false;
	}
	(*PlayerObject)->TryGetStringField(TEXT("steamId"), OutResponse.Player.SteamId);
	(*PlayerObject)->TryGetStringField(TEXT("nickname"), OutResponse.Player.Nickname);
	(*DataObject)->TryGetBoolField(TEXT("isNewPlayer"), OutResponse.bIsNewPlayer);

	const TSharedPtr<FJsonObject>* MetaObject = nullptr;
	if (RootObject->TryGetObjectField(TEXT("meta"), MetaObject) && MetaObject && MetaObject->IsValid())
	{
		(*MetaObject)->TryGetStringField(TEXT("requestId"), OutResponse.RequestId);
		(*MetaObject)->TryGetStringField(TEXT("serverTime"), OutResponse.ServerTime);
	}

	if (OutResponse.Session.SessionId.IsEmpty())
	{
		OutResponse.bSuccess = false;
		OutError = TEXT("Steam 로그인 세션 ID가 비어 있습니다.");
		return false;
	}
	if (OutResponse.Player.PlayerIdString.IsEmpty())
	{
		OutResponse.bSuccess = false;
		OutError = TEXT("Steam 로그인 플레이어 ID가 유효하지 않습니다.");
		return false;
	}
	if (OutResponse.Tokens.AccessToken.IsEmpty() || OutResponse.Tokens.RefreshToken.IsEmpty())
	{
		OutResponse.bSuccess = false;
		OutError = TEXT("Steam 로그인 토큰이 비어 있습니다.");
		return false;
	}

	return true;
}

bool FFrontierOnlineHttpClient::ParseProfileResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineProfileResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineProfileResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Profile response body could not be parsed as JSON.");
		return false;
	}

	ParseResponseMeta(RootObject, OutResponse.RequestId, OutResponse.ServerTime);
	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Profile request failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}

	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess) || !OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty()
			? TEXT("Profile request was rejected by the backend.")
			: OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* DataObject = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), DataObject) || !DataObject || !DataObject->IsValid()
		|| !ParseRequiredInt64(*DataObject, TEXT("playerId"), OutResponse.PlayerId, OutError)
		|| !ParseRequiredString(*DataObject, TEXT("displayName"), OutResponse.DisplayName, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Profile response is missing required data.");
		}
		return false;
	}

	(*DataObject)->TryGetStringField(TEXT("locale"), OutResponse.Locale);

	return true;
}

bool FFrontierOnlineHttpClient::ParseRefreshResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineRefreshResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineRefreshResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Session refresh response body could not be parsed as JSON.");
		return false;
	}
	ParseResponseMeta(RootObject, OutResponse.RequestId, OutResponse.ServerTime);
	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Session refresh failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}
	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess) || !OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Session refresh was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* Data = nullptr;
	const TSharedPtr<FJsonObject>* Session = nullptr;
	const TSharedPtr<FJsonObject>* Tokens = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), Data) || !Data || !Data->IsValid()
		|| !(*Data)->TryGetObjectField(TEXT("session"), Session) || !Session || !Session->IsValid()
		|| !(*Data)->TryGetObjectField(TEXT("tokens"), Tokens) || !Tokens || !Tokens->IsValid())
	{
		OutError = TEXT("Session refresh response is missing data.session or data.tokens.");
		return false;
	}

	if (!ParseRequiredString(*Session, TEXT("sessionId"), OutResponse.Session.SessionId, OutError)
		|| !ParseRequiredIdentifier(
			*Session,
			TEXT("playerId"),
			OutResponse.Session.PlayerIdString,
			OutResponse.Session.PlayerId,
			OutError)
		|| !ParseRequiredString(*Session, TEXT("platform"), OutResponse.Session.Platform, OutError)
		|| !ParseRequiredString(*Session, TEXT("expiresAt"), OutResponse.Session.ExpiresAt, OutError)
		|| !ParseRequiredString(*Tokens, TEXT("tokenType"), OutResponse.Tokens.TokenType, OutError)
		|| !ParseRequiredString(*Tokens, TEXT("accessToken"), OutResponse.Tokens.AccessToken, OutError)
		|| !ParseRequiredString(*Tokens, TEXT("accessTokenExpiresAt"), OutResponse.Tokens.AccessTokenExpiresAt, OutError)
		|| !ParseRequiredString(*Tokens, TEXT("refreshToken"), OutResponse.Tokens.RefreshToken, OutError)
		|| !ParseRequiredString(*Tokens, TEXT("refreshTokenExpiresAt"), OutResponse.Tokens.RefreshTokenExpiresAt, OutError))
	{
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::ParseLogoutResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineLogoutResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineLogoutResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Logout response body could not be parsed as JSON.");
		return false;
	}
	ParseResponseMeta(RootObject, OutResponse.RequestId, OutResponse.ServerTime);
	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Logout failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}
	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess) || !OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Logout was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* Data = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), Data) || !Data || !Data->IsValid()
		|| !(*Data)->TryGetBoolField(TEXT("revoked"), OutResponse.bRevoked)
		|| !ParseRequiredInt32(*Data, TEXT("revokedSessionCount"), OutResponse.RevokedSessionCount, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Logout response is missing data.revoked or data.revokedSessionCount.");
		}
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::ParsePartyResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlinePartyResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlinePartyResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Party response body could not be parsed as JSON.");
		return false;
	}

	ParseResponseMeta(RootObject, OutResponse.RequestId, OutResponse.ServerTime);
	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Party request failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}

	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess) || !OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Party request was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonValue>* DataValue = RootObject->Values.Find(TEXT("data"));
	if (!DataValue || !DataValue->IsValid())
	{
		OutError = TEXT("Party response has no data field.");
		return false;
	}
	if ((*DataValue)->Type == EJson::Null)
	{
		return true;
	}
	if ((*DataValue)->Type != EJson::Object)
	{
		OutError = TEXT("Party response data is neither an object nor null.");
		return false;
	}
	const TSharedPtr<FJsonObject> DataObject = (*DataValue)->AsObject();

	const TSharedPtr<FJsonObject>* NestedPartyObject = nullptr;
	const TSharedPtr<FJsonObject> PartyObject =
		DataObject->TryGetObjectField(TEXT("party"), NestedPartyObject)
			&& NestedPartyObject
			&& NestedPartyObject->IsValid()
			? *NestedPartyObject
			: DataObject;
	if (!PartyObject->HasField(TEXT("partyId")))
	{
		OutError = TEXT("Party response data is missing partyId.");
		return false;
	}
	OutResponse.bHasData = ParsePartyDataObject(PartyObject, OutResponse.Data, OutError);
	return OutResponse.bHasData;
}

bool FFrontierOnlineHttpClient::ParseLeavePartyResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineLeavePartyResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineLeavePartyResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Party leave response body could not be parsed as JSON.");
		return false;
	}

	ParseResponseMeta(RootObject, OutResponse.RequestId, OutResponse.ServerTime);
	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Party leave failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}
	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess) || !OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Party leave was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* DataObject = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), DataObject) || !DataObject || !DataObject->IsValid())
	{
		OutError = TEXT("Party leave response has no data object.");
		return false;
	}
	if (!(*DataObject)->TryGetStringField(TEXT("partyId"), OutResponse.Data.PartyId)
		|| !ParseRequiredInt64(*DataObject, TEXT("leftPlayerId"), OutResponse.Data.LeftPlayerId, OutError)
		|| !(*DataObject)->TryGetStringField(TEXT("status"), OutResponse.Data.Status)
		|| !ParseRequiredInt32(*DataObject, TEXT("remainingMemberCount"), OutResponse.Data.RemainingMemberCount, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Party leave response is missing required data.");
		}
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::ParseInventoryResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineInventoryResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineInventoryResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Inventory response body could not be parsed as JSON.");
		return false;
	}

	ParseResponseMeta(RootObject, OutResponse.RequestId, OutResponse.ServerTime);

	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Inventory request failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}

	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess))
	{
		OutError = TEXT("Inventory response has no success field.");
		return false;
	}
	if (!OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Inventory request was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* DataObject = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), DataObject) || !DataObject || !DataObject->IsValid())
	{
		OutError = TEXT("Inventory response has no data object.");
		return false;
	}

	return ParseInventoryDataObject(*DataObject, OutResponse.Data, OutError);
}

bool FFrontierOnlineHttpClient::ParseItemUpgradeResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineItemUpgradeResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineItemUpgradeResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Item upgrade response body could not be parsed as JSON.");
		return false;
	}

	ParseResponseMeta(RootObject, OutResponse.RequestId, OutResponse.ServerTime);
	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Item upgrade failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}
	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess) || !OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Item upgrade was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* DataObject = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), DataObject) || !DataObject || !DataObject->IsValid())
	{
		OutError = TEXT("Item upgrade response has no data object.");
		return false;
	}
	if (!(*DataObject)->TryGetBoolField(TEXT("upgradeSucceeded"), OutResponse.bUpgradeSucceeded)
		|| !ParseRequiredInt32(*DataObject, TEXT("previousEnhancementLevel"), OutResponse.PreviousEnhancementLevel, OutError)
		|| !ParseRequiredInt32(*DataObject, TEXT("currentEnhancementLevel"), OutResponse.CurrentEnhancementLevel, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Item upgrade response is missing upgrade result fields.");
		}
		return false;
	}
	(*DataObject)->TryGetStringField(TEXT("failureReason"), OutResponse.FailureReason);

	const TSharedPtr<FJsonObject>* ItemObject = nullptr;
	if (!(*DataObject)->TryGetObjectField(TEXT("item"), ItemObject) || !ItemObject || !ItemObject->IsValid()
		|| !ParseUpgradeItem(*ItemObject, OutResponse.Item, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Item upgrade response has no valid item object.");
		}
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Materials = nullptr;
	if ((*DataObject)->TryGetArrayField(TEXT("consumedMaterials"), Materials) && Materials)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Materials)
		{
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				OutError = TEXT("consumedMaterials contains a non-object value.");
				return false;
			}
			const TSharedPtr<FJsonObject> MaterialObject = Value->AsObject();
			FFrontierOnlineConsumedUpgradeMaterial& Material = OutResponse.ConsumedMaterials.AddDefaulted_GetRef();
			MaterialObject->TryGetStringField(TEXT("itemInstanceId"), Material.ItemInstanceId);
			if (!ParseRequiredString(MaterialObject, TEXT("itemTemplateId"), Material.ItemTemplateId, OutError))
			{
				return false;
			}
			double Amount = 0.0;
			if (!TryGetNumberAlias(MaterialObject, { TEXT("consumedAmount"), TEXT("amount") }, Amount))
			{
				OutError = TEXT("consumedMaterials entry is missing consumedAmount/amount.");
				return false;
			}
			Material.ConsumedAmount = FMath::RoundToInt(Amount);
			double Remaining = 0.0;
			if (TryGetNumberAlias(MaterialObject, { TEXT("remainingQuantity") }, Remaining))
			{
				Material.bHasRemainingQuantity = true;
				Material.RemainingQuantity = FMath::RoundToInt(Remaining);
			}
			if (Material.ConsumedAmount < 0 || Material.RemainingQuantity < 0)
			{
				OutError = TEXT("consumedMaterials contains a negative quantity.");
				return false;
			}
		}
	}

	for (const TCHAR* FieldName : { TEXT("currencyChanges"), TEXT("consumedCurrencies"), TEXT("currencies") })
	{
		const TArray<TSharedPtr<FJsonValue>>* Currencies = nullptr;
		if (!(*DataObject)->TryGetArrayField(FieldName, Currencies) || !Currencies)
		{
			continue;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Currencies)
		{
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				OutError = FString::Printf(TEXT("%s contains a non-object value."), FieldName);
				return false;
			}
			const TSharedPtr<FJsonObject> CurrencyObject = Value->AsObject();
			FString CurrencyCode;
			if (!ParseRequiredString(CurrencyObject, TEXT("currencyCode"), CurrencyCode, OutError))
			{
				return false;
			}
			FFrontierOnlineUpgradeCurrencyChange* Currency = OutResponse.CurrencyChanges.FindByPredicate(
				[&CurrencyCode](const FFrontierOnlineUpgradeCurrencyChange& Existing)
				{
					return Existing.CurrencyCode.Equals(CurrencyCode, ESearchCase::IgnoreCase);
				});
			if (!Currency)
			{
				Currency = &OutResponse.CurrencyChanges.AddDefaulted_GetRef();
				Currency->CurrencyCode = CurrencyCode;
			}
			double Amount = 0.0;
			if (TryGetNumberAlias(CurrencyObject, { TEXT("consumedAmount"), TEXT("amount") }, Amount))
			{
				Currency->ConsumedAmount = FMath::RoundToInt64(Amount);
			}
			double Balance = 0.0;
			if (TryGetNumberAlias(CurrencyObject, { TEXT("balance") }, Balance))
			{
				Currency->bHasBalance = true;
				Currency->Balance = FMath::RoundToInt64(Balance);
			}
		}
	}

	const TSharedPtr<FJsonObject>* SnapshotObject = nullptr;
	if ((*DataObject)->TryGetObjectField(TEXT("inventory"), SnapshotObject) && SnapshotObject && SnapshotObject->IsValid()
		&& (*SnapshotObject)->HasField(TEXT("container")))
	{
		OutResponse.bHasInventory = ParseInventoryDataObject(*SnapshotObject, OutResponse.Inventory, OutError);
		if (!OutResponse.bHasInventory) return false;
	}
	if ((*DataObject)->TryGetObjectField(TEXT("storage"), SnapshotObject) && SnapshotObject && SnapshotObject->IsValid()
		&& (*SnapshotObject)->HasField(TEXT("container")))
	{
		OutResponse.bHasStorage = ParseStorageDataObject(*SnapshotObject, OutResponse.Storage, OutError);
		if (!OutResponse.bHasStorage) return false;
	}
	if ((*DataObject)->TryGetObjectField(TEXT("equipment"), SnapshotObject) && SnapshotObject && SnapshotObject->IsValid()
		&& (*SnapshotObject)->HasField(TEXT("container")))
	{
		OutResponse.bHasEquipment = ParseEquipmentDataObject(*SnapshotObject, OutResponse.Equipment, OutError);
		if (!OutResponse.bHasEquipment) return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::ParseStorageResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineStorageResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineStorageResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Storage response body could not be parsed as JSON.");
		return false;
	}

	ParseResponseMeta(RootObject, OutResponse.RequestId, OutResponse.ServerTime);

	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Storage request failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}

	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess))
	{
		OutError = TEXT("Storage response has no success field.");
		return false;
	}
	if (!OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Storage request was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* DataObject = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), DataObject) || !DataObject || !DataObject->IsValid())
	{
		OutError = TEXT("Storage response has no data object.");
		return false;
	}

	return ParseStorageDataObject(*DataObject, OutResponse.Data, OutError);
}

bool FFrontierOnlineHttpClient::ParseStorageTransferResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineStorageTransferResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineStorageTransferResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Storage transfer response body could not be parsed as JSON.");
		return false;
	}

	ParseResponseMeta(RootObject, OutResponse.RequestId, OutResponse.ServerTime);
	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Storage transfer request failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}

	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess))
	{
		OutError = TEXT("Storage transfer response has no success field.");
		return false;
	}
	if (!OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty()
			? TEXT("Storage transfer request was rejected by the backend.")
			: OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* DataObject = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), DataObject) || !DataObject || !DataObject->IsValid())
	{
		OutError = TEXT("Storage transfer response has no data object.");
		return false;
	}

	const TSharedPtr<FJsonObject>* InventoryObject = nullptr;
	if (!(*DataObject)->TryGetObjectField(TEXT("inventory"), InventoryObject) || !InventoryObject || !InventoryObject->IsValid())
	{
		OutError = TEXT("Storage transfer response has no data.inventory object.");
		return false;
	}
	if (!ParseInventoryDataObject(*InventoryObject, OutResponse.Data.Inventory, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* StorageObject = nullptr;
	if (!(*DataObject)->TryGetObjectField(TEXT("storage"), StorageObject) || !StorageObject || !StorageObject->IsValid())
	{
		OutError = TEXT("Storage transfer response has no data.storage object.");
		return false;
	}
	return ParseStorageDataObject(*StorageObject, OutResponse.Data.Storage, OutError);
}

bool FFrontierOnlineHttpClient::ParseLobbyBootstrapResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineLobbyBootstrapResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineLobbyBootstrapResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Lobby bootstrap response body could not be parsed as JSON.");
		return false;
	}

	ParseResponseMeta(RootObject, OutResponse.RequestId, OutResponse.ServerTime);

	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Lobby bootstrap request failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}

	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess))
	{
		OutError = TEXT("Lobby bootstrap response has no success field.");
		return false;
	}
	if (!OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Lobby bootstrap request was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* DataObject = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), DataObject) || !DataObject || !DataObject->IsValid())
	{
		OutError = TEXT("Lobby bootstrap response has no data object.");
		return false;
	}

	return ParseLobbyBootstrapDataObject(*DataObject, OutResponse.Data, OutError);
}

bool FFrontierOnlineHttpClient::ParsePlayerLevelResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlinePlayerLevelResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlinePlayerLevelResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Player level response body could not be parsed as JSON.");
		return false;
	}

	ParseResponseMeta(RootObject, OutResponse.RequestId, OutResponse.ServerTime);
	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Player level request failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}

	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess))
	{
		OutError = TEXT("Player level response has no success field.");
		return false;
	}
	if (!OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Player level request was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* DataObject = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), DataObject) || !DataObject || !DataObject->IsValid())
	{
		OutError = TEXT("Player level response has no data object.");
		return false;
	}
	return ParsePlayerLevelDataObject(*DataObject, OutResponse.Data, OutError);
}

bool FFrontierOnlineHttpClient::ParseMatchmakingTicketResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineMatchmakingTicketResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineMatchmakingTicketResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Matchmaking response body could not be parsed as JSON.");
		return false;
	}
	ParseResponseMeta(RootObject, OutResponse.RequestId, OutResponse.ServerTime);
	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutResponse.bRetryable = OutResponse.bRetryable || HttpStatus == 429 || HttpStatus == 503;
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Matchmaking request failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}
	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess) || !OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Matchmaking request was rejected.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* Data = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), Data) || !Data || !Data->IsValid()
		|| !ParseRequiredString(*Data, TEXT("ticketId"), OutResponse.Data.TicketId, OutError))
	{
		return false;
	}
	(*Data)->TryGetStringField(TEXT("matchMode"), OutResponse.Data.MatchMode);
	if (!ParseNullableString(*Data, TEXT("partyId"), OutResponse.Data.bHasPartyId, OutResponse.Data.PartyId)
		|| !ParseNullableString(*Data, TEXT("matchId"), OutResponse.Data.bHasMatchId, OutResponse.Data.MatchId)
		|| !ParseNullableString(*Data, TEXT("matchStatus"), OutResponse.Data.bHasMatchStatus, OutResponse.Data.MatchStatus))
	{
		OutError = TEXT("Matchmaking response contains an invalid nullable partyId, matchId, or matchStatus.");
		return false;
	}
	if (!(*Data)->TryGetStringField(TEXT("status"), OutResponse.Data.Status))
	{
		// The readable design document uses ticketStatus for GET while the current
		// OpenAPI aggregate uses status. Accept both during backend convergence.
		(*Data)->TryGetStringField(TEXT("ticketStatus"), OutResponse.Data.Status);
	}
	if (OutResponse.Data.Status.IsEmpty())
	{
		OutError = TEXT("Matchmaking response is missing status/ticketStatus.");
		return false;
	}
	double Number = 0.0;
	if ((*Data)->TryGetNumberField(TEXT("currentPlayerCount"), Number))
	{
		OutResponse.Data.CurrentPlayerCount = FMath::RoundToInt(Number);
	}
	if ((*Data)->TryGetNumberField(TEXT("maximumPlayerCount"), Number))
	{
		OutResponse.Data.MaximumPlayerCount = FMath::RoundToInt(Number);
	}
	if ((*Data)->TryGetNumberField(TEXT("maximumWaitSeconds"), Number))
	{
		OutResponse.Data.MaximumWaitSeconds = FMath::RoundToInt(Number);
	}
	return true;
}

bool FFrontierOnlineHttpClient::ParseRaidEntryResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineRaidEntryResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineRaidEntryResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Raid entry response body could not be parsed as JSON.");
		return false;
	}
	ParseResponseMeta(RootObject, OutResponse.RequestId, OutResponse.ServerTime);
	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutResponse.bRetryable = OutResponse.bRetryable || HttpStatus == 429 || HttpStatus == 503;
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Raid entry failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}
	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess) || !OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Raid entry was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* Data = nullptr;
	const TSharedPtr<FJsonObject>* RaidSession = nullptr;
	const TSharedPtr<FJsonObject>* Connection = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), Data) || !Data || !Data->IsValid()
		|| !(*Data)->TryGetObjectField(TEXT("raidSession"), RaidSession) || !RaidSession || !RaidSession->IsValid()
		|| !(*Data)->TryGetObjectField(TEXT("connection"), Connection) || !Connection || !Connection->IsValid()
		|| !ParseRaidSessionDTO(*RaidSession, OutResponse.Data.RaidSession, OutError)
		|| !ParseRequiredString(*Data, TEXT("joinToken"), OutResponse.Data.JoinToken, OutError)
		|| !ParseRequiredString(*Data, TEXT("joinTokenExpiresAt"), OutResponse.Data.JoinTokenExpiresAt, OutError)
		|| !ParseRequiredString(*Connection, TEXT("serverEndpoint"), OutResponse.Data.ServerEndpoint, OutError)
		|| !ParseRequiredString(*Connection, TEXT("transport"), OutResponse.Data.Transport, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Raid entry response is missing required data.");
		}
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::ParseJoinAuthorizationResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineJoinAuthorizationResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineJoinAuthorizationResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Join authorization response body could not be parsed as JSON.");
		return false;
	}
	ParseResponseMeta(RootObject, OutResponse.RequestId, OutResponse.ServerTime);
	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutResponse.bRetryable = OutResponse.bRetryable || !OutResponse.bTransportSucceeded || HttpStatus == 429 || HttpStatus == 503;
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Join authorization failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}
	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess) || !OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Join authorization was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* Data = nullptr;
	const TSharedPtr<FJsonObject>* RaidSession = nullptr;
	const TSharedPtr<FJsonObject>* Manifest = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), Data) || !Data || !Data->IsValid())
	{
		OutError = FString::Printf(
			TEXT("Join authorization response is missing object data. Root fields: %s"),
			*DescribeJsonFields(RootObject));
		return false;
	}
	if (!(*Data)->TryGetObjectField(TEXT("raidSession"), RaidSession) || !RaidSession || !RaidSession->IsValid())
	{
		OutError = FString::Printf(
			TEXT("Join authorization response is missing object data.raidSession. Data fields: %s"),
			*DescribeJsonFields(*Data));
		return false;
	}
	if (!(*Data)->TryGetObjectField(TEXT("loadoutManifest"), Manifest) || !Manifest || !Manifest->IsValid())
	{
		OutError = FString::Printf(
			TEXT("Join authorization response is missing object data.loadoutManifest. Data fields: %s"),
			*DescribeJsonFields(*Data));
		return false;
	}
	if (!ParseRaidSessionDTO(*RaidSession, OutResponse.RaidSession, OutError)
		|| !ParseRequiredInt64(*Data, TEXT("playerId"), OutResponse.PlayerId, OutError)
		|| !ParseRequiredString(*Data, TEXT("steamId"), OutResponse.SteamId, OutError)
		|| !ParseRaidLoadoutManifestDTO(*Manifest, OutResponse.LoadoutManifest, OutError))
	{
		return false;
	}
	// displayName is supplied by the profile-backed authorization contract. Keep it optional
	// so older backend responses remain join-compatible while the server rolls out the field.
	(*Data)->TryGetStringField(TEXT("displayName"), OutResponse.DisplayName);
	return true;
}

bool FFrontierOnlineHttpClient::ParseRaidResultResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineRaidResultResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineRaidResultResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Raid result response body could not be parsed as JSON.");
		return false;
	}
	ParseResponseMeta(RootObject, OutResponse.RequestId, OutResponse.ServerTime);
	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutResponse.bRetryable = OutResponse.bRetryable || HttpStatus == 429 || HttpStatus == 503;
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Raid result request failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}
	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess) || !OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Raid result request was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* Data = nullptr;
	const TSharedPtr<FJsonObject>* Result = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), Data) || !Data || !Data->IsValid()
		|| !(*Data)->TryGetObjectField(TEXT("result"), Result) || !Result || !Result->IsValid()
		|| !ParseRaidResultDTO(*Result, OutResponse.Result, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Raid result response is missing data.result.");
		}
		return false;
	}

	const TSharedPtr<FJsonValue> InventoryValue = (*Data)->TryGetField(TEXT("inventory"));
	if (InventoryValue.IsValid() && InventoryValue->Type != EJson::Null)
	{
		if (InventoryValue->Type != EJson::Object
			|| !ParseInventoryDataObject(InventoryValue->AsObject(), OutResponse.Inventory, OutError))
		{
			return false;
		}
		OutResponse.bHasInventory = true;
	}
	const TSharedPtr<FJsonValue> EquipmentValue = (*Data)->TryGetField(TEXT("equipment"));
	if (EquipmentValue.IsValid() && EquipmentValue->Type != EJson::Null)
	{
		if (EquipmentValue->Type != EJson::Object
			|| !ParseEquipmentDataObject(EquipmentValue->AsObject(), OutResponse.Equipment, OutError))
		{
			return false;
		}
		OutResponse.bHasEquipment = true;
	}
	if (!ParseNullableInt32(
		*Data,
		TEXT("retryAfterSeconds"),
		OutResponse.bHasRetryAfterSeconds,
		OutResponse.RetryAfterSeconds))
	{
		OutError = TEXT("Raid result retryAfterSeconds must be an integer or null.");
		return false;
	}
	return true;
}

bool FFrontierOnlineHttpClient::ParseRaidCommitResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	const bool bExtractResponse,
	FFrontierOnlineRaidCommitResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineRaidCommitResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Raid commit response body could not be parsed as JSON.");
		return false;
	}
	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutResponse.bRetryable = OutResponse.bRetryable || HttpStatus == 429 || HttpStatus == 503;
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Raid commit failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}
	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess) || !OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Raid commit was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* Data = nullptr;
	const TSharedPtr<FJsonObject>* Result = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), Data) || !Data || !Data->IsValid()
		|| !(*Data)->TryGetObjectField(TEXT("result"), Result) || !Result || !Result->IsValid()
		|| !ParseRaidResultDTO(*Result, OutResponse.Result, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Raid commit response is missing data.result.");
		}
		return false;
	}

	const TSharedPtr<FJsonObject>* Inventory = nullptr;
	if ((*Data)->TryGetObjectField(TEXT("inventory"), Inventory) && Inventory && Inventory->IsValid())
	{
		if (!ParseInventoryDataObject(*Inventory, OutResponse.Inventory, OutError))
		{
			return false;
		}
		OutResponse.bHasInventory = true;
	}
	const TSharedPtr<FJsonObject>* Equipment = nullptr;
	if ((*Data)->TryGetObjectField(TEXT("equipment"), Equipment) && Equipment && Equipment->IsValid())
	{
		if (!ParseEquipmentDataObject(*Equipment, OutResponse.Equipment, OutError))
		{
			return false;
		}
		OutResponse.bHasEquipment = true;
	}
	const TSharedPtr<FJsonObject>* Level = nullptr;
	if (bExtractResponse && (*Data)->TryGetObjectField(TEXT("level"), Level) && Level && Level->IsValid())
	{
		if (!ParsePlayerLevelDataObject(*Level, OutResponse.Level, OutError))
		{
			return false;
		}
		OutResponse.bHasLevel = true;
	}
	if (bExtractResponse)
	{
		const TArray<TSharedPtr<FJsonValue>>* MintedItems = nullptr;
		if ((*Data)->TryGetArrayField(TEXT("mintedItems"), MintedItems) && MintedItems)
		{
			OutResponse.MintedItems.Reset(MintedItems->Num());
			for (const TSharedPtr<FJsonValue>& Value : *MintedItems)
			{
				const TSharedPtr<FJsonObject> ItemObject =
					Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
				FFrontierOnlineMintedRaidItemDTO& MintedItem =
					OutResponse.MintedItems.AddDefaulted_GetRef();
				if (!ItemObject.IsValid()
					|| !ParseRequiredString(
						ItemObject,
						TEXT("raidItemId"),
						MintedItem.RaidItemId,
						OutError)
					|| !ParseRequiredString(
						ItemObject,
						TEXT("itemInstanceId"),
						MintedItem.ItemInstanceId,
						OutError))
				{
					OutError = OutError.IsEmpty()
						? TEXT("Raid extract response contains an invalid mintedItems entry.")
						: OutError;
					return false;
				}
			}
		}
	}
	return true;
}

bool FFrontierOnlineHttpClient::ParseExperienceGrantResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineExperienceGrantResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineExperienceGrantResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Experience grant response body could not be parsed as JSON.");
		return false;
	}
	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutResponse.bRetryable = OutResponse.bRetryable || HttpStatus == 429 || HttpStatus == 503;
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Experience grant failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}
	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess) || !OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Experience grant was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* Data = nullptr;
	const TSharedPtr<FJsonObject>* Level = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* LevelUps = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), Data) || !Data || !Data->IsValid()
		|| !(*Data)->TryGetObjectField(TEXT("level"), Level) || !Level || !Level->IsValid()
		|| !ParseRequiredInt32(*Data, TEXT("previousLevel"), OutResponse.PreviousLevel, OutError)
		|| !ParsePlayerLevelDataObject(*Level, OutResponse.Level, OutError)
		|| !(*Data)->TryGetArrayField(TEXT("levelUps"), LevelUps) || !LevelUps)
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Experience grant response is missing required data.");
		}
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *LevelUps)
	{
		if (!Value.IsValid() || Value->Type != EJson::Number)
		{
			OutError = TEXT("Experience grant levelUps contains a non-integer value.");
			return false;
		}
		const double Number = Value->AsNumber();
		const int64 Rounded = FMath::RoundToInt64(Number);
		if (!FMath::IsNearlyEqual(Number, static_cast<double>(Rounded))
			|| Rounded < 0 || Rounded > TNumericLimits<int32>::Max())
		{
			OutError = TEXT("Experience grant levelUps contains an invalid integer.");
			return false;
		}
		OutResponse.LevelUps.Add(static_cast<int32>(Rounded));
	}
	return true;
}

bool FFrontierOnlineHttpClient::ParseEquipmentResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineEquipmentResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineEquipmentResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Equipment response body could not be parsed as JSON.");
		return false;
	}

	ParseResponseMeta(RootObject, OutResponse.RequestId, OutResponse.ServerTime);

	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Equipment request failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}

	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess))
	{
		OutError = TEXT("Equipment response has no success field.");
		return false;
	}
	if (!OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Equipment request was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* DataObject = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), DataObject) || !DataObject || !DataObject->IsValid())
	{
		OutError = TEXT("Equipment response has no data object.");
		return false;
	}

	return ParseEquipmentDataObject(*DataObject, OutResponse.Data, OutError);
}

bool FFrontierOnlineHttpClient::ParseEquipmentChangeResponse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineEquipmentChangeResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineEquipmentChangeResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Equipment change response body could not be parsed as JSON.");
		return false;
	}

	ParseResponseMeta(RootObject, OutResponse.RequestId, OutResponse.ServerTime);

	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Equipment change request failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}

	if (!RootObject->TryGetBoolField(TEXT("success"), OutResponse.bSuccess))
	{
		OutError = TEXT("Equipment change response has no success field.");
		return false;
	}
	if (!OutResponse.bSuccess)
	{
		ParseErrorFields(RootObject, OutResponse.ErrorCode, OutResponse.Message, OutResponse.bRetryable);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Equipment change request was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* DataObject = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("data"), DataObject) || !DataObject || !DataObject->IsValid())
	{
		OutError = TEXT("Equipment change response has no data object.");
		return false;
	}

	const TSharedPtr<FJsonObject>* InventoryObject = nullptr;
	if (!(*DataObject)->TryGetObjectField(TEXT("inventory"), InventoryObject) || !InventoryObject || !InventoryObject->IsValid())
	{
		OutError = TEXT("Equipment change response has no data.inventory object.");
		return false;
	}
	if (!ParseInventoryDataObject(*InventoryObject, OutResponse.Inventory, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* EquipmentObject = nullptr;
	if (!(*DataObject)->TryGetObjectField(TEXT("equipment"), EquipmentObject) || !EquipmentObject || !EquipmentObject->IsValid())
	{
		OutError = TEXT("Equipment change response has no data.equipment object.");
		return false;
	}
	return ParseEquipmentDataObject(*EquipmentObject, OutResponse.Equipment, OutError);
}
