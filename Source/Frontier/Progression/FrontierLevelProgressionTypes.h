#pragma once

#include "CoreMinimal.h"
#include "FrontierLevelProgressionTypes.generated.h"

/** Client-facing snapshot of the documented PlayerLevelDTO fields only. */
USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierPlayerLevelSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	int32 Level = 0;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	int64 TotalExperience = 0;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	int64 CurrentLevelExperience = 0;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	bool bHasNextLevelRequiredExperience = false;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	int64 NextLevelRequiredExperience = 0;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	int32 MaxLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	FString UpdatedAt;

	float GetProgress() const
	{
		return bHasNextLevelRequiredExperience && NextLevelRequiredExperience > 0
			? FMath::Clamp(static_cast<float>(CurrentLevelExperience) / static_cast<float>(NextLevelRequiredExperience), 0.0f, 1.0f)
			: 1.0f;
	}

	bool IsMaxLevel() const
	{
		return Level >= MaxLevel || !bHasNextLevelRequiredExperience || NextLevelRequiredExperience <= 0;
	}
};

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierTemporarySkillPointResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	bool bWasFirstLevelQuery = false;

	/** True when an older Backend snapshot was ignored using its UpdatedAt value. */
	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	bool bWasStaleLevelSnapshot = false;

	/** True when the account-scoped local cache changed and required a disk save. */
	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	bool bProgressionChanged = false;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	int32 PreviousLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	int32 CurrentLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	int32 LevelDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	int32 GrantedTemporarySkillPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	int32 TemporarySkillPoints = 0;

	/** Backend-level-derived total point budget. This is never accepted from the local save as authority. */
	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	int32 TotalSkillPoints = 0;

	/** Cached UI/audit value. The authoritative server re-derives this from node ranks and DA costs. */
	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	int32 SpentSkillPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Progression")
	FString ErrorMessage;
};
