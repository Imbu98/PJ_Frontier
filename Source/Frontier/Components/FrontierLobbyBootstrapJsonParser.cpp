#include "Components/FrontierLobbyBootstrapJsonParser.h"

#include "Frontier.h"
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

bool ReadRequiredString(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FString& OutValue,
	FString& OutError)
{
	if (!Object.IsValid() || !Object->TryGetStringField(FieldName, OutValue))
	{
		OutError = FString::Printf(TEXT("Missing or invalid string field: %s"), FieldName);
		return false;
	}
	return true;
}

bool ReadRequiredStringAlias(
	const TSharedPtr<FJsonObject>& Object,
	std::initializer_list<const TCHAR*> FieldNames,
	FString& OutValue,
	FString& OutError)
{
	for (const TCHAR* FieldName : FieldNames)
	{
		if (Object.IsValid() && Object->TryGetStringField(FieldName, OutValue))
		{
			return true;
		}
	}

	OutError = TEXT("Missing or invalid containerId field.");
	return false;
}

bool ReadRequiredBool(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	bool& OutValue,
	FString& OutError)
{
	if (!Object.IsValid() || !Object->TryGetBoolField(FieldName, OutValue))
	{
		OutError = FString::Printf(TEXT("Missing or invalid bool field: %s"), FieldName);
		return false;
	}
	return true;
}

bool ReadRequiredInt64(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	int64& OutValue,
	FString& OutError)
{
	double Number = 0.0;
	if (!Object.IsValid() || !Object->TryGetNumberField(FieldName, Number))
	{
		OutError = FString::Printf(TEXT("Missing or invalid numeric field: %s"), FieldName);
		return false;
	}

	const int64 Rounded = FMath::RoundToInt64(Number);
	if (!FMath::IsFinite(Number) || !FMath::IsNearlyEqual(Number, static_cast<double>(Rounded)))
	{
		OutError = FString::Printf(TEXT("Numeric field is not an integer: %s"), FieldName);
		return false;
	}

	OutValue = Rounded;
	return true;
}

bool ReadRequiredInt32(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	int32& OutValue,
	FString& OutError)
{
	int64 Value = 0;
	if (!ReadRequiredInt64(Object, FieldName, Value, OutError)
		|| Value < TNumericLimits<int32>::Lowest()
		|| Value > TNumericLimits<int32>::Max())
	{
		if (OutError.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Numeric field is outside int32 range: %s"), FieldName);
		}
		return false;
	}

	OutValue = static_cast<int32>(Value);
	return true;
}

bool ReadIdentifier(
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

	if (OutString.IsNumeric())
	{
		const int64 NumericValue = FCString::Atoi64(*OutString);
		if (NumericValue > 0)
		{
			OutNumericId = NumericValue;
		}
	}
	return true;
}

bool ReadNullableString(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	bool& bOutHasValue,
	FString& OutValue)
{
	bOutHasValue = false;
	OutValue.Reset();
	const TSharedPtr<FJsonValue> Value = Object.IsValid() ? Object->TryGetField(FieldName) : nullptr;
	if (!Value.IsValid() || Value->Type == EJson::Null)
	{
		return true;
	}
	if (Value->Type != EJson::String)
	{
		return false;
	}

	bOutHasValue = true;
	OutValue = Value->AsString();
	return true;
}

bool ReadNullableDouble(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	bool& bOutHasValue,
	double& OutValue)
{
	bOutHasValue = false;
	OutValue = 0.0;
	const TSharedPtr<FJsonValue> Value = Object.IsValid() ? Object->TryGetField(FieldName) : nullptr;
	if (!Value.IsValid() || Value->Type == EJson::Null)
	{
		return true;
	}
	if (Value->Type != EJson::Number)
	{
		return false;
	}

	bOutHasValue = true;
	OutValue = Value->AsNumber();
	return FMath::IsFinite(OutValue);
}

bool ReadNullableInt32(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	bool& bOutHasValue,
	int32& OutValue)
{
	double Number = 0.0;
	if (!ReadNullableDouble(Object, FieldName, bOutHasValue, Number))
	{
		return false;
	}
	if (!bOutHasValue)
	{
		OutValue = 0;
		return true;
	}

	const int64 Rounded = FMath::RoundToInt64(Number);
	if (!FMath::IsNearlyEqual(Number, static_cast<double>(Rounded))
		|| Rounded < TNumericLimits<int32>::Lowest()
		|| Rounded > TNumericLimits<int32>::Max())
	{
		return false;
	}

	OutValue = static_cast<int32>(Rounded);
	return true;
}

bool GetRequiredObject(
	const TSharedPtr<FJsonObject>& Parent,
	const TCHAR* FieldName,
	const TSharedPtr<FJsonObject>*& OutObject,
	FString& OutError)
{
	if (!Parent.IsValid()
		|| !Parent->TryGetObjectField(FieldName, OutObject)
		|| !OutObject
		|| !OutObject->IsValid())
	{
		OutError = FString::Printf(TEXT("Lobby bootstrap response has no %s object."), FieldName);
		return false;
	}
	return true;
}

bool GetRequiredArray(
	const TSharedPtr<FJsonObject>& Parent,
	const TCHAR* FieldName,
	const TArray<TSharedPtr<FJsonValue>>*& OutArray,
	FString& OutError)
{
	if (!Parent.IsValid() || !Parent->TryGetArrayField(FieldName, OutArray) || !OutArray)
	{
		OutError = FString::Printf(TEXT("Lobby bootstrap response has no %s array."), FieldName);
		return false;
	}
	return true;
}

void ParseMeta(
	const TSharedPtr<FJsonObject>& Root,
	FFrontierOnlineLobbyBootstrapResponse& OutResponse)
{
	const TSharedPtr<FJsonObject>* Meta = nullptr;
	if (Root.IsValid() && Root->TryGetObjectField(TEXT("meta"), Meta) && Meta && Meta->IsValid())
	{
		(*Meta)->TryGetStringField(TEXT("requestId"), OutResponse.RequestId);
		(*Meta)->TryGetStringField(TEXT("serverTime"), OutResponse.ServerTime);
	}
}

void ParseError(
	const TSharedPtr<FJsonObject>& Root,
	FFrontierOnlineLobbyBootstrapResponse& OutResponse)
{
	if (!Root.IsValid())
	{
		return;
	}
	Root->TryGetStringField(TEXT("message"), OutResponse.Message);
	const TSharedPtr<FJsonObject>* Error = nullptr;
	if (Root->TryGetObjectField(TEXT("error"), Error) && Error && Error->IsValid())
	{
		(*Error)->TryGetStringField(TEXT("code"), OutResponse.ErrorCode);
		(*Error)->TryGetStringField(TEXT("message"), OutResponse.Message);
		(*Error)->TryGetBoolField(TEXT("retryable"), OutResponse.bRetryable);
	}
}

bool SerializeObject(const TSharedPtr<FJsonObject>& Object, FString& OutJson)
{
	OutJson.Reset();
	if (!Object.IsValid())
	{
		return false;
	}
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	return FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
}

bool TryNumberAlias(
	const TSharedPtr<FJsonObject>& Object,
	std::initializer_list<const TCHAR*> FieldNames,
	double& OutValue)
{
	for (const TCHAR* FieldName : FieldNames)
	{
		if (Object.IsValid() && Object->TryGetNumberField(FieldName, OutValue))
		{
			return FMath::IsFinite(OutValue);
		}
	}
	return false;
}

bool TryStringAlias(
	const TSharedPtr<FJsonObject>& Object,
	std::initializer_list<const TCHAR*> FieldNames,
	FString& OutValue)
{
	for (const TCHAR* FieldName : FieldNames)
	{
		if (Object.IsValid() && Object->TryGetStringField(FieldName, OutValue))
		{
			return true;
		}
	}
	return false;
}

bool ParseItem(
	const TSharedPtr<FJsonObject>& Object,
	FFrontierOnlineItemDTO& OutItem,
	FString& OutError)
{
	if (!ReadRequiredString(Object, TEXT("itemInstanceId"), OutItem.ItemInstanceId, OutError)
		|| !ReadRequiredString(Object, TEXT("itemTemplateId"), OutItem.ItemTemplateId, OutError)
		|| !ReadRequiredInt32(Object, TEXT("enhancementLevel"), OutItem.EnhancementLevel, OutError)
		|| !ReadRequiredString(Object, TEXT("finalRarityTag"), OutItem.FinalRarityTag, OutError)
		|| !ReadRequiredString(Object, TEXT("bindState"), OutItem.BindState, OutError))
	{
		return false;
	}

	if (!ReadNullableDouble(Object, TEXT("durability"), OutItem.bHasDurability, OutItem.Durability))
	{
		OutError = TEXT("durability must be a number or null.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Tags = nullptr;
	if (Object->TryGetArrayField(TEXT("instanceTags"), Tags) && Tags)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Tags)
		{
			if (!Value.IsValid() || Value->Type != EJson::String)
			{
				OutError = TEXT("instanceTags must contain only strings.");
				return false;
			}
			OutItem.InstanceTags.Add(Value->AsString());
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
	if (Object->TryGetArrayField(TEXT("randomOptions"), Options) && Options)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Options)
		{
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				OutError = TEXT("randomOptions contains a non-object value.");
				return false;
			}

			const TSharedPtr<FJsonObject> OptionObject = Value->AsObject();
			FFrontierOnlineItemOption& Option = OutItem.RandomOptions.AddDefaulted_GetRef();
			if (!TryStringAlias(OptionObject, { TEXT("optionId"), TEXT("statTag"), TEXT("statId") }, Option.OptionId)
				|| Option.OptionId.IsEmpty())
			{
				OutError = TEXT("randomOptions entry is missing optionId.");
				return false;
			}
			OptionObject->TryGetStringField(TEXT("unit"), Option.Unit);
			(void)TryNumberAlias(OptionObject, { TEXT("baseValue") }, Option.BaseValue);
			if (!TryNumberAlias(OptionObject, { TEXT("randomValue"), TEXT("rolledBonus"), TEXT("rolledRandomBonus"), TEXT("randomBonus") }, Option.RandomValue))
			{
				double RolledValue = 0.0;
				if (TryNumberAlias(OptionObject, { TEXT("rolledValue") }, RolledValue))
				{
					Option.RandomValue = RolledValue - Option.BaseValue;
				}
			}
			(void)TryNumberAlias(OptionObject, { TEXT("upgradeValue"), TEXT("enhancementBonus") }, Option.UpgradeValue);
			double ReforgeBonus = 0.0;
			if (TryNumberAlias(OptionObject, { TEXT("reforgeBonus") }, ReforgeBonus))
			{
				Option.UpgradeValue += ReforgeBonus;
			}
			if (!TryNumberAlias(OptionObject, { TEXT("finalValue"), TEXT("currentFinalValue"), TEXT("value") }, Option.FinalValue))
			{
				Option.FinalValue = Option.BaseValue + Option.RandomValue + Option.UpgradeValue;
			}
			Option.bHasNormalizedValue = TryNumberAlias(
				OptionObject,
				{ TEXT("normalizedValue"), TEXT("normalizedScore") },
				Option.NormalizedValue);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Skills = nullptr;
	if (Object->TryGetArrayField(TEXT("generatedSkills"), Skills) && Skills)
	{
		for (int32 Index = 0; Index < Skills->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& Value = (*Skills)[Index];
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				OutError = TEXT("generatedSkills contains a non-object value.");
				return false;
			}

			const TSharedPtr<FJsonObject> SkillObject = Value->AsObject();
			FFrontierOnlineGeneratedItemSkill Skill;
			(void)TryStringAlias(SkillObject, { TEXT("skillId"), TEXT("skillTemplateId"), TEXT("skillTag") }, Skill.SkillId);
			if (Skill.SkillId.IsEmpty())
			{
				FRONTIER_LOG(
					Warning,
					TEXT("Ignoring generatedSkills entry without a supported skill identifier. ItemInstanceId=%s ItemTemplateId=%s SkillIndex=%d Fields=%s"),
					*OutItem.ItemInstanceId,
					*OutItem.ItemTemplateId,
					Index,
					*DescribeJsonFields(SkillObject));
				continue;
			}

			double SlotNumber = static_cast<double>(Index);
			(void)TryNumberAlias(SkillObject, { TEXT("slotIndex") }, SlotNumber);
			Skill.SlotIndex = static_cast<int32>(FMath::RoundToInt64(SlotNumber));
			double SkillLevelNumber = 1.0;
			(void)TryNumberAlias(SkillObject, { TEXT("skillLevel"), TEXT("level") }, SkillLevelNumber);
			const int64 RoundedSkillLevel = FMath::RoundToInt64(SkillLevelNumber);
			if (!FMath::IsNearlyEqual(SkillLevelNumber, static_cast<double>(RoundedSkillLevel))
				|| RoundedSkillLevel < TNumericLimits<int32>::Lowest()
				|| RoundedSkillLevel > TNumericLimits<int32>::Max())
			{
				OutError = FString::Printf(TEXT("generatedSkills.level must be an integer for skillId: %s"), *Skill.SkillId);
				return false;
			}
			Skill.Level = static_cast<int32>(RoundedSkillLevel);
			OutItem.GeneratedSkills.Add(MoveTemp(Skill));
		}
	}

	Object->TryGetStringField(TEXT("createdAt"), OutItem.CreatedAt);
	Object->TryGetStringField(TEXT("acquiredAt"), OutItem.AcquiredAt);
	Object->TryGetStringField(TEXT("updatedAt"), OutItem.UpdatedAt);

	const TSharedPtr<FJsonValue> Metadata = Object->TryGetField(TEXT("metadata"));
	if (Metadata.IsValid() && Metadata->Type == EJson::Object)
	{
		OutItem.bHasMetadata = true;
		if (!SerializeObject(Metadata->AsObject(), OutItem.MetadataJson))
		{
			OutError = TEXT("Item metadata object could not be serialized.");
			return false;
		}
	}
	else if (Metadata.IsValid() && Metadata->Type == EJson::String)
	{
		OutItem.bHasMetadata = true;
		OutItem.MetadataJson = Metadata->AsString();
	}
	else if (Metadata.IsValid() && Metadata->Type != EJson::Null)
	{
		OutError = TEXT("Item metadata must be an object, string, or null.");
		return false;
	}

	return true;
}

bool ParseInventorySlot(
	const TSharedPtr<FJsonObject>& Object,
	FFrontierOnlineItemDTO& OutSlot,
	FString& OutError)
{
	if (!ParseItem(Object, OutSlot, OutError)
		|| !ReadRequiredInt32(Object, TEXT("slotIndex"), OutSlot.SlotIndex, OutError)
		|| !ReadRequiredInt32(Object, TEXT("quantity"), OutSlot.Quantity, OutError))
	{
		return false;
	}
	if (OutSlot.Quantity <= 0)
	{
		OutError = TEXT("Inventory item quantity must be greater than zero.");
		return false;
	}
	return true;
}

bool ParseInventory(
	const TSharedPtr<FJsonObject>& Object,
	FFrontierOnlineInventoryData& OutInventory,
	FString& OutError)
{
	const TSharedPtr<FJsonObject>* Container = nullptr;
	if (!GetRequiredObject(Object, TEXT("container"), Container, OutError)
		|| !ReadRequiredStringAlias(*Container, { TEXT("containerId"), TEXT("inventoryId") }, OutInventory.Container.InventoryId, OutError)
		|| !ReadRequiredInt64(*Container, TEXT("ownerPlayerId"), OutInventory.Container.OwnerPlayerId, OutError)
		|| !ReadRequiredInt32(*Container, TEXT("slotCapacity"), OutInventory.Container.SlotCapacity, OutError)
		|| !ReadRequiredInt32(*Container, TEXT("capacityLevel"), OutInventory.Container.CapacityLevel, OutError))
	{
		return false;
	}
	(*Container)->TryGetStringField(TEXT("updatedAt"), OutInventory.Container.UpdatedAt);
	if (OutInventory.Container.SlotCapacity <= 0)
	{
		OutError = TEXT("inventory.container.slotCapacity must be greater than zero.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Slots = nullptr;
	if (!GetRequiredArray(Object, TEXT("slots"), Slots, OutError))
	{
		return false;
	}
	TSet<int32> SeenIndices;
	for (const TSharedPtr<FJsonValue>& Value : *Slots)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			OutError = TEXT("Inventory slots contains a non-object value.");
			return false;
		}
		FFrontierOnlineItemDTO& Slot = OutInventory.Slots.AddDefaulted_GetRef();
		if (!ParseInventorySlot(Value->AsObject(), Slot, OutError)
			|| Slot.SlotIndex < 0
			|| Slot.SlotIndex >= OutInventory.Container.SlotCapacity
			|| SeenIndices.Contains(Slot.SlotIndex))
		{
			if (OutError.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Invalid or duplicate inventory slotIndex: %d"), Slot.SlotIndex);
			}
			return false;
		}
		SeenIndices.Add(Slot.SlotIndex);
	}

	const TSharedPtr<FJsonObject>* Upgrade = nullptr;
	if (Object->TryGetObjectField(TEXT("upgrade"), Upgrade) && Upgrade && Upgrade->IsValid())
	{
		if (!ReadNullableInt32(*Upgrade, TEXT("nextCapacityLevel"), OutInventory.Upgrade.bHasNextCapacityLevel, OutInventory.Upgrade.NextCapacityLevel)
			|| !ReadNullableInt32(*Upgrade, TEXT("nextSlotCapacity"), OutInventory.Upgrade.bHasNextSlotCapacity, OutInventory.Upgrade.NextSlotCapacity))
		{
			OutError = TEXT("Inventory upgrade object is invalid.");
			return false;
		}
	}

	return true;
}

bool ParseStorage(
	const TSharedPtr<FJsonObject>& Object,
	FFrontierOnlineStorageData& OutStorage,
	FString& OutError)
{
	const TSharedPtr<FJsonObject>* Container = nullptr;
	if (!GetRequiredObject(Object, TEXT("container"), Container, OutError)
		|| !ReadRequiredStringAlias(*Container, { TEXT("containerId"), TEXT("storageId") }, OutStorage.Container.StorageId, OutError)
		|| !ReadRequiredInt64(*Container, TEXT("ownerPlayerId"), OutStorage.Container.OwnerPlayerId, OutError)
		|| !ReadRequiredInt32(*Container, TEXT("slotCapacity"), OutStorage.Container.SlotCapacity, OutError)
		|| !ReadRequiredInt32(*Container, TEXT("capacityLevel"), OutStorage.Container.CapacityLevel, OutError))
	{
		return false;
	}
	(*Container)->TryGetStringField(TEXT("updatedAt"), OutStorage.Container.UpdatedAt);
	if (OutStorage.Container.SlotCapacity <= 0)
	{
		OutError = TEXT("storage.container.slotCapacity must be greater than zero.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Slots = nullptr;
	if (!GetRequiredArray(Object, TEXT("slots"), Slots, OutError))
	{
		return false;
	}
	TSet<int32> SeenIndices;
	for (const TSharedPtr<FJsonValue>& Value : *Slots)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			OutError = TEXT("Storage slots contains a non-object value.");
			return false;
		}
		FFrontierOnlineItemDTO& Slot = OutStorage.Slots.AddDefaulted_GetRef();
		if (!ParseInventorySlot(Value->AsObject(), Slot, OutError)
			|| Slot.SlotIndex < 0
			|| Slot.SlotIndex >= OutStorage.Container.SlotCapacity
			|| SeenIndices.Contains(Slot.SlotIndex))
		{
			if (OutError.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Invalid or duplicate storage slotIndex: %d"), Slot.SlotIndex);
			}
			return false;
		}
		SeenIndices.Add(Slot.SlotIndex);
	}

	const TSharedPtr<FJsonObject>* Upgrade = nullptr;
	if (Object->TryGetObjectField(TEXT("upgrade"), Upgrade) && Upgrade && Upgrade->IsValid())
	{
		if (!ReadRequiredInt32(*Upgrade, TEXT("capacityLevel"), OutStorage.Upgrade.CapacityLevel, OutError)
			|| !ReadRequiredInt32(*Upgrade, TEXT("maxCapacityLevel"), OutStorage.Upgrade.MaxCapacityLevel, OutError)
			|| !ReadNullableInt32(*Upgrade, TEXT("nextCapacityLevel"), OutStorage.Upgrade.bHasNextCapacityLevel, OutStorage.Upgrade.NextCapacityLevel)
			|| !ReadNullableInt32(*Upgrade, TEXT("nextSlotCapacity"), OutStorage.Upgrade.bHasNextSlotCapacity, OutStorage.Upgrade.NextSlotCapacity))
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Costs = nullptr;
		if ((*Upgrade)->TryGetArrayField(TEXT("nextUpgradeCost"), Costs) && Costs)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Costs)
			{
				if (!Value.IsValid() || Value->Type != EJson::Object)
				{
					OutError = TEXT("Storage nextUpgradeCost contains a non-object value.");
					return false;
				}
				FFrontierOnlineStorageCurrencyCost& Cost = OutStorage.Upgrade.NextUpgradeCost.AddDefaulted_GetRef();
				if (!ReadRequiredString(Value->AsObject(), TEXT("currencyCode"), Cost.CurrencyCode, OutError)
					|| !ReadRequiredInt64(Value->AsObject(), TEXT("amount"), Cost.Amount, OutError))
				{
					return false;
				}
			}
		}
	}

	return true;
}

bool ParseEquipment(
	const TSharedPtr<FJsonObject>& Object,
	FFrontierOnlineEquipmentData& OutEquipment,
	FString& OutError)
{
	const TSharedPtr<FJsonObject>* Container = nullptr;
	if (!GetRequiredObject(Object, TEXT("container"), Container, OutError)
		|| !ReadRequiredStringAlias(*Container, { TEXT("containerId"), TEXT("equipmentId") }, OutEquipment.Container.EquipmentId, OutError)
		|| !ReadRequiredInt64(*Container, TEXT("ownerPlayerId"), OutEquipment.Container.OwnerPlayerId, OutError))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Slots = nullptr;
	if (!GetRequiredArray(Object, TEXT("slots"), Slots, OutError))
	{
		return false;
	}
	TSet<FString> SeenSlotTypes;
	for (const TSharedPtr<FJsonValue>& Value : *Slots)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			OutError = TEXT("Equipment slots contains a non-object value.");
			return false;
		}

		const TSharedPtr<FJsonObject> SlotObject = Value->AsObject();
		FFrontierOnlineEquipmentSlot& Slot = OutEquipment.Slots.AddDefaulted_GetRef();
		if (!ReadRequiredString(SlotObject, TEXT("slotType"), Slot.SlotType, OutError)
			|| Slot.SlotType.IsEmpty()
			|| SeenSlotTypes.Contains(Slot.SlotType))
		{
			if (OutError.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Invalid or duplicate equipment slotType: %s"), *Slot.SlotType);
			}
			return false;
		}
		SeenSlotTypes.Add(Slot.SlotType);

		const TSharedPtr<FJsonValue> Item = SlotObject->TryGetField(TEXT("item"));
		if (!Item.IsValid() || Item->Type == EJson::Null)
		{
			continue;
		}
		if (Item->Type != EJson::Object)
		{
			OutError = TEXT("Equipment item must be an object or null.");
			return false;
		}
		Slot.bHasItem = true;
		if (!ParseItem(Item->AsObject(), Slot.Item, OutError))
		{
			return false;
		}
	}
	return true;
}

bool ParseRaid(
	const TSharedPtr<FJsonObject>& Object,
	FFrontierOnlineRaidSessionDTO& OutRaid,
	FString& OutError)
{
	return ReadRequiredString(Object, TEXT("raidSessionId"), OutRaid.RaidSessionId, OutError)
		&& ReadRequiredInt64(Object, TEXT("playerId"), OutRaid.PlayerId, OutError)
		&& ReadRequiredString(Object, TEXT("state"), OutRaid.State, OutError)
		&& ReadNullableString(Object, TEXT("serverId"), OutRaid.bHasServerId, OutRaid.ServerId)
		&& ReadRequiredString(Object, TEXT("createdAt"), OutRaid.CreatedAt, OutError)
		&& ReadNullableString(Object, TEXT("startedAt"), OutRaid.bHasStartedAt, OutRaid.StartedAt)
		&& ReadNullableString(Object, TEXT("completedAt"), OutRaid.bHasCompletedAt, OutRaid.CompletedAt);
}
}

bool FrontierLobbyBootstrapJson::Parse(
	const int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineLobbyBootstrapResponse& OutResponse,
	FString& OutError)
{
	OutError.Reset();
	OutResponse = FFrontierOnlineLobbyBootstrapResponse();
	OutResponse.bTransportSucceeded = true;
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Lobby bootstrap response body could not be parsed as JSON.");
		return false;
	}

	ParseMeta(Root, OutResponse);
	if (HttpStatus < 200 || HttpStatus >= 300)
	{
		ParseError(Root, OutResponse);
		OutError = OutResponse.Message.IsEmpty()
			? FString::Printf(TEXT("Lobby bootstrap request failed. HTTP %d ErrorCode=%s"), HttpStatus, *OutResponse.ErrorCode)
			: OutResponse.Message;
		return false;
	}

	if (!Root->TryGetBoolField(TEXT("success"), OutResponse.bSuccess))
	{
		OutError = TEXT("Lobby bootstrap response has no success field.");
		return false;
	}
	if (!OutResponse.bSuccess)
	{
		ParseError(Root, OutResponse);
		OutError = OutResponse.Message.IsEmpty() ? TEXT("Lobby bootstrap request was rejected by the backend.") : OutResponse.Message;
		return false;
	}

	const TSharedPtr<FJsonObject>* Data = nullptr;
	const TSharedPtr<FJsonObject>* Player = nullptr;
	if (!GetRequiredObject(Root, TEXT("data"), Data, OutError)
		|| !GetRequiredObject(*Data, TEXT("player"), Player, OutError)
		|| !ReadIdentifier(*Player, TEXT("playerId"), OutResponse.Data.Player.PlayerIdString, OutResponse.Data.Player.PlayerId, OutError))
	{
		return false;
	}
	(*Player)->TryGetStringField(TEXT("steamId"), OutResponse.Data.Player.SteamId);
	(*Player)->TryGetStringField(TEXT("nickname"), OutResponse.Data.Player.Nickname);

	const TArray<TSharedPtr<FJsonValue>>* Currencies = nullptr;
	if (!GetRequiredArray(*Data, TEXT("currencies"), Currencies, OutError))
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Currencies)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			OutError = TEXT("Lobby bootstrap currencies contains a non-object value.");
			return false;
		}
		FFrontierOnlineCurrencyDTO& Currency = OutResponse.Data.Currencies.AddDefaulted_GetRef();
		if (!ReadRequiredString(Value->AsObject(), TEXT("currencyCode"), Currency.CurrencyCode, OutError)
			|| !ReadRequiredInt64(Value->AsObject(), TEXT("balance"), Currency.Balance, OutError))
		{
			return false;
		}
	}

	const TSharedPtr<FJsonObject>* Inventory = nullptr;
	const TSharedPtr<FJsonObject>* Storage = nullptr;
	const TSharedPtr<FJsonObject>* Equipment = nullptr;
	if (!GetRequiredObject(*Data, TEXT("inventory"), Inventory, OutError)
		|| !ParseInventory(*Inventory, OutResponse.Data.Inventory, OutError)
		|| !GetRequiredObject(*Data, TEXT("storage"), Storage, OutError)
		|| !ParseStorage(*Storage, OutResponse.Data.Storage, OutError)
		|| !GetRequiredObject(*Data, TEXT("equipment"), Equipment, OutError)
		|| !ParseEquipment(*Equipment, OutResponse.Data.Equipment, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonValue> ActiveRaid = (*Data)->TryGetField(TEXT("activeRaid"));
	if (ActiveRaid.IsValid() && ActiveRaid->Type != EJson::Null)
	{
		if (ActiveRaid->Type != EJson::Object)
		{
			OutError = TEXT("Lobby bootstrap activeRaid must be an object or null.");
			return false;
		}
		OutResponse.Data.bHasActiveRaid = true;
		if (!ParseRaid(ActiveRaid->AsObject(), OutResponse.Data.ActiveRaid, OutError))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Lobby bootstrap activeRaid is invalid.");
			}
			return false;
		}
	}

	return true;
}
