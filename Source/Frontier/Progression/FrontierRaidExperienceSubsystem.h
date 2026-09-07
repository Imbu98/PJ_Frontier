#pragma once

#include "CoreMinimal.h"
#include "Progression/FrontierRaidExperienceTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "FrontierRaidExperienceSubsystem.generated.h"

class AActor;
class AFrontierLootContainerActor;
class AFrontierPlayerController;
class AFrontierPlayerState;

/** Authoritative, per-world aggregate for temporary raid experience. */
UCLASS()
class FRONTIER_API UFrontierRaidExperienceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void BeginRaid(const FString& InRaidSessionId = FString());
	bool RegisterPlayer(AFrontierPlayerState* PlayerState);

	void RecordAppliedDamage(
		AActor* SourceActor,
		AActor* TargetActor,
		float EffectiveDamage,
		float TargetMaximumHealth,
		bool bTargetDied,
		const FGuid& DamageEventId);
	void RecordChestSearchCompleted(AFrontierPlayerState* PlayerState, const AFrontierLootContainerActor* Container);
	bool AwardTemporaryExperience(
		AFrontierPlayerState* PlayerState,
		const FString& EventId,
		int32 Amount);
	void MarkPlayerAfk(AFrontierPlayerState* PlayerState);
	bool FinalizePlayer(AFrontierPlayerState* PlayerState, EFrontierRaidOutcome Outcome);
	bool SetFinalGrantState(
		AFrontierPlayerState* PlayerState,
		EFrontierExperienceGrantState GrantState);

	const FFrontierRaidPlayerExperienceState* FindPlayerState(const AFrontierPlayerState* PlayerState) const;
	const FFrontierRaidExperienceResult* FindFinalResult(const AFrontierPlayerState* PlayerState) const;
	bool HasPendingExperienceGrants() const;

	static int64 CalculateSurvivalExperience(
		double SurvivalSeconds,
		bool bIsAfk,
		double IntervalSeconds,
		int32 ExperiencePerInterval,
		int32 MaximumIntervals);
	static bool CalculateFinalExperience(
		EFrontierRaidOutcome Outcome,
		int64 TemporaryExperience,
		int32 DeathDivisor,
		int64& OutFinalExperience);
	static TArray<FFrontierAssistAward> DistributeAssistExperience(
		TArray<FFrontierAssistCandidate> Candidates,
		int32 ExperiencePool);
	static TArray<FFrontierAssistCandidate> SelectEligibleAssistCandidates(
		const TArray<FFrontierAssistCandidate>& Candidates,
		const FString& KillerPlayerId,
		int32 VictimTeamId,
		double DeathWorldSeconds,
		double VictimMaximumHealth,
		double ContributionWindowSeconds,
		double MinimumDamageRatio);
	static bool TryAcceptEventId(TSet<FString>& ProcessedEventIds, const FString& EventId);

private:
	struct FVictimDamageContributions
	{
		double MaximumHealth = 0.0;
		TMap<FString, FFrontierAssistCandidate> ByAttacker;
	};

	bool IsAuthorityWorld() const;
	FString ResolveParticipantKey(const AFrontierPlayerState* PlayerState) const;
	AFrontierPlayerState* ResolvePlayerState(AActor* Actor) const;
	AFrontierPlayerState* FindRegisteredPlayerState(const FString& ParticipantKey) const;
	bool IsPlayerAlive(const AFrontierPlayerState* PlayerState) const;
	void ProcessTargetDeath(AFrontierPlayerState* Killer, AActor* TargetActor, const FString& DeathEventId);
	void AwardPlayerKillAndAssists(AFrontierPlayerState* Killer, AFrontierPlayerState* Victim, const FString& DeathEventId);
	void TryAwardTeamWipe(AFrontierPlayerState* Killer, AFrontierPlayerState* Victim);
	void NotifyClient(AFrontierPlayerState* PlayerState, const FFrontierRaidExperienceResult& Result) const;

	FString RaidSessionId;
	double RaidStartedWorldSeconds = 0.0;
	bool bRaidStarted = false;
	TMap<FString, FFrontierRaidPlayerExperienceState> PlayerExperienceByKey;
	TMap<TWeakObjectPtr<AFrontierPlayerState>, FString> ParticipantKeyByPlayerState;
	TMap<FString, TWeakObjectPtr<AFrontierPlayerState>> PlayerStateByParticipantKey;
	TMap<TWeakObjectPtr<AActor>, FVictimDamageContributions> DamageByVictim;
	TSet<FString> ProcessedDamageEventIds;

};
