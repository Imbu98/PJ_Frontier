#include "Progression/FrontierRaidSessionSubsystem.h"

#include "Components/FrontierLoadoutComponent.h"
#include "Components/FrontierRaidInventoryComponent.h"
#include "Dom/JsonObject.h"
#include "Frontier.h"
#include "FrontierOnlineConfig.h"
#include "FrontierOnlineHttpClient.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierGameMode.h"
#include "Game/FrontierLobbyPlayerController.h"
#include "Game/FrontierPlayerState.h"
#include "Inventory/FrontierBackendInventoryMapper.h"
#include "Inventory/FrontierRaidRuntimeItemMapper.h"
#include "Inventory/Items/FrontierItemCatalogSubsystem.h"
#include "Inventory/Persistence/FrontierItemPersistenceTypes.h"
#include "Online/FrontierPlayerSessionSubsystem.h"
#include "Progression/FrontierInternalApiSubsystem.h"
#include "Progression/FrontierRaidExperienceSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
TArray<TSharedPtr<FJsonValue>> ParsePreservedJsonArray(const FString& Json)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	if (Json.IsEmpty())
	{
		return Values;
	}
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Values);
	return Values;
}

TSharedRef<FJsonObject> MakeRuntimeItemJson(const FFrontierOnlineRaidRuntimeItemDTO& Item)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("raidItemId"), Item.RaidItemId);
	if (Item.bHasOriginItemInstanceId)
	{
		Json->SetStringField(TEXT("originItemInstanceId"), Item.OriginItemInstanceId);
	}
	else
	{
		Json->SetField(TEXT("originItemInstanceId"), MakeShared<FJsonValueNull>());
	}
	Json->SetStringField(TEXT("itemTemplateId"), Item.ItemTemplateId);
	Json->SetNumberField(TEXT("quantity"), Item.Quantity);
	if (Item.bHasDurability)
	{
		Json->SetNumberField(TEXT("durability"), Item.Durability);
	}
	else
	{
		Json->SetField(TEXT("durability"), MakeShared<FJsonValueNull>());
	}
	Json->SetNumberField(TEXT("enhancementLevel"), Item.EnhancementLevel);
	Json->SetStringField(TEXT("finalRarityTag"), Item.FinalRarityTag);
	Json->SetArrayField(TEXT("randomOptions"), ParsePreservedJsonArray(Item.RandomOptionsJson));
	Json->SetArrayField(TEXT("generatedSkills"), ParsePreservedJsonArray(Item.GeneratedSkillsJson));
	if (Item.bHasLootSourceId)
	{
		Json->SetStringField(TEXT("lootSourceId"), Item.LootSourceId);
	}
	else
	{
		Json->SetField(TEXT("lootSourceId"), MakeShared<FJsonValueNull>());
	}
	return Json;
}

TArray<TSharedPtr<FJsonValue>> MakeInventorySlotsJson(
	const TArray<FFrontierOnlineRaidInventorySlotDTO>& Slots)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	Values.Reserve(Slots.Num());
	for (const FFrontierOnlineRaidInventorySlotDTO& Slot : Slots)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("slotIndex"), Slot.SlotIndex);
		Json->SetObjectField(TEXT("item"), MakeRuntimeItemJson(Slot.Item));
		Values.Add(MakeShared<FJsonValueObject>(Json));
	}
	return Values;
}

TArray<TSharedPtr<FJsonValue>> MakeEquipmentSlotsJson(
	const TArray<FFrontierOnlineRaidEquipmentSlotDTO>& Slots)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	Values.Reserve(Slots.Num());
	for (const FFrontierOnlineRaidEquipmentSlotDTO& Slot : Slots)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("slotType"), Slot.SlotType);
		Json->SetObjectField(TEXT("item"), MakeRuntimeItemJson(Slot.Item));
		Values.Add(MakeShared<FJsonValueObject>(Json));
	}
	return Values;
}

TArray<TSharedPtr<FJsonValue>> MakeGuidArrayJson(const TSet<FGuid>& Guids)
{
	TArray<FString> SortedIds;
	SortedIds.Reserve(Guids.Num());
	for (const FGuid& Guid : Guids)
	{
		if (Guid.IsValid())
		{
			SortedIds.Add(Guid.ToString(EGuidFormats::DigitsWithHyphensLower));
		}
	}
	SortedIds.Sort();

	TArray<TSharedPtr<FJsonValue>> Values;
	Values.Reserve(SortedIds.Num());
	for (const FString& Id : SortedIds)
	{
		Values.Add(MakeShared<FJsonValueString>(Id));
	}
	return Values;
}

bool SerializeObject(const TSharedRef<FJsonObject>& Object, FString& OutBody)
{
	OutBody.Reset();
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutBody);
	return FJsonSerializer::Serialize(Object, Writer);
}

FString FormatGuid(const FGuid& Guid)
{
	return Guid.ToString(EGuidFormats::DigitsWithHyphensLower);
}

bool TryReadNumber(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	double& OutValue)
{
	return Object.IsValid() && Object->TryGetNumberField(FieldName, OutValue);
}

bool ParseRuntimeItemOptions(
	const FString& Json,
	TArray<FFrontierItemOptionDTO>& OutOptions,
	FString& OutError)
{
	OutOptions.Reset();
	const TArray<TSharedPtr<FJsonValue>> Values = ParsePreservedJsonArray(Json);
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
		TryReadNumber(Object, TEXT("baseValue"), Option.BaseValue);
		TryReadNumber(Object, TEXT("randomValue"), Option.RandomValue);
		TryReadNumber(Object, TEXT("upgradeValue"), Option.UpgradeValue);
		if (!TryReadNumber(Object, TEXT("finalValue"), Option.FinalValue))
		{
			// RaidRuntimeItemDTO permits opaque option objects and documents "value" as
			// the compact form. Preserve it as both base and final runtime value.
			if (TryReadNumber(Object, TEXT("value"), Option.FinalValue))
			{
				Option.BaseValue = Option.FinalValue;
			}
			else
			{
				Option.FinalValue = Option.BaseValue + Option.RandomValue + Option.UpgradeValue;
			}
		}
		if (TryReadNumber(Object, TEXT("normalizedValue"), Option.NormalizedValue))
		{
			Option.bHasNormalizedValue = true;
		}
		else
		{
			Option.bHasNormalizedValue = TryReadNumber(Object, TEXT("normalizedScore"), Option.NormalizedValue);
		}
	}
	return true;
}

bool ParseRuntimeItemSkills(
	const FString& Json,
	TArray<FFrontierGeneratedItemSkillDTO>& OutSkills,
	FString& OutError)
{
	OutSkills.Reset();
	const TArray<TSharedPtr<FJsonValue>> Values = ParsePreservedJsonArray(Json);
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
		Skill.Level = (TryReadNumber(Object, TEXT("skillLevel"), Number)
			|| TryReadNumber(Object, TEXT("level"), Number))
			? FMath::Max(1, FMath::RoundToInt(Number))
			: 1;
		Skill.SlotIndex = TryReadNumber(Object, TEXT("slotIndex"), Number)
			? FMath::RoundToInt(Number)
			: Index;
	}
	return true;
}

bool RegisterUniqueRaidItemId(
	const FGuid& RaidItemId,
	const FString& Location,
	TMap<FGuid, FString>& SeenLocations,
	FString& OutError)
{
	if (!RaidItemId.IsValid())
	{
		OutError = FString::Printf(
			TEXT("Raid item at %s has an invalid raidItemId."),
			*Location);
		return false;
	}

	if (const FString* ExistingLocation = SeenLocations.Find(RaidItemId))
	{
		OutError = FString::Printf(
			TEXT("Duplicate raidItemId %s found at %s and %s."),
			*FormatGuid(RaidItemId),
			**ExistingLocation,
			*Location);
		FRONTIER_LOG(Error, TEXT("%s"), *OutError);
		return false;
	}

	SeenLocations.Add(RaidItemId, Location);
	return true;
}

bool BuildRuntimeItemFromRaidItem(
	const FFrontierOnlineRaidRuntimeItemDTO& RaidItem,
	const UFrontierItemCatalogSubsystem& ItemCatalog,
	FFrontierItemInstance& OutItem,
	FString& OutError)
{
	return FFrontierRaidRuntimeItemMapper::TryBuildRuntimeItem(
		RaidItem,
		ItemCatalog,
		false,
		OutItem,
		OutError);
}

bool ApplyJoinManifestToPlayerState(
	AFrontierPlayerState& PlayerState,
	const FFrontierOnlineRaidLoadoutManifestDTO& Manifest,
	FString& OutError)
{
	UGameInstance* GameInstance = PlayerState.GetGameInstance();
	const UFrontierItemCatalogSubsystem* ItemCatalog = GameInstance
		? GameInstance->GetSubsystem<UFrontierItemCatalogSubsystem>()
		: nullptr;
	UFrontierRaidInventoryComponent* RaidInventory = PlayerState.GetRaidInventoryComponent();
	UFrontierLoadoutComponent* Loadout = PlayerState.GetLoadoutComponent();
	if (!ItemCatalog || !RaidInventory || !Loadout)
	{
		OutError = TEXT("Raid loadout initialization requires item catalog, inventory, and loadout components.");
		return false;
	}

	int32 Capacity = FMath::Max(
		1,
		RaidInventory->GetSlotCount() - RaidInventory->GetBonusSlotCount());
	for (const FFrontierOnlineRaidInventorySlotDTO& SourceSlot : Manifest.InventorySlots)
	{
		if (SourceSlot.SlotIndex < 0)
		{
			OutError = TEXT("Join loadout contains a negative inventory slot index.");
			return false;
		}
		Capacity = FMath::Max(Capacity, SourceSlot.SlotIndex + 1);
	}

	TArray<FFrontierInventorySlot> InventorySlots;
	InventorySlots.SetNum(Capacity);
	for (int32 Index = 0; Index < Capacity; ++Index)
	{
		InventorySlots[Index].SlotIndex = Index;
	}
	TSet<int32> OccupiedIndices;
	TMap<FGuid, FString> RaidItemLocations;
	for (const FFrontierOnlineRaidInventorySlotDTO& SourceSlot : Manifest.InventorySlots)
	{
		if (OccupiedIndices.Contains(SourceSlot.SlotIndex))
		{
			OutError = TEXT("Join loadout contains duplicate inventory slot indices.");
			return false;
		}
		OccupiedIndices.Add(SourceSlot.SlotIndex);

		FFrontierInventorySlot& TargetSlot = InventorySlots[SourceSlot.SlotIndex];
		if (!BuildRuntimeItemFromRaidItem(SourceSlot.Item, *ItemCatalog, TargetSlot.ItemInstance, OutError))
		{
			return false;
		}
		if (!RegisterUniqueRaidItemId(
				TargetSlot.ItemInstance.RaidItemId,
				FString::Printf(TEXT("inventorySlots[%d]"), SourceSlot.SlotIndex),
				RaidItemLocations,
				OutError))
		{
			return false;
		}
		TargetSlot.bOccupied = true;
	}

	TArray<FFrontierLoadoutSlot> LoadoutSlots;
	TSet<int32> OccupiedEquipmentSlots;
	for (const FFrontierOnlineRaidEquipmentSlotDTO& SourceSlot : Manifest.EquipmentSlots)
	{
		EFrontierEquipmentSlot SlotType = EFrontierEquipmentSlot::None;
		if (!FFrontierBackendInventoryMapper::ConvertBackendEquipmentSlot(
				SourceSlot.SlotType,
				SlotType,
				OutError)
			|| OccupiedEquipmentSlots.Contains(static_cast<int32>(SlotType)))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Join loadout contains a duplicate equipment slot.");
			}
			return false;
		}
		OccupiedEquipmentSlots.Add(static_cast<int32>(SlotType));

		FFrontierLoadoutSlot& TargetSlot = LoadoutSlots.AddDefaulted_GetRef();
		TargetSlot.SlotType = SlotType;
		if (!BuildRuntimeItemFromRaidItem(SourceSlot.Item, *ItemCatalog, TargetSlot.ItemInstance, OutError))
		{
			return false;
		}
		if (!RegisterUniqueRaidItemId(
				TargetSlot.ItemInstance.RaidItemId,
				FString::Printf(TEXT("equipmentSlots[%s]"), *SourceSlot.SlotType),
				RaidItemLocations,
				OutError))
		{
			return false;
		}
		TargetSlot.bOccupied = true;
	}

	if (!RaidInventory->ApplyAuthoritativeInventoryState(InventorySlots))
	{
		OutError = TEXT("Join loadout inventory snapshot could not be applied.");
		return false;
	}
	RaidInventory->ResetConsumedItemTracking();
	Loadout->SetLoadoutSlotsFromSnapshot(LoadoutSlots);
	return true;
}

FString BuildRuntimeOptionsJson(const FFrontierItemInstance& Item)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	Values.Reserve(Item.RuntimeGeneratedStats.Num());
	for (const FFrontierRuntimeStatData& Stat : Item.RuntimeGeneratedStats)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(
			TEXT("optionId"),
			Stat.OptionId.IsEmpty() ? Stat.StatTag.ToString() : Stat.OptionId);
		Object->SetStringField(TEXT("unit"), Stat.Unit);
		Object->SetNumberField(TEXT("baseValue"), Stat.BaseValue);
		Object->SetNumberField(TEXT("randomValue"), Stat.RandomValue);
		Object->SetNumberField(TEXT("upgradeValue"), Stat.UpgradeValue);
		Object->SetNumberField(TEXT("finalValue"), Stat.FinalValue);
		Object->SetNumberField(TEXT("normalizedValue"), Stat.NormalizedValue);
		Values.Add(MakeShared<FJsonValueObject>(Object));
	}

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Values, Writer);
	return Json;
}

FString BuildRuntimeSkillsJson(const FFrontierItemInstance& Item)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	Values.Reserve(Item.RuntimeGeneratedSkills.Num());
	for (const FFrontierRuntimeSkillData& Skill : Item.RuntimeGeneratedSkills)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(
			TEXT("skillId"),
			Skill.SkillTemplateId.IsEmpty() ? Skill.SkillTag.ToString() : Skill.SkillTemplateId);
		Object->SetNumberField(TEXT("level"), FMath::Max(1, Skill.SkillLevel));
		Object->SetNumberField(TEXT("slotIndex"), Skill.SlotIndex);
		Values.Add(MakeShared<FJsonValueObject>(Object));
	}

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Values, Writer);
	return Json;
}

bool BuildRaidRuntimeItem(
	const FFrontierItemInstance& Item,
	FFrontierOnlineRaidRuntimeItemDTO& OutItem,
	FString& OutError)
{
	if (!Item.IsValid())
	{
		OutError = TEXT("Current raid snapshot contains an invalid item.");
		return false;
	}

	FGuid RaidItemId = Item.RaidItemId;
	if (!RaidItemId.IsValid())
	{
		RaidItemId = Item.ItemInstanceId.IsValid() ? Item.ItemInstanceId : FGuid::NewGuid();
	}
	OutItem.RaidItemId = FormatGuid(RaidItemId);
	OutItem.bHasOriginItemInstanceId = Item.OriginItemInstanceId.IsValid();
	OutItem.OriginItemInstanceId = OutItem.bHasOriginItemInstanceId
		? FormatGuid(Item.OriginItemInstanceId)
		: FString();
	OutItem.ItemTemplateId = Item.GetTemplateId().ToString();
	OutItem.Quantity = Item.Quantity;
	OutItem.bHasDurability = Item.GetMaxDurability() > 0;
	OutItem.Durability = Item.Durability;
	OutItem.EnhancementLevel = Item.EnhancementLevel;
	OutItem.FinalRarityTag = ConvertItemRarityToBackendString(Item.GetDisplayRarity());
	OutItem.RandomOptionsJson = BuildRuntimeOptionsJson(Item);
	OutItem.GeneratedSkillsJson = BuildRuntimeSkillsJson(Item);
	OutItem.bHasLootSourceId = !Item.RaidLootSourceId.IsEmpty();
	OutItem.LootSourceId = Item.RaidLootSourceId;
	FRONTIER_LOG(
		Log,
		TEXT("[RaidExtractItem] RaidItemId=%s OriginItemInstanceId=%s ItemTemplateId=%s OptionCount=%d SkillCount=%d LootSourceId=%s"),
		*OutItem.RaidItemId,
		OutItem.bHasOriginItemInstanceId ? *OutItem.OriginItemInstanceId : TEXT("<none>"),
		*OutItem.ItemTemplateId,
		Item.RuntimeGeneratedStats.Num(),
		Item.RuntimeGeneratedSkills.Num(),
		OutItem.bHasLootSourceId ? *OutItem.LootSourceId : TEXT("<none>"));
	return true;
}

bool BuildCurrentRaidManifest(
	const AFrontierPlayerState& PlayerState,
	FFrontierOnlineRaidLoadoutManifestDTO& OutManifest,
	int32& OutInventoryCapacity,
	FString& OutError)
{
	const UFrontierRaidInventoryComponent* RaidInventory = PlayerState.GetRaidInventoryComponent();
	const UFrontierLoadoutComponent* Loadout = PlayerState.GetLoadoutComponent();
	if (!RaidInventory || !Loadout)
	{
		OutError = TEXT("Current raid inventory or loadout component is unavailable.");
		return false;
	}

	OutManifest = FFrontierOnlineRaidLoadoutManifestDTO();
	OutInventoryCapacity = RaidInventory->GetSlotCount();
	TMap<FGuid, FString> RaidItemLocations;
	for (const FFrontierInventorySlot& RuntimeSlot : RaidInventory->GetSlots())
	{
		if (!RuntimeSlot.bOccupied || !RuntimeSlot.ItemInstance.IsValid())
		{
			continue;
		}
		FFrontierOnlineRaidInventorySlotDTO& Slot = OutManifest.InventorySlots.AddDefaulted_GetRef();
		Slot.SlotIndex = RuntimeSlot.SlotIndex;
		if (!BuildRaidRuntimeItem(RuntimeSlot.ItemInstance, Slot.Item, OutError))
		{
			return false;
		}
		FGuid EffectiveRaidItemId;
		if (!FGuid::Parse(Slot.Item.RaidItemId, EffectiveRaidItemId)
			|| !RegisterUniqueRaidItemId(
				EffectiveRaidItemId,
				FString::Printf(TEXT("inventorySlots[%d]"), RuntimeSlot.SlotIndex),
				RaidItemLocations,
				OutError))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Current raid inventory contains an invalid raidItemId.");
			}
			return false;
		}
	}

	for (const FFrontierLoadoutSlot& RuntimeSlot : Loadout->GetLoadoutSlots())
	{
		if (!RuntimeSlot.bOccupied || !RuntimeSlot.ItemInstance.IsValid())
		{
			continue;
		}
		FFrontierOnlineRaidEquipmentSlotDTO& Slot = OutManifest.EquipmentSlots.AddDefaulted_GetRef();
		if (!FFrontierBackendInventoryMapper::ConvertEquipmentSlotToBackend(
				RuntimeSlot.SlotType,
				Slot.SlotType,
				OutError)
			|| !BuildRaidRuntimeItem(RuntimeSlot.ItemInstance, Slot.Item, OutError))
		{
			return false;
		}
		FGuid EffectiveRaidItemId;
		if (!FGuid::Parse(Slot.Item.RaidItemId, EffectiveRaidItemId)
			|| !RegisterUniqueRaidItemId(
				EffectiveRaidItemId,
				FString::Printf(TEXT("equipmentSlots[%s]"), *Slot.SlotType),
				RaidItemLocations,
				OutError))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Current raid equipment contains an invalid raidItemId.");
			}
			return false;
		}
	}
	return true;
}

FString OutcomeToWire(const EFrontierRaidOutcome Outcome)
{
	return Outcome == EFrontierRaidOutcome::Extracted ? TEXT("EXTRACTED") : TEXT("DEAD");
}
}

void UFrontierRaidSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	InternalApi = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFrontierInternalApiSubsystem>() : nullptr;
}

void UFrontierRaidSessionSubsystem::Deinitialize()
{
	ClientRaidSessionId.Reset();
	ServerContexts.Reset();
	RaidIdByController.Reset();
	ConflictConfirmationClients.Reset();
	PendingSettlementPresentation = FFrontierRaidSettlementPresentation();
	bHasPreRaidLevel = false;
	bAwaitingSettlementLevelRefresh = false;
	InternalApi = nullptr;
	Super::Deinitialize();
}

bool UFrontierRaidSessionSubsystem::CanTransition(
	const EFrontierRaidFlowState From,
	const EFrontierRaidFlowState To)
{
	if (To == EFrontierRaidFlowState::RetryableFailure)
	{
		return From != EFrontierRaidFlowState::Completed;
	}
	switch (From)
	{
	case EFrontierRaidFlowState::Idle:
	case EFrontierRaidFlowState::Completed:
	case EFrontierRaidFlowState::RetryableFailure:
		return To == EFrontierRaidFlowState::CreatingEntry;
	case EFrontierRaidFlowState::CreatingEntry:
		return To == EFrontierRaidFlowState::AuthorizingJoin;
	case EFrontierRaidFlowState::AuthorizingJoin:
		return To == EFrontierRaidFlowState::RaidActiveSimulated;
	case EFrontierRaidFlowState::RaidActiveSimulated:
		return To == EFrontierRaidFlowState::FinalizingLocalResult;
	case EFrontierRaidFlowState::FinalizingLocalResult:
		return To == EFrontierRaidFlowState::CommittingRaidResult;
	case EFrontierRaidFlowState::CommittingRaidResult:
		return To == EFrontierRaidFlowState::GrantingExperience;
	case EFrontierRaidFlowState::GrantingExperience:
		return To == EFrontierRaidFlowState::ReturningToLobby;
	case EFrontierRaidFlowState::ReturningToLobby:
		return To == EFrontierRaidFlowState::RefreshingLobby;
	case EFrontierRaidFlowState::RefreshingLobby:
		return To == EFrontierRaidFlowState::Completed;
	default:
		return false;
	}
}

bool UFrontierRaidSessionSubsystem::IsDebugExperienceAllowed(
	const EFrontierRaidFlowState State)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return State == EFrontierRaidFlowState::RaidActiveSimulated;
#endif
}

bool UFrontierRaidSessionSubsystem::DoesRaidResultConfirmCommit(
	const FFrontierOnlineRaidResultDTO& Result,
	const FString& ExpectedRaidSessionId,
	const int64 ExpectedPlayerId,
	const EFrontierRaidOutcome ExpectedOutcome)
{
	return Result.RaidSessionId.Equals(ExpectedRaidSessionId, ESearchCase::CaseSensitive)
		&& Result.PlayerId == ExpectedPlayerId
		&& Result.Outcome.Equals(OutcomeToWire(ExpectedOutcome), ESearchCase::CaseSensitive)
		&& Result.bHasCommittedAt
		&& !Result.CommittedAt.IsEmpty();
}

void UFrontierRaidSessionSubsystem::TransitionClient(
	const EFrontierRaidFlowState NewState,
	const FString& Message)
{
	if (ClientState != NewState && !CanTransition(ClientState, NewState))
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Rejected invalid client raid-flow transition. From=%d To=%d"),
			static_cast<int32>(ClientState),
			static_cast<int32>(NewState));
		return;
	}
	ClientState = NewState;
	OnRaidFlowStateChanged.Broadcast(ClientState, Message);
}

bool UFrontierRaidSessionSubsystem::BeginClientRaidEntry(FString& OutError)
{
	OutError.Reset();
	const FString StateName = StaticEnum<EFrontierRaidFlowState>()->GetNameStringByValue(
		static_cast<int64>(ClientState));
	if (ClientState != EFrontierRaidFlowState::Idle
		&& ClientState != EFrontierRaidFlowState::Completed
		&& ClientState != EFrontierRaidFlowState::RetryableFailure)
	{
		OutError = TEXT("A raid entry or settlement operation is already in progress.");
		FRONTIER_LOG(
			Error,
			TEXT("[RaidEntry] Client entry rejected: state is not enterable. State=%s(%d) RaidSessionId=%s Error=%s"),
			*StateName,
			static_cast<int32>(ClientState),
			ClientRaidSessionId.IsEmpty() ? TEXT("<empty>") : *ClientRaidSessionId,
			*OutError);
		return false;
	}
	if (ClientState == EFrontierRaidFlowState::RetryableFailure
		&& !ClientRaidSessionId.IsEmpty())
	{
		OutError = TEXT("The existing Raid settlement failed and must be recovered before creating another entry.");
		FRONTIER_LOG(
			Error,
			TEXT("[RaidEntry] Client entry rejected: a failed settlement still owns raid context. State=%s(%d) RaidSessionId=%s Error=%s"),
			*StateName,
			static_cast<int32>(ClientState),
			*ClientRaidSessionId,
			*OutError);
		return false;
	}
	ClientRaidSessionId.Reset();
	LastResult = FFrontierRaidResultSnapshot();
	FRONTIER_LOG(
		Log,
		TEXT("[RaidEntry] Client entry accepted. PreviousState=%s(%d)"),
		*StateName,
		static_cast<int32>(ClientState));
	TransitionClient(EFrontierRaidFlowState::CreatingEntry, TEXT("레이드 입장을 생성하는 중입니다."));
	return true;
}

void UFrontierRaidSessionSubsystem::AcceptClientRaidEntry(
	const FFrontierOnlineRaidEntryDTO& Entry)
{
	AcceptClientRaidEntryContext(Entry.RaidSession.RaidSessionId);
}

void UFrontierRaidSessionSubsystem::AcceptClientRaidEntryContext(
	const FString& RaidSessionId)
{
	if (ClientState != EFrontierRaidFlowState::CreatingEntry)
	{
		return;
	}
	ClientRaidSessionId = RaidSessionId;
	TransitionClient(EFrontierRaidFlowState::AuthorizingJoin, TEXT("전용 서버가 입장 토큰을 승인하는 중입니다."));
}

void UFrontierRaidSessionSubsystem::FailClientFlow(
	const FString& Error,
	const bool bRetryable)
{
	TransitionClient(EFrontierRaidFlowState::RetryableFailure, Error);
}

void UFrontierRaidSessionSubsystem::MarkClientRaidActive(
	const FString& RaidSessionId)
{
	if (ClientState != EFrontierRaidFlowState::AuthorizingJoin
		|| !ClientRaidSessionId.Equals(RaidSessionId, ESearchCase::CaseSensitive))
	{
		return;
	}
	TransitionClient(EFrontierRaidFlowState::RaidActiveSimulated, TEXT("RaidActiveSimulated"));
}

void UFrontierRaidSessionSubsystem::MarkClientSettlementState(
	const EFrontierRaidFlowState NewState,
	const FString& Message)
{
	TransitionClient(NewState, Message);
}

void UFrontierRaidSessionSubsystem::BeginClientLobbyRefresh()
{
	if (ClientState == EFrontierRaidFlowState::ReturningToLobby)
	{
		TransitionClient(EFrontierRaidFlowState::RefreshingLobby, TEXT("로비 레벨과 레이드 결과를 갱신하는 중입니다."));
	}
}

void UFrontierRaidSessionSubsystem::CompleteClientLobbyRefresh(
	const FFrontierOnlineRaidResultDTO& Result)
{
	if (ClientState != EFrontierRaidFlowState::RefreshingLobby)
	{
		return;
	}
	LastResult.RaidSessionId = Result.RaidSessionId;
	LastResult.Outcome = Result.Outcome;
	LastResult.ReasonCode = Result.bHasReasonCode ? Result.ReasonCode : FString();
	LastResult.CommittedAt = Result.bHasCommittedAt ? Result.CommittedAt : FString();
	OnRaidResultChanged.Broadcast(LastResult);
	TransitionClient(EFrontierRaidFlowState::Completed, TEXT("레이드 정산과 로비 갱신이 완료되었습니다."));
}

void UFrontierRaidSessionSubsystem::CachePreRaidLevel(const FFrontierPlayerLevelSnapshot& Level)
{
	PreRaidLevel = Level;
	bHasPreRaidLevel = Level.Level > 0;
	PendingSettlementPresentation = FFrontierRaidSettlementPresentation();
	bAwaitingSettlementLevelRefresh = false;
}

void UFrontierRaidSessionSubsystem::MarkClientSettlementCommitted(
	const FString& RaidSessionId,
	const EFrontierRaidOutcome Outcome,
	const int64 AwardedExperience)
{
	PendingSettlementRaidSessionId = RaidSessionId;
	PendingSettlementOutcome = Outcome;
	PendingSettlementExperience = FMath::Max<int64>(0, AwardedExperience);
	bAwaitingSettlementLevelRefresh = true;
	if (ClientState == EFrontierRaidFlowState::CommittingRaidResult)
	{
		MarkClientSettlementState(EFrontierRaidFlowState::GrantingExperience);
	}
	MarkClientSettlementState(EFrontierRaidFlowState::ReturningToLobby);
}

bool UFrontierRaidSessionSubsystem::CompletePendingSettlementPresentation(
	const FFrontierPlayerLevelSnapshot& AuthoritativeAfterLevel,
	FFrontierRaidSettlementPresentation& OutPresentation)
{
	OutPresentation = FFrontierRaidSettlementPresentation();
	if (!bAwaitingSettlementLevelRefresh || !bHasPreRaidLevel || AuthoritativeAfterLevel.Level <= 0)
	{
		return false;
	}

	PendingSettlementPresentation.bValid = true;
	PendingSettlementPresentation.RaidSessionId = PendingSettlementRaidSessionId;
	PendingSettlementPresentation.Outcome = PendingSettlementOutcome;
	PendingSettlementPresentation.AwardedExperience = PendingSettlementExperience;
	PendingSettlementPresentation.BeforeLevel = PreRaidLevel;
	PendingSettlementPresentation.AfterLevel = AuthoritativeAfterLevel;
	OutPresentation = PendingSettlementPresentation;

	if (ClientState == EFrontierRaidFlowState::ReturningToLobby)
	{
		TransitionClient(EFrontierRaidFlowState::RefreshingLobby, TEXT("최종 레벨 정보를 확인했습니다."));
	}
	if (ClientState == EFrontierRaidFlowState::RefreshingLobby)
	{
		TransitionClient(EFrontierRaidFlowState::Completed, TEXT("레이드 정산이 완료되었습니다."));
	}

	bAwaitingSettlementLevelRefresh = false;
	PendingSettlementRaidSessionId.Reset();
	PendingSettlementExperience = 0;
	return true;
}

bool UFrontierRaidSessionSubsystem::ConsumeSettlementPresentation(
	FFrontierRaidSettlementPresentation& OutPresentation)
{
	OutPresentation = PendingSettlementPresentation;
	if (!PendingSettlementPresentation.bValid)
	{
		return false;
	}

	PendingSettlementPresentation = FFrontierRaidSettlementPresentation();
	return true;
}

bool UFrontierRaidSessionSubsystem::BeginServerJoinAuthorization(
	APlayerController* Controller,
	const FFrontierOnlineRaidEntryDTO& Entry,
	FString& OutError)
{
	OutError.Reset();
	if (!Controller || !Controller->HasAuthority() || !InternalApi)
	{
		OutError = TEXT("Join authorization requires a dedicated-server controller and Internal API subsystem.");
		return false;
	}
	if (Entry.RaidSession.RaidSessionId.IsEmpty() || Entry.JoinToken.IsEmpty()
		|| !Entry.RaidSession.bHasServerId || Entry.RaidSession.ServerId.IsEmpty())
	{
		OutError = TEXT("Raid entry is missing raidSessionId, serverId, or join token.");
		return false;
	}
	if (RaidIdByController.Contains(Controller) || ServerContexts.Contains(Entry.RaidSession.RaidSessionId))
	{
		OutError = TEXT("Join authorization is already in progress for this controller or raid.");
		return false;
	}

	FServerRaidContext& Context = ServerContexts.Add(Entry.RaidSession.RaidSessionId);
	Context.Controller = Controller;
	Context.PlayerState = Controller->GetPlayerState<AFrontierPlayerState>();
	if (!Context.PlayerState.IsValid())
	{
		ServerContexts.Remove(Entry.RaidSession.RaidSessionId);
		OutError = TEXT("Join authorization requires a valid authoritative PlayerState.");
		return false;
	}
	Context.RaidSessionId = Entry.RaidSession.RaidSessionId;
	Context.ServerId = Entry.RaidSession.ServerId;
	Context.JoinToken = Entry.JoinToken;
	Context.PlayerConnectionId = FString::Printf(
		TEXT("frontier-connection-%d-%u"),
		Context.PlayerState.IsValid() ? Context.PlayerState->GetPlayerId() : 0,
		Controller->GetUniqueID());
	RaidIdByController.Add(Controller, Context.RaidSessionId);

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("joinToken"), Context.JoinToken);
	Body->SetStringField(TEXT("playerConnectionId"), Context.PlayerConnectionId);
	Body->SetStringField(TEXT("serverId"), Context.ServerId);
	FFrontierInternalApiRequest Request;
	Request.Url = InternalApi->GetConfig().BuildJoinAuthorizationUrl(Context.RaidSessionId);
	Request.RaidServerId = Context.ServerId;
	Request.bIncludeRaidServerHeader = true;
	if (!SerializeObject(Body, Request.Body))
	{
		ServerContexts.Remove(Context.RaidSessionId);
		RaidIdByController.Remove(Controller);
		OutError = TEXT("Join authorization body could not be serialized.");
		return false;
	}

	TArray<FString> JoinTokenSegments;
	Context.JoinToken.ParseIntoArray(JoinTokenSegments, TEXT("."), false);
	FRONTIER_LOG(
		Log,
		TEXT("[RaidJoinAuthorization] Queueing session-scoped authorization. RaidSessionId=%s ServerId=%s ConfiguredRaidServerId=%s PlayerConnectionId=%s JoinTokenLength=%d JoinTokenSegments=%d BodyLength=%d"),
		*Context.RaidSessionId,
		*Context.ServerId,
		InternalApi->GetConfig().RaidServerId.IsEmpty() ? TEXT("<empty>") : *InternalApi->GetConfig().RaidServerId,
		*Context.PlayerConnectionId,
		Context.JoinToken.Len(),
		JoinTokenSegments.Num(),
		Request.Body.Len());

	const FString RaidSessionId = Context.RaidSessionId;
	const TWeakObjectPtr<UFrontierRaidSessionSubsystem> WeakThis(this);
	if (!InternalApi->QueueAuthorizedRequest(
		MoveTemp(Request),
		[WeakThis, RaidSessionId](const FFrontierInternalApiResponse& Response)
		{
			if (UFrontierRaidSessionSubsystem* This = WeakThis.Get())
			{
				This->HandleJoinAuthorizationCompleted(RaidSessionId, Response);
			}
		},
		OutError))
	{
		ServerContexts.Remove(RaidSessionId);
		RaidIdByController.Remove(Controller);
		return false;
	}
	return true;
}

bool UFrontierRaidSessionSubsystem::HasServerRaidContext(
	APlayerController* Controller) const
{
	return Controller && RaidIdByController.Contains(Controller);
}

void UFrontierRaidSessionSubsystem::RemoveServerRaidContext(APlayerController* Controller)
{
	if (!Controller)
	{
		return;
	}

	FString RaidSessionId;
	if (!RaidIdByController.RemoveAndCopyValue(Controller, RaidSessionId))
	{
		return;
	}

	ConflictConfirmationClients.Remove(RaidSessionId);
	ServerContexts.Remove(RaidSessionId);
}

bool UFrontierRaidSessionSubsystem::IsServerRaidActive(APlayerController* Controller) const
{
	const FString* RaidSessionId = Controller ? RaidIdByController.Find(Controller) : nullptr;
	const FServerRaidContext* Context = RaidSessionId ? ServerContexts.Find(*RaidSessionId) : nullptr;
	return Context && Context->State == EFrontierRaidFlowState::RaidActiveSimulated;
}

void UFrontierRaidSessionSubsystem::HandleJoinAuthorizationCompleted(
	const FString& RaidSessionId,
	const FFrontierInternalApiResponse& TransportResponse)
{
	FServerRaidContext* Context = ServerContexts.Find(RaidSessionId);
	if (!Context)
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidJoinAuthorization] Authorization response ignored because the server raid context no longer exists. RaidSessionId=%s"),
			*RaidSessionId);
		return;
	}

	FRONTIER_LOG(
		Log,
		TEXT("[RaidJoinAuthorization] Authorization response received. RaidSessionId=%s Transport=%d HttpStatus=%d BodyLength=%d ErrorCode=%s Message=%s"),
		*RaidSessionId,
		TransportResponse.bTransportSucceeded ? 1 : 0,
		TransportResponse.HttpStatus,
		TransportResponse.ResponseBody.Len(),
		TransportResponse.ErrorCode.IsEmpty() ? TEXT("<empty>") : *TransportResponse.ErrorCode,
		TransportResponse.Message.IsEmpty() ? TEXT("<empty>") : *TransportResponse.Message);

	FFrontierOnlineJoinAuthorizationResponse Parsed;
	FString ParseError;
	if (!TransportResponse.bSucceeded)
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidJoinAuthorization] Authorization transport failed. RaidSessionId=%s Retryable=%d Error=%s"),
			*RaidSessionId,
			TransportResponse.bRetryable ? 1 : 0,
			TransportResponse.Message.IsEmpty() ? TEXT("<empty>") : *TransportResponse.Message);
		FailServerFlow(RaidSessionId, TransportResponse.Message, TransportResponse.bRetryable);
		return;
	}
	if (!FFrontierOnlineHttpClient::ParseJoinAuthorizationResponse(
			TransportResponse.HttpStatus,
			TransportResponse.ResponseBody,
			Parsed,
			ParseError))
	{
		if (ParseError.IsEmpty())
		{
			ParseError = TEXT("Join authorization response parsing failed without a detailed parser error.");
		}
		FRONTIER_LOG(
			Error,
			TEXT("[RaidJoinAuthorization] Authorization failed. RaidSessionId=%s HttpStatus=%d TransportSucceeded=%d ErrorCode=%s Retryable=%d Message=%s ResponseBodyLength=%d"),
			*RaidSessionId,
			TransportResponse.HttpStatus,
			TransportResponse.bTransportSucceeded ? 1 : 0,
			TransportResponse.ErrorCode.IsEmpty() ? TEXT("<empty>") : *TransportResponse.ErrorCode,
			TransportResponse.bRetryable ? 1 : 0,
			ParseError.IsEmpty() ? *TransportResponse.Message : *ParseError,
			TransportResponse.ResponseBody.Len());
		FailServerFlow(RaidSessionId, ParseError, TransportResponse.bRetryable);
		return;
	}

	FString ValidationError;
	if (!Parsed.RaidSession.RaidSessionId.Equals(RaidSessionId, ESearchCase::CaseSensitive))
	{
		ValidationError = TEXT("Join authorization response raidSessionId does not match the requested raid.");
	}
	else if (Parsed.RaidSession.PlayerId != Parsed.PlayerId || Parsed.PlayerId <= 0)
	{
		ValidationError = TEXT("Join authorization response contains an invalid or inconsistent playerId.");
	}
	else if (Parsed.SteamId.IsEmpty())
	{
		ValidationError = TEXT("Join authorization response contains an empty steamId.");
	}
	else if (!Parsed.RaidSession.bHasServerId
		|| !Parsed.RaidSession.ServerId.Equals(Context->ServerId, ESearchCase::CaseSensitive))
	{
		ValidationError = TEXT("Join authorization response serverId does not match this raid server.");
	}
	else if (!Parsed.RaidSession.State.Equals(TEXT("ACTIVE"), ESearchCase::CaseSensitive)
		&& !Parsed.RaidSession.State.Equals(TEXT("PREPARING"), ESearchCase::CaseSensitive))
	{
		ValidationError = FString::Printf(
			TEXT("Join authorization response has a non-joinable raid state: %s"),
			*Parsed.RaidSession.State);
	}
	if (!ValidationError.IsEmpty())
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidJoinAuthorization] Authorization response validation failed. RaidSessionId=%s Error=%s"),
			*RaidSessionId,
			*ValidationError);
		FailServerFlow(RaidSessionId, ValidationError, false);
		return;
	}

	Context->PlayerId = Parsed.PlayerId;
	Context->SteamId = Parsed.SteamId;
	Context->LoadoutManifest = MoveTemp(Parsed.LoadoutManifest);
	if (AFrontierPlayerState* PlayerState = Context->PlayerState.Get())
	{
		FString LoadoutError;
		if (!ApplyJoinManifestToPlayerState(*PlayerState, Context->LoadoutManifest, LoadoutError))
		{
			FailServerFlow(RaidSessionId, LoadoutError, false);
			return;
		}

		PlayerState->SetBackendIdentity(LexToString(Context->PlayerId), Context->SteamId);
		if (!Parsed.DisplayName.IsEmpty())
		{
			PlayerState->SetPlayerName(Parsed.DisplayName);
		}
		if (UWorld* World = PlayerState->GetWorld())
		{
			if (UFrontierRaidExperienceSubsystem* Experience = World->GetSubsystem<UFrontierRaidExperienceSubsystem>())
			{
				Experience->BeginRaid(RaidSessionId);
				Experience->RegisterPlayer(PlayerState);
			}
		}
	}
	Context->State = EFrontierRaidFlowState::RaidActiveSimulated;
	FRONTIER_LOG(
		Log,
		TEXT("[RaidJoinAuthorization] Authorization and authoritative player data apply succeeded. RaidSessionId=%s PlayerId=%lld DisplayName=%s InventorySlots=%d EquipmentSlots=%d"),
		*RaidSessionId,
		Context->PlayerId,
		Parsed.DisplayName.IsEmpty() ? TEXT("<missing>") : *Parsed.DisplayName,
		Context->LoadoutManifest.InventorySlots.Num(),
		Context->LoadoutManifest.EquipmentSlots.Num());
	if (APlayerController* Controller = Context->Controller.Get())
	{
		if (AFrontierPlayerController* RaidController = Cast<AFrontierPlayerController>(Controller))
		{
			if (AFrontierGameMode* GameMode = RaidController->GetWorld()
				? RaidController->GetWorld()->GetAuthGameMode<AFrontierGameMode>()
				: nullptr)
			{
				GameMode->NotifyRaidControllerPrepared(RaidController);
			}
		}
		if (AFrontierPlayerController* RaidController = Cast<AFrontierPlayerController>(Controller))
		{
			RaidController->ClientReceiveRaidActivated(RaidSessionId);
		}
		else if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(Controller))
		{
			LobbyController->ClientReceiveSimulatedRaidActivated(RaidSessionId);
		}
	}
}

bool UFrontierRaidSessionSubsystem::TryAwardDebugExperience(
	APlayerController* Controller,
	const FString& EventId,
	FString& OutError)
{
	OutError.Reset();
#if UE_BUILD_SHIPPING
	OutError = TEXT("Manual raid experience is disabled in Shipping builds.");
	return false;
#else
	const FString* RaidSessionId = RaidIdByController.Find(Controller);
	FServerRaidContext* Context = RaidSessionId ? ServerContexts.Find(*RaidSessionId) : nullptr;
	AFrontierPlayerState* PlayerState = Context ? Context->PlayerState.Get() : nullptr;
	if (!Controller || !Controller->HasAuthority() || !Context || !PlayerState
		|| !IsDebugExperienceAllowed(Context->State))
	{
		OutError = TEXT("Manual raid experience is allowed only after successful Join Authorization.");
		return false;
	}
	UFrontierRaidExperienceSubsystem* Experience = PlayerState->GetWorld()
		? PlayerState->GetWorld()->GetSubsystem<UFrontierRaidExperienceSubsystem>()
		: nullptr;
	if (!Experience || !Experience->AwardTemporaryExperience(PlayerState, EventId, 500))
	{
		OutError = TEXT("Manual raid experience event was rejected or duplicated.");
		return false;
	}
	const FFrontierRaidPlayerExperienceState* ExperienceState = Experience->FindPlayerState(PlayerState);
	return ExperienceState != nullptr;
#endif
}

bool UFrontierRaidSessionSubsystem::BeginServerSettlement(
	APlayerController* Controller,
	const EFrontierRaidOutcome Outcome,
	FString& OutError)
{
	OutError.Reset();
	const FString* RaidSessionId = RaidIdByController.Find(Controller);
	FServerRaidContext* Context = RaidSessionId ? ServerContexts.Find(*RaidSessionId) : nullptr;
	AFrontierPlayerState* PlayerState = Context ? Context->PlayerState.Get() : nullptr;
	if (!Controller || !Controller->HasAuthority() || !Context || !PlayerState
		|| Context->State != EFrontierRaidFlowState::RaidActiveSimulated)
	{
		OutError = TEXT("Raid settlement can start only once from RaidActive.");
		return false;
	}
	if (Outcome != EFrontierRaidOutcome::Dead && Outcome != EFrontierRaidOutcome::Extracted)
	{
		OutError = TEXT("The temporary settlement supports only DEAD and EXTRACTED outcomes.");
		return false;
	}

	Context->State = EFrontierRaidFlowState::FinalizingLocalResult;
	Context->Outcome = Outcome;
	UFrontierRaidExperienceSubsystem* Experience = PlayerState->GetWorld()
		? PlayerState->GetWorld()->GetSubsystem<UFrontierRaidExperienceSubsystem>()
		: nullptr;
	if (!Experience || !Experience->FinalizePlayer(PlayerState, Outcome))
	{
		Context->State = EFrontierRaidFlowState::RaidActiveSimulated;
		OutError = TEXT("Local authoritative raid result could not be finalized.");
		return false;
	}
	const FFrontierRaidExperienceResult* FinalResult = Experience->FindFinalResult(PlayerState);
	if (!FinalResult)
	{
		Context->State = EFrontierRaidFlowState::RaidActiveSimulated;
		OutError = TEXT("Final raid experience snapshot is unavailable.");
		return false;
	}

	Context->FinalExperience = FinalResult->FinalExperience;
	Context->ServerResultId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	Context->ResultOccurredAt = FDateTime::UtcNow().ToIso8601();
	Context->ResultEventDigest = FString::Printf(
		TEXT("frontier:%s:%s:%s:%lld"),
		*Context->RaidSessionId,
		*Context->ServerResultId,
		*OutcomeToWire(Outcome),
		Context->FinalExperience);
	if (!BuildFrozenResultBody(*Context, OutError)
		|| (Context->FinalExperience > 0 && !BuildFrozenGrantBody(*Context, OutError)))
	{
		Context->State = EFrontierRaidFlowState::RetryableFailure;
		return false;
	}
	if (AFrontierPlayerController* RaidController = Cast<AFrontierPlayerController>(Controller))
	{
		RaidController->ClientReceiveRaidSettlementState(EFrontierRaidFlowState::FinalizingLocalResult, FString());
	}
	else if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(Controller))
	{
		LobbyController->ClientReceiveRaidSettlementState(EFrontierRaidFlowState::FinalizingLocalResult, FString());
	}
	SubmitServerResultCommit(*RaidSessionId);
	return true;
}

bool UFrontierRaidSessionSubsystem::BeginServerLobbyRefresh(
	APlayerController* Controller,
	const FString& RaidSessionId,
	FString& OutError)
{
	OutError.Reset();
	FServerRaidContext* Context = ServerContexts.Find(RaidSessionId);
	if (!Controller || !Controller->HasAuthority() || !Context
		|| Context->Controller.Get() != Controller
		|| Context->State != EFrontierRaidFlowState::ReturningToLobby)
	{
		OutError = TEXT("Lobby refresh does not match a committed server Raid context.");
		return false;
	}
	Context->State = EFrontierRaidFlowState::RefreshingLobby;
	return true;
}

void UFrontierRaidSessionSubsystem::CompleteServerLobbyRefresh(
	APlayerController* Controller,
	const FString& RaidSessionId)
{
	FServerRaidContext* Context = ServerContexts.Find(RaidSessionId);
	if (!Controller || !Context || Context->Controller.Get() != Controller
		|| Context->State != EFrontierRaidFlowState::RefreshingLobby)
	{
		return;
	}
	Context->State = EFrontierRaidFlowState::Completed;
	ConflictConfirmationClients.Remove(RaidSessionId);
	RaidIdByController.Remove(Controller);
	ServerContexts.Remove(RaidSessionId);
}

void UFrontierRaidSessionSubsystem::MarkServerLobbyRefreshFailed(
	APlayerController* Controller,
	const FString& RaidSessionId)
{
	if (FServerRaidContext* Context = ServerContexts.Find(RaidSessionId);
		Controller && Context && Context->Controller.Get() == Controller)
	{
		Context->State = EFrontierRaidFlowState::RetryableFailure;
	}
}

bool UFrontierRaidSessionSubsystem::BuildFrozenResultBody(
	FServerRaidContext& Context,
	FString& OutError) const
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("playerId"), static_cast<double>(Context.PlayerId));
	Body->SetStringField(TEXT("serverResultId"), Context.ServerResultId);
	Body->SetNumberField(TEXT("resultSequence"), Context.ResultSequence);
	Body->SetStringField(TEXT("eventDigest"), Context.ResultEventDigest);

	if (Context.Outcome == EFrontierRaidOutcome::Dead)
	{
		const AFrontierPlayerState* PlayerState = Context.PlayerState.Get();
		const UFrontierRaidInventoryComponent* RaidInventory = PlayerState
			? PlayerState->GetRaidInventoryComponent()
			: nullptr;
		Body->SetStringField(TEXT("outcome"), TEXT("DEAD"));
		Body->SetStringField(TEXT("reasonCode"), TEXT("HEALTH_ZERO"));
		Body->SetStringField(TEXT("occurredAt"), Context.ResultOccurredAt);
		TSharedRef<FJsonObject> LootDisposition = MakeShared<FJsonObject>();
		LootDisposition->SetStringField(
			TEXT("lootContainerId"),
			FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
		LootDisposition->SetArrayField(
			TEXT("droppedInventorySlots"),
			MakeInventorySlotsJson(Context.LoadoutManifest.InventorySlots));
		LootDisposition->SetArrayField(
			TEXT("droppedEquipmentSlots"),
			MakeEquipmentSlotsJson(Context.LoadoutManifest.EquipmentSlots));
		LootDisposition->SetArrayField(
			TEXT("consumedRaidItemIds"),
			RaidInventory
				? MakeGuidArrayJson(RaidInventory->GetConsumedRaidItemIds())
				: TArray<TSharedPtr<FJsonValue>>());
		LootDisposition->SetArrayField(TEXT("destroyedRaidItemIds"), TArray<TSharedPtr<FJsonValue>>());
		Body->SetObjectField(TEXT("lootDisposition"), LootDisposition);
	}
	else
	{
		AFrontierPlayerState* PlayerState = Context.PlayerState.Get();
		FFrontierOnlineRaidLoadoutManifestDTO CurrentManifest;
		int32 InventoryCapacity = 0;
		if (!PlayerState
			|| !BuildCurrentRaidManifest(
				*PlayerState,
				CurrentManifest,
				InventoryCapacity,
				OutError))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Current authoritative raid inventory snapshot is unavailable.");
			}
			return false;
		}

		Body->SetNumberField(TEXT("inventoryCapacity"), InventoryCapacity);
		Body->SetArrayField(
			TEXT("inventorySlots"),
			MakeInventorySlotsJson(CurrentManifest.InventorySlots));
		Body->SetArrayField(
			TEXT("equipmentSlots"),
			MakeEquipmentSlotsJson(CurrentManifest.EquipmentSlots));
		const UFrontierRaidInventoryComponent* RaidInventory = PlayerState->GetRaidInventoryComponent();
		Body->SetArrayField(
			TEXT("consumedOriginItemIds"),
			RaidInventory
				? MakeGuidArrayJson(RaidInventory->GetConsumedOriginItemIds())
				: TArray<TSharedPtr<FJsonValue>>());
		Body->SetStringField(TEXT("extractedAt"), Context.ResultOccurredAt);

		FRONTIER_LOG(
			Log,
			TEXT("[RaidExtract] Frozen current authoritative item snapshot. RaidSessionId=%s Capacity=%d InventoryItems=%d EquipmentItems=%d"),
			*Context.RaidSessionId,
			InventoryCapacity,
			CurrentManifest.InventorySlots.Num(),
			CurrentManifest.EquipmentSlots.Num());
	}

	if (!SerializeObject(Body, Context.FrozenResultBody))
	{
		OutError = TEXT("Frozen raid result body could not be serialized.");
		return false;
	}
	return true;
}

bool UFrontierRaidSessionSubsystem::BuildFrozenGrantBody(
	FServerRaidContext& Context,
	FString& OutError) const
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("sourceType"), TEXT("RAID_RESULT"));
	Body->SetStringField(TEXT("sourceId"), Context.ServerResultId);
	Body->SetNumberField(TEXT("amount"), static_cast<double>(Context.FinalExperience));
	Body->SetStringField(TEXT("reasonCode"), TEXT("RAID_EXTRACT_REWARD"));
	Body->SetStringField(TEXT("occurredAt"), Context.ResultOccurredAt);
	if (!SerializeObject(Body, Context.FrozenGrantBody))
	{
		OutError = TEXT("Frozen experience grant body could not be serialized.");
		return false;
	}
	return true;
}

void UFrontierRaidSessionSubsystem::SubmitServerResultCommit(const FString& RaidSessionId)
{
	FServerRaidContext* Context = ServerContexts.Find(RaidSessionId);
	if (!Context || !InternalApi)
	{
		return;
	}
	Context->State = EFrontierRaidFlowState::CommittingRaidResult;
	if (APlayerController* Controller = Context->Controller.Get())
	{
		if (AFrontierPlayerController* RaidController = Cast<AFrontierPlayerController>(Controller))
		{
			RaidController->ClientReceiveRaidSettlementState(EFrontierRaidFlowState::CommittingRaidResult, FString());
		}
		else if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(Controller))
		{
			LobbyController->ClientReceiveRaidSettlementState(EFrontierRaidFlowState::CommittingRaidResult, FString());
		}
	}

	FFrontierInternalApiRequest Request;
	Request.Url = Context->Outcome == EFrontierRaidOutcome::Extracted
		? InternalApi->GetConfig().BuildRaidExtractUrl(RaidSessionId)
		: InternalApi->GetConfig().BuildRaidResultCommitUrl(RaidSessionId);
	Request.Body = Context->FrozenResultBody;
	Request.RaidServerId = Context->ServerId;
	Request.bIncludeRaidServerHeader = true;
	FString QueueError;
	const TWeakObjectPtr<UFrontierRaidSessionSubsystem> WeakThis(this);
	if (!InternalApi->QueueAuthorizedRequest(
		MoveTemp(Request),
		[WeakThis, RaidSessionId](const FFrontierInternalApiResponse& Response)
		{
			if (UFrontierRaidSessionSubsystem* This = WeakThis.Get())
			{
				This->HandleServerResultCommitCompleted(RaidSessionId, Response);
			}
		},
		QueueError))
	{
		FailServerFlow(RaidSessionId, QueueError, false);
	}
}

void UFrontierRaidSessionSubsystem::HandleServerResultCommitCompleted(
	const FString& RaidSessionId,
	const FFrontierInternalApiResponse& TransportResponse)
{
	FServerRaidContext* Context = ServerContexts.Find(RaidSessionId);
	if (!Context)
	{
		return;
	}
	FFrontierOnlineRaidCommitResponse Parsed;
	FString ParseError;
	if (TransportResponse.bSucceeded)
	{
		if (!FFrontierOnlineHttpClient::ParseRaidCommitResponse(
				TransportResponse.HttpStatus,
				TransportResponse.ResponseBody,
				Context->Outcome == EFrontierRaidOutcome::Extracted,
				Parsed,
				ParseError)
			|| !DoesRaidResultConfirmCommit(
				Parsed.Result,
				RaidSessionId,
				Context->PlayerId,
				Context->Outcome))
		{
			FailServerFlow(
				RaidSessionId,
				ParseError.IsEmpty()
					? TEXT("Raid commit response did not match the authoritative settlement context.")
					: ParseError,
				false);
			return;
		}
	}
	if (!TransportResponse.bSucceeded)
	{
		if (TransportResponse.HttpStatus == 409)
		{
			ConfirmServerResultAfterConflict(RaidSessionId);
			return;
		}
		FailServerFlow(
			RaidSessionId,
			ParseError.IsEmpty() ? TransportResponse.Message : ParseError,
			TransportResponse.bRetryable);
		return;
	}
	if (Parsed.bHasLevel)
	{
		Context->bHasGrantedLevel = true;
		Context->GrantedLevel = Parsed.Level;
	}
	ContinueAfterServerResultCommit(RaidSessionId);
}

void UFrontierRaidSessionSubsystem::ConfirmServerResultAfterConflict(
	const FString& RaidSessionId,
	const bool bAfterAccessTokenRefresh)
{
	FServerRaidContext* Context = ServerContexts.Find(RaidSessionId);
	UFrontierPlayerSessionSubsystem* PlayerSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!Context || !PlayerSession)
	{
		FailServerFlow(RaidSessionId, TEXT("Player session is unavailable for 409 result confirmation."), true);
		return;
	}

	const TWeakObjectPtr<UFrontierRaidSessionSubsystem> WeakThis(this);
	PlayerSession->AcquireAccessToken(
		[WeakThis, RaidSessionId, bAfterAccessTokenRefresh](
			const bool bSucceeded,
			const FString& CurrentAccessToken,
			const FString& Error)
		{
			UFrontierRaidSessionSubsystem* This = WeakThis.Get();
			if (!This || !This->ServerContexts.Contains(RaidSessionId))
			{
				return;
			}
			if (!bSucceeded)
			{
				This->FailServerFlow(RaidSessionId, Error, true);
				return;
			}

			TSharedPtr<FFrontierOnlineHttpClient> Client =
				MakeShared<FFrontierOnlineHttpClient>(FFrontierOnlineConfig::Load());
			This->ConflictConfirmationClients.Add(RaidSessionId, Client);
			FString StartError;
			if (!Client->GetRaidResult(
				CurrentAccessToken,
				RaidSessionId,
				false,
				[WeakThis, RaidSessionId, bAfterAccessTokenRefresh](
					const FFrontierOnlineRaidResultResponse& Response)
				{
					if (UFrontierRaidSessionSubsystem* InnerThis = WeakThis.Get())
					{
						InnerThis->HandleServerResultConflictConfirmation(
							RaidSessionId,
							bAfterAccessTokenRefresh,
							Response);
					}
				},
				StartError))
			{
				This->ConflictConfirmationClients.Remove(RaidSessionId);
				This->FailServerFlow(RaidSessionId, StartError, true);
			}
		},
		bAfterAccessTokenRefresh);
}

void UFrontierRaidSessionSubsystem::HandleServerResultConflictConfirmation(
	const FString& RaidSessionId,
	const bool bAfterAccessTokenRefresh,
	const FFrontierOnlineRaidResultResponse& Response)
{
	ConflictConfirmationClients.Remove(RaidSessionId);
	FServerRaidContext* Context = ServerContexts.Find(RaidSessionId);
	if (!Context)
	{
		return;
	}
	if (!Response.bSuccess
		&& Response.HttpStatus == 401
		&& Response.ErrorCode.Equals(TEXT("ACCESS_TOKEN_EXPIRED"), ESearchCase::CaseSensitive)
		&& !bAfterAccessTokenRefresh)
	{
		ConfirmServerResultAfterConflict(RaidSessionId, true);
		return;
	}
	if (!Response.bTransportSucceeded || !Response.bSuccess
		|| !DoesRaidResultConfirmCommit(
			Response.Result,
			RaidSessionId,
			Context->PlayerId,
			Context->Outcome))
	{
		FailServerFlow(
			RaidSessionId,
			Response.Message.IsEmpty()
				? TEXT("HTTP 409 could not be confirmed as an already-committed Raid result.")
				: Response.Message,
			true);
		return;
	}
	ContinueAfterServerResultCommit(RaidSessionId);
}

void UFrontierRaidSessionSubsystem::ContinueAfterServerResultCommit(
	const FString& RaidSessionId)
{
	FServerRaidContext* Context = ServerContexts.Find(RaidSessionId);
	if (!Context)
	{
		return;
	}
	if (Context->FinalExperience <= 0
		|| InternalApi->GetConfig().bRaidResultCommitAutomaticallyGrantsExperience)
	{
		if (UFrontierRaidExperienceSubsystem* Experience = Context->PlayerState.IsValid()
			? Context->PlayerState->GetWorld()->GetSubsystem<UFrontierRaidExperienceSubsystem>()
			: nullptr)
		{
			Experience->SetFinalGrantState(Context->PlayerState.Get(), EFrontierExperienceGrantState::GrantSucceeded);
		}
		Context->State = EFrontierRaidFlowState::ReturningToLobby;
		if (APlayerController* Controller = Context->Controller.Get())
		{
			if (AFrontierPlayerController* RaidController = Cast<AFrontierPlayerController>(Controller))
			{
				RaidController->HandleRaidSettlementReadyFromBackend(
					RaidSessionId,
					Context->Outcome,
					Context->FinalExperience);
			}
			else if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(Controller))
			{
				LobbyController->ClientReceiveRaidSettlementReady(RaidSessionId);
			}
		}
		return;
	}
	SubmitExperienceGrant(RaidSessionId);
}

void UFrontierRaidSessionSubsystem::SubmitExperienceGrant(const FString& RaidSessionId)
{
	FServerRaidContext* Context = ServerContexts.Find(RaidSessionId);
	if (!Context || !InternalApi)
	{
		return;
	}
	Context->State = EFrontierRaidFlowState::GrantingExperience;
	if (UFrontierRaidExperienceSubsystem* Experience = Context->PlayerState.IsValid()
		? Context->PlayerState->GetWorld()->GetSubsystem<UFrontierRaidExperienceSubsystem>()
		: nullptr)
	{
		Experience->SetFinalGrantState(Context->PlayerState.Get(), EFrontierExperienceGrantState::GrantQueued);
	}
	if (APlayerController* Controller = Context->Controller.Get())
	{
		if (AFrontierPlayerController* RaidController = Cast<AFrontierPlayerController>(Controller))
		{
			RaidController->ClientReceiveRaidSettlementState(EFrontierRaidFlowState::GrantingExperience, FString());
		}
		else if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(Controller))
		{
			LobbyController->ClientReceiveRaidSettlementState(EFrontierRaidFlowState::GrantingExperience, FString());
		}
	}

	FFrontierInternalApiRequest Request;
	Request.Url = InternalApi->GetConfig().BuildExperienceGrantUrl(LexToString(Context->PlayerId));
	Request.Body = Context->FrozenGrantBody;
	Request.RaidServerId = Context->ServerId;
	Request.bIncludeRaidServerHeader = true;
	FString QueueError;
	const TWeakObjectPtr<UFrontierRaidSessionSubsystem> WeakThis(this);
	if (!InternalApi->QueueAuthorizedRequest(
		MoveTemp(Request),
		[WeakThis, RaidSessionId](const FFrontierInternalApiResponse& Response)
		{
			if (UFrontierRaidSessionSubsystem* This = WeakThis.Get())
			{
				This->HandleExperienceGrantCompleted(RaidSessionId, Response);
			}
		},
		QueueError))
	{
		FailServerFlow(RaidSessionId, QueueError, false);
	}
}

void UFrontierRaidSessionSubsystem::HandleExperienceGrantCompleted(
	const FString& RaidSessionId,
	const FFrontierInternalApiResponse& TransportResponse)
{
	FServerRaidContext* Context = ServerContexts.Find(RaidSessionId);
	if (!Context)
	{
		return;
	}
	FFrontierOnlineExperienceGrantResponse Parsed;
	FString ParseError;
	if (!TransportResponse.bSucceeded
		|| !FFrontierOnlineHttpClient::ParseExperienceGrantResponse(
			TransportResponse.HttpStatus,
			TransportResponse.ResponseBody,
			Parsed,
			ParseError))
	{
		if (UFrontierRaidExperienceSubsystem* Experience = Context->PlayerState.IsValid()
			? Context->PlayerState->GetWorld()->GetSubsystem<UFrontierRaidExperienceSubsystem>()
			: nullptr)
		{
			Experience->SetFinalGrantState(
				Context->PlayerState.Get(),
				TransportResponse.bRetryable
					? EFrontierExperienceGrantState::GrantFailedRetryable
					: EFrontierExperienceGrantState::GrantFailedPermanent);
		}
		FailServerFlow(
			RaidSessionId,
			ParseError.IsEmpty() ? TransportResponse.Message : ParseError,
			TransportResponse.bRetryable);
		return;
	}

	if (UFrontierRaidExperienceSubsystem* Experience = Context->PlayerState.IsValid()
		? Context->PlayerState->GetWorld()->GetSubsystem<UFrontierRaidExperienceSubsystem>()
		: nullptr)
	{
		Experience->SetFinalGrantState(Context->PlayerState.Get(), EFrontierExperienceGrantState::GrantSucceeded);
	}
	Context->bHasGrantedLevel = true;
	Context->GrantedLevel = Parsed.Level;
	Context->State = EFrontierRaidFlowState::ReturningToLobby;
	if (APlayerController* Controller = Context->Controller.Get())
	{
		if (AFrontierPlayerController* RaidController = Cast<AFrontierPlayerController>(Controller))
		{
			RaidController->HandleRaidSettlementReadyFromBackend(
				RaidSessionId,
				Context->Outcome,
				Context->FinalExperience);
		}
		else if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(Controller))
		{
			LobbyController->ClientReceiveRaidSettlementReady(RaidSessionId);
		}
	}
}

void UFrontierRaidSessionSubsystem::FailServerFlow(
	const FString& RaidSessionId,
	const FString& Error,
	const bool bRetryable)
{
	ConflictConfirmationClients.Remove(RaidSessionId);
	if (FServerRaidContext* Context = ServerContexts.Find(RaidSessionId))
	{
		const EFrontierRaidFlowState FailedState = Context->State;
		const bool bJoinAuthorizationFailure =
			FailedState == EFrontierRaidFlowState::AuthorizingJoin;
		const FString ResolvedError = Error.IsEmpty() ? TEXT("Raid server flow failed.") : Error;
		FRONTIER_LOG(
			Error,
			TEXT("[RaidServerFlow] Flow failed. RaidSessionId=%s State=%s Retryable=%d Error=%s"),
			*RaidSessionId,
			*StaticEnum<EFrontierRaidFlowState>()->GetNameStringByValue(static_cast<int64>(FailedState)),
			bRetryable ? 1 : 0,
			*ResolvedError);
		Context->State = EFrontierRaidFlowState::RetryableFailure;
		if (APlayerController* Controller = Context->Controller.Get())
		{
			if (bJoinAuthorizationFailure)
			{
				if (AFrontierPlayerController* RaidController = Cast<AFrontierPlayerController>(Controller))
				{
					if (AFrontierGameMode* GameMode = RaidController->GetWorld()
						? RaidController->GetWorld()->GetAuthGameMode<AFrontierGameMode>()
						: nullptr)
					{
						GameMode->HandleRaidJoinAuthorizationFailed(RaidController, ResolvedError);
					}
				}
			}
			else if (AFrontierPlayerController* RaidController = Cast<AFrontierPlayerController>(Controller))
			{
				RaidController->ClientReceiveRaidFlowFailed(ResolvedError, bRetryable);
			}
			else if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(Controller))
			{
				LobbyController->ClientReceiveRaidFlowFailed(ResolvedError, bRetryable);
			}
		}
	}
	else
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidServerFlow] Flow failure could not be routed because the server raid context was not found. RaidSessionId=%s Error=%s"),
			*RaidSessionId,
			Error.IsEmpty() ? TEXT("<empty>") : *Error);
	}
}
