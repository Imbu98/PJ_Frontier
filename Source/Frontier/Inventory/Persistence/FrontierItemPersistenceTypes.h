#pragma once

#include "CoreMinimal.h"
#include "Inventory/FrontierInventoryTypes.h"

enum class EFrontierItemSourceType : uint8
{
	None,
	InitialGrant,
	RaidLoot,
	QuestReward,
	Shop,
	Auction,
	Craft,
	Admin
};

struct FRONTIER_API FFrontierItemOptionDTO
{
	FString OptionId;
	FString Unit;
	double BaseValue = 0.0;
	double RandomValue = 0.0;
	double UpgradeValue = 0.0;
	double FinalValue = 0.0;
	double NormalizedValue = 0.0;
	bool bHasNormalizedValue = false;
};

struct FRONTIER_API FFrontierGeneratedItemSkillDTO
{
	FString SkillId;
	FString SkillTag;
	FString Rarity;
	FString ElementalType;
	bool bGenerationFailed = false;
	int32 Level = 1;
	int32 SlotIndex = INDEX_NONE;
};

struct FRONTIER_API FFrontierItemSourceDTO
{
	EFrontierItemSourceType SourceType = EFrontierItemSourceType::None;
	TOptional<FString> SourceId;
	FString CorrelationId;
};

/**
 * C++ representation of the backend ItemDTO contract.
 * It intentionally contains no UObject reference or packaged asset path.
 */
struct FRONTIER_API FFrontierItemPersistenceDTO
{
	FString ItemInstanceId;
	FString ItemTemplateId;
	int32 Quantity = 0;
	TOptional<double> Durability;
	int32 EnhancementLevel = 0;
	FString FinalRarityTag;
	EFrontierItemBindState BindState = EFrontierItemBindState::Unbound;
	TArray<FString> InstanceTags;
	TArray<FFrontierItemOptionDTO> RandomOptions;
	TArray<FFrontierGeneratedItemSkillDTO> GeneratedSkills;
	FFrontierItemSourceDTO Source;
	FString CreatedAt;
	FString AcquiredAt;
	FString UpdatedAt;
	FString MetadataJson = TEXT("{}");
};

/**
 * Maps server-owned DTO state to the existing gameplay item without changing its public layout.
 */
struct FRONTIER_API FFrontierItemPersistenceMapper
{
	static bool TryBuildRuntimeItem(
		const FFrontierItemPersistenceDTO& DTO,
		const FFrontierResolvedItemTemplateData& ResolvedItemTemplateData,
		FFrontierItemInstance& OutItemInstance,
		FString& OutError);

	/**
	 * Updates only fields represented by the runtime item. Identity, source, timestamps,
	 * metadata remains server-owned.
	 * Persistent item options and generated skills are written to the explicit DTO arrays.
	 * This is a state bridge, not an API mutation payload builder.
	 */
	static bool TryUpdateDTOFromRuntimeItem(
		const FFrontierItemInstance& ItemInstance,
		FFrontierItemPersistenceDTO& InOutDTO,
		FString& OutError);
};
