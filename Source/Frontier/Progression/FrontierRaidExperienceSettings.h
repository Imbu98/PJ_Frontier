#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FrontierRaidExperienceSettings.generated.h"

/** Editor/configurable raid XP policy. Authoritative calculations run on the server only. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Frontier Raid Experience"))
class FRONTIER_API UFrontierRaidExperienceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Kills", meta=(ClampMin="0"))
	int32 NormalMonsterKillExperience = 40;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Kills", meta=(ClampMin="0"))
	int32 BossMonsterKillExperience = 600;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Kills", meta=(ClampMin="0"))
	int32 PlayerKillExperience = 300;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Kills", meta=(ClampMin="0"))
	int32 TeamWipeExperience = 250;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Exploration", meta=(ClampMin="0"))
	int32 ChestSearchExperience = 15;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Assists", meta=(ClampMin="0"))
	int32 AssistExperiencePool = 120;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Assists", meta=(ClampMin="0.0"))
	float AssistContributionWindowSeconds = 15.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Assists", meta=(ClampMin="0.0", ClampMax="1.0"))
	float AssistMinimumDamageRatio = 0.15f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Survival", meta=(ClampMin="0.001"))
	float SurvivalIntervalSeconds = 300.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Survival", meta=(ClampMin="0"))
	int32 SurvivalExperiencePerInterval = 25;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Survival", meta=(ClampMin="0"))
	int32 MaximumSurvivalIntervals = 4;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Settlement", meta=(ClampMin="1"))
	int32 DeathExperienceDivisor = 3;

	bool Validate(FString& OutError) const;
};
