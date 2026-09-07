#pragma once

#include "CoreMinimal.h"
#include "FrontierRaidExperienceTypes.generated.h"

UENUM(BlueprintType)
enum class EFrontierRaidOutcome : uint8
{
	Extracted,
	Dead,
	Abandoned,
	TimedOut,
	ServerRecoveryLoss
};

UENUM(BlueprintType)
enum class EFrontierExperienceGrantState : uint8
{
	NotFinalized,
	Finalized,
	GrantQueued,
	GrantSucceeded,
	GrantFailedRetryable,
	GrantFailedPermanent
};

UENUM()
enum class EFrontierRaidExperienceCategory : uint8
{
	NormalMonsterKill,
	BossMonsterKill,
	PlayerKill,
	TeamWipe,
	ChestSearch,
	Assist,
	Survival
};

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierRaidExperienceBreakdown
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid Experience") int64 NormalMonsterKillExperience = 0;
	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid Experience") int64 BossMonsterKillExperience = 0;
	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid Experience") int64 PlayerKillExperience = 0;
	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid Experience") int64 TeamWipeExperience = 0;
	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid Experience") int64 ChestSearchExperience = 0;
	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid Experience") int64 AssistExperience = 0;
	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid Experience") int64 SurvivalExperience = 0;

	int64 GetTotal() const
	{
		return NormalMonsterKillExperience + BossMonsterKillExperience + PlayerKillExperience
			+ TeamWipeExperience + ChestSearchExperience + AssistExperience + SurvivalExperience;
	}
};

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierRaidExperienceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid Experience")
	FString RaidSessionId;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid Experience")
	FString SourceId;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid Experience")
	EFrontierRaidOutcome Outcome = EFrontierRaidOutcome::Dead;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid Experience")
	FFrontierRaidExperienceBreakdown Breakdown;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid Experience")
	int64 TotalTemporaryExperience = 0;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid Experience")
	int64 DeathReduction = 0;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid Experience")
	int64 FinalExperience = 0;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Raid Experience")
	EFrontierExperienceGrantState GrantState = EFrontierExperienceGrantState::NotFinalized;
};

/** Mutable, server-owned state for one player and one raid. */
struct FRONTIER_API FFrontierRaidPlayerExperienceState
{
	FString PlayerId;
	FString RaidSessionId;
	FString SourceId;
	FFrontierRaidExperienceBreakdown Breakdown;
	TSet<FString> ProcessedExperienceEventIds;
	double RaidStartedWorldSeconds = 0.0;
	bool bIsAfk = false;
	bool bRaidResultFinalized = false;
	bool bExperienceGrantQueued = false;
	EFrontierExperienceGrantState GrantState = EFrontierExperienceGrantState::NotFinalized;
	FFrontierRaidExperienceResult FinalResult;

	void Initialize(const FString& InPlayerId, const FString& InRaidSessionId, double InRaidStartedWorldSeconds);
	bool TryAward(const FString& EventId, EFrontierRaidExperienceCategory Category, int32 Amount);
	int64 GetTotalTemporaryExperience() const { return Breakdown.GetTotal(); }
};

struct FRONTIER_API FFrontierAssistCandidate
{
	FString PlayerId;
	int32 TeamId = 0;
	double AccumulatedValidDamage = 0.0;
	double LastDamageWorldSeconds = 0.0;
};

struct FRONTIER_API FFrontierAssistAward
{
	FString PlayerId;
	int32 Experience = 0;
};
