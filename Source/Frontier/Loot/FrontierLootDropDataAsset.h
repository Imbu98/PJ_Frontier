#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FrontierLootDropDataAsset.generated.h"

class UFrontierItemDataAsset;
class UFrontierLootItemPresetDataAsset;

USTRUCT(BlueprintType)
struct FFrontierLootTableEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot")
	TSoftObjectPtr<UFrontierItemDataAsset> ItemTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot", meta=(ToolTip="Optional authored test result. When assigned, this preset is used instead of ItemTemplate."))
	TSoftObjectPtr<UFrontierLootItemPresetDataAsset> ItemPreset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot", meta=(ClampMin="1"))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot", meta=(ClampMin="1"))
	int32 MinQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot", meta=(ClampMin="1"))
	int32 MaxQuantity = 1;
};

UCLASS(BlueprintType)
class FRONTIER_API UFrontierLootDropDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta=(ClampMin="0"))
	int32 MinLootRolls = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta=(ClampMin="0"))
	int32 MaxLootRolls = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
	TArray<FFrontierLootTableEntry> LootTable;
};
