#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FrontierUpgradeSettings.generated.h"

class UDataTable;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Frontier Upgrade Data"))
class FRONTIER_API UFrontierUpgradeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	virtual FName GetSectionName() const override { return TEXT("Frontier Upgrade Data"); }

	UPROPERTY(Config, EditAnywhere, Category="Upgrade Data")
	TSoftObjectPtr<UDataTable> LevelTableDataTable;

	UPROPERTY(Config, EditAnywhere, Category="Upgrade Data")
	TSoftObjectPtr<UDataTable> MaterialTableDataTable;

	UPROPERTY(Config, EditAnywhere, Category="Upgrade Data")
	TSoftObjectPtr<UDataTable> ItemStatDisplayDefinitionDataTable;

	UPROPERTY(Config, EditAnywhere, Category="Equipment Score")
	TSoftObjectPtr<UDataTable> EquipmentScoreEnhancementRateDataTable;
};
