#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "FrontierLootItemPresetDataAsset.generated.h"

USTRUCT(BlueprintType)
struct FFrontierAuthoredLootStat
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
	FName OptionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
	FGameplayTag StatTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
	FString Unit;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
	float FinalValue = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta=(ClampMin="0.0", ClampMax="1.0"))
	float NormalizedValue = 0.0f;
};

USTRUCT(BlueprintType)
struct FFrontierAuthoredLootSkill
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta=(DisplayName="SkillId"))
	FName SkillTemplateId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
	FGameplayTag SkillTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta=(ClampMin="1"))
	int32 SkillLevel = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta=(ClampMin="0"))
	int32 SlotIndex = 0;
};

UCLASS(BlueprintType)
class FRONTIER_API UFrontierLootItemPresetDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	FFrontierItemInstance CreateTemporaryItemInstance(
		const FFrontierResolvedItemTemplateData& ResolvedItemTemplate,
		int32 Quantity) const;

	/** RowName used by UFrontierItemCatalogSubsystem across the typed item DataTables. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
	FName ItemTemplateId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
	EFrontierItemRarity FinalRarity = EFrontierItemRarity::Common;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta=(ClampMin="0"))
	int32 EnhancementLevel = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
	TArray<FString> InstanceTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta=(TitleProperty="StatTag"))
	TArray<FFrontierAuthoredLootStat> FinalStats;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta=(TitleProperty="SkillTag"))
	TArray<FFrontierAuthoredLootSkill> GeneratedSkills;
};
