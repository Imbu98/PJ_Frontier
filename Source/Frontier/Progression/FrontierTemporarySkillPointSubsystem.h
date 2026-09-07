#pragma once

#include "CoreMinimal.h"
#include "Progression/FrontierLevelProgressionTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FrontierTemporarySkillPointSubsystem.generated.h"

struct FFrontierLocalAccountProgression;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FFrontierTemporarySkillPointsUpdatedSignature,
	const FFrontierPlayerLevelSnapshot&,
	Level,
	const FFrontierTemporarySkillPointResult&,
	Result);

/**
 * Temporary local skill-point service used until Backend owns skill-tree progression.
 * Data is account-scoped but is not synchronized across PCs and can be lost with local save data.
 */
UCLASS()
class FRONTIER_API UFrontierTemporarySkillPointSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	using FProcessCompletion = TFunction<void(const FFrontierTemporarySkillPointResult&)>;

	void ProcessLevelSnapshot(
		const FString& AccountId,
		const FFrontierPlayerLevelSnapshot& Level,
		FProcessCompletion Completion);

	UFUNCTION(BlueprintPure, Category="Frontier|Progression")
	const FString& GetActiveAccountId() const { return ActiveAccountId; }

	UFUNCTION(BlueprintPure, Category="Frontier|Progression")
	int32 GetActiveTemporarySkillPoints() const;

	UPROPERTY(BlueprintAssignable, Category="Frontier|Progression")
	FFrontierTemporarySkillPointsUpdatedSignature OnTemporarySkillPointsUpdated;

	/** Pure transition used by the service and automation tests. */
	static FFrontierTemporarySkillPointResult EvaluateLevelSnapshot(
		FFrontierLocalAccountProgression& InOutAccount,
		const FFrontierPlayerLevelSnapshot& Level);

	/** One skill point per Backend level. Kept in one policy function for future rule changes. */
	static int32 CalculateTotalSkillPointsForLevel(int32 BackendLevel);

	static FString BuildProfileSlotName(const UWorld* World);

private:
	FString ActiveAccountId;
	TSet<FString> AccountsWithPendingSave;
};
