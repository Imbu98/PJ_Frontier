#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FrontierItemTemplateSettings.generated.h"

class UDataTable;

/** Typed item template tables used to build the runtime item catalog. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Frontier Item Templates"))
class FRONTIER_API UFrontierItemTemplateSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	virtual FName GetSectionName() const override { return TEXT("Frontier Item Templates"); }

	UPROPERTY(Config, EditAnywhere, Category="Item Template Data")
	TSoftObjectPtr<UDataTable> WeaponTemplateDataTable;

	UPROPERTY(Config, EditAnywhere, Category="Item Template Data")
	TSoftObjectPtr<UDataTable> ArmorTemplateDataTable;

	UPROPERTY(Config, EditAnywhere, Category="Item Template Data")
	TSoftObjectPtr<UDataTable> ConsumableTemplateDataTable;

	UPROPERTY(Config, EditAnywhere, Category="Item Template Data")
	TSoftObjectPtr<UDataTable> MiscellaneousTemplateDataTable;

	UPROPERTY(Config, EditAnywhere, Category="Item Template Data")
	TSoftObjectPtr<UDataTable> AccessoryTemplateDataTable;
};
