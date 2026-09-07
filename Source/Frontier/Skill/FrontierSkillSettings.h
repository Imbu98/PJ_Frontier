#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FrontierSkillSettings.generated.h"

class UDataTable;

/** Project-wide skill template and level milestone data sources. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Frontier Skill Data"))
class FRONTIER_API UFrontierSkillSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	virtual FName GetSectionName() const override { return TEXT("Frontier Skill Data"); }

	UPROPERTY(Config, EditAnywhere, Category="Skill Data")
	TSoftObjectPtr<UDataTable> SkillDataTable;

	UPROPERTY(Config, EditAnywhere, Category="Skill Data")
	TSoftObjectPtr<UDataTable> SkillBalanceDataTable;
};
