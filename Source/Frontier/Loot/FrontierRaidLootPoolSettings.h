#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FrontierRaidLootPoolSettings.generated.h"

class UFrontierRaidLootPoolPolicyDataAsset;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Frontier Raid Loot Pool"))
class FRONTIER_API UFrontierRaidLootPoolSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	virtual FName GetSectionName() const override { return TEXT("Frontier Raid Loot Pool"); }

	UPROPERTY(Config, EditAnywhere, Category="Loot Pool")
	TSoftObjectPtr<UFrontierRaidLootPoolPolicyDataAsset> PolicyData;
};
