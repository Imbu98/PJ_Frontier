#include "Progression/FrontierRaidExperienceSubsystem.h"

#include "Character/FrontierBossEnemyCharacter.h"
#include "Character/FrontierEnemyCharacter.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Frontier.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierPlayerState.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Loot/FrontierLootContainerActor.h"
#include "Progression/FrontierRaidExperienceSettings.h"

namespace
{
FString MakeRaidEventId(const TCHAR* Prefix, const FString& RaidSessionId, const UObject* Object)
{
	return FString::Printf(TEXT("%s:%s:%u"), Prefix, *RaidSessionId, Object ? Object->GetUniqueID() : 0U);
}
}

void UFrontierRaidExperienceSubsystem::BeginRaid(const FString& InRaidSessionId)
{
	if (!IsAuthorityWorld() || bRaidStarted)
	{
		return;
	}

	RaidSessionId = InRaidSessionId.IsEmpty()
		? FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower)
		: InRaidSessionId;
	RaidStartedWorldSeconds = GetWorld()->GetTimeSeconds();
	bRaidStarted = true;
	PlayerExperienceByKey.Reset();
	ParticipantKeyByPlayerState.Reset();
	PlayerStateByParticipantKey.Reset();
	DamageByVictim.Reset();
	ProcessedDamageEventIds.Reset();
}

bool UFrontierRaidExperienceSubsystem::RegisterPlayer(AFrontierPlayerState* PlayerState)
{
	if (!IsAuthorityWorld() || !bRaidStarted || !IsValid(PlayerState))
	{
		return false;
	}

	if (ParticipantKeyByPlayerState.Contains(PlayerState))
	{
		return true;
	}

	const FString ParticipantKey = ResolveParticipantKey(PlayerState);
	if (ParticipantKey.IsEmpty() || PlayerExperienceByKey.Contains(ParticipantKey))
	{
		return false;
	}

	FFrontierRaidPlayerExperienceState& State = PlayerExperienceByKey.Add(ParticipantKey);
	State.Initialize(PlayerState->GetBackendPlayerId(), RaidSessionId, RaidStartedWorldSeconds);
	ParticipantKeyByPlayerState.Add(PlayerState, ParticipantKey);
	PlayerStateByParticipantKey.Add(ParticipantKey, PlayerState);
	return true;
}

void UFrontierRaidExperienceSubsystem::RecordAppliedDamage(
	AActor* SourceActor,
	AActor* TargetActor,
	const float EffectiveDamage,
	const float TargetMaximumHealth,
	const bool bTargetDied,
	const FGuid& DamageEventId)
{
	if (!IsAuthorityWorld() || !bRaidStarted || !IsValid(TargetActor) || EffectiveDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FString DamageId = DamageEventId.IsValid()
		? DamageEventId.ToString(EGuidFormats::DigitsWithHyphensLower)
		: FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	if (!TryAcceptEventId(ProcessedDamageEventIds, DamageId))
	{
		return;
	}

	AFrontierPlayerState* Attacker = ResolvePlayerState(SourceActor);
	AFrontierPlayerState* Victim = ResolvePlayerState(TargetActor);
	if (!Attacker || !FindPlayerState(Attacker))
	{
		return;
	}

	if (Victim && FindPlayerState(Victim) && Victim != Attacker
		&& (Victim->GetTeamId() <= 0 || Attacker->GetTeamId() != Victim->GetTeamId()))
	{
		FVictimDamageContributions& Contributions = DamageByVictim.FindOrAdd(TargetActor);
		Contributions.MaximumHealth = FMath::Max(Contributions.MaximumHealth, static_cast<double>(TargetMaximumHealth));
		const FString AttackerKey = ResolveParticipantKey(Attacker);
		FFrontierAssistCandidate& Candidate = Contributions.ByAttacker.FindOrAdd(AttackerKey);
		Candidate.PlayerId = AttackerKey;
		Candidate.TeamId = Attacker->GetTeamId();
		Candidate.AccumulatedValidDamage += EffectiveDamage;
		Candidate.LastDamageWorldSeconds = GetWorld()->GetTimeSeconds();
	}

	if (bTargetDied)
	{
		ProcessTargetDeath(Attacker, TargetActor, MakeRaidEventId(TEXT("death"), RaidSessionId, TargetActor));
	}
}

void UFrontierRaidExperienceSubsystem::RecordChestSearchCompleted(
	AFrontierPlayerState* PlayerState,
	const AFrontierLootContainerActor* Container)
{
	if (!PlayerState || !Container || Container->GetSourceType() != EFrontierLootContainerSourceType::WorldLoot)
	{
		return;
	}

	FFrontierRaidPlayerExperienceState* State = PlayerExperienceByKey.Find(ResolveParticipantKey(PlayerState));
	if (State)
	{
		State->TryAward(
			MakeRaidEventId(TEXT("chest"), RaidSessionId, Container),
			EFrontierRaidExperienceCategory::ChestSearch,
			GetDefault<UFrontierRaidExperienceSettings>()->ChestSearchExperience);
	}
}

bool UFrontierRaidExperienceSubsystem::AwardTemporaryExperience(
	AFrontierPlayerState* PlayerState,
	const FString& EventId,
	const int32 Amount)
{
	if (!IsAuthorityWorld() || Amount <= 0)
	{
		return false;
	}

	FFrontierRaidPlayerExperienceState* State =
		PlayerExperienceByKey.Find(ResolveParticipantKey(PlayerState));
	return State
		&& !State->bRaidResultFinalized
		&& State->TryAward(
			EventId,
			EFrontierRaidExperienceCategory::NormalMonsterKill,
			Amount);
}

void UFrontierRaidExperienceSubsystem::MarkPlayerAfk(AFrontierPlayerState* PlayerState)
{
	if (FFrontierRaidPlayerExperienceState* State = PlayerExperienceByKey.Find(ResolveParticipantKey(PlayerState)))
	{
		if (!State->bRaidResultFinalized)
		{
			State->bIsAfk = true;
		}
	}
}

bool UFrontierRaidExperienceSubsystem::FinalizePlayer(
	AFrontierPlayerState* PlayerState,
	const EFrontierRaidOutcome Outcome)
{
	if (!IsAuthorityWorld() || !bRaidStarted || !PlayerState)
	{
		return false;
	}

	const FString ParticipantKey = ResolveParticipantKey(PlayerState);
	FFrontierRaidPlayerExperienceState* State = PlayerExperienceByKey.Find(ParticipantKey);
	if (!State || State->bRaidResultFinalized)
	{
		return false;
	}

	const UFrontierRaidExperienceSettings* Settings = GetDefault<UFrontierRaidExperienceSettings>();
	const double SurvivalSeconds = FMath::Max(0.0, GetWorld()->GetTimeSeconds() - State->RaidStartedWorldSeconds);
	const int64 SurvivalExperience = CalculateSurvivalExperience(
		SurvivalSeconds,
		State->bIsAfk,
		Settings->SurvivalIntervalSeconds,
		Settings->SurvivalExperiencePerInterval,
		Settings->MaximumSurvivalIntervals);
	State->TryAward(
		FString::Printf(TEXT("survival:%s:%s"), *RaidSessionId, *ParticipantKey),
		EFrontierRaidExperienceCategory::Survival,
		static_cast<int32>(SurvivalExperience));

	State->bRaidResultFinalized = true;
	State->GrantState = EFrontierExperienceGrantState::Finalized;
	State->FinalResult.RaidSessionId = RaidSessionId;
	State->FinalResult.SourceId = State->SourceId;
	State->FinalResult.Outcome = Outcome;
	State->FinalResult.Breakdown = State->Breakdown;
	State->FinalResult.TotalTemporaryExperience = State->GetTotalTemporaryExperience();
	State->FinalResult.GrantState = State->GrantState;

	int64 FinalExperience = 0;
	if (!CalculateFinalExperience(Outcome, State->FinalResult.TotalTemporaryExperience, Settings->DeathExperienceDivisor, FinalExperience))
	{
		// No product policy is defined for abandoned, timed-out, or recovery-loss results.
		NotifyClient(PlayerState, State->FinalResult);
		return true;
	}

	State->FinalResult.FinalExperience = FinalExperience;
	State->FinalResult.DeathReduction = State->FinalResult.TotalTemporaryExperience - FinalExperience;
	NotifyClient(PlayerState, State->FinalResult);
	return true;
}

bool UFrontierRaidExperienceSubsystem::SetFinalGrantState(
	AFrontierPlayerState* PlayerState,
	const EFrontierExperienceGrantState GrantState)
{
	FFrontierRaidPlayerExperienceState* State = PlayerExperienceByKey.Find(ResolveParticipantKey(PlayerState));
	if (!IsAuthorityWorld() || !State || !State->bRaidResultFinalized)
	{
		return false;
	}
	State->GrantState = GrantState;
	State->bExperienceGrantQueued = GrantState == EFrontierExperienceGrantState::GrantQueued
		|| State->bExperienceGrantQueued;
	State->FinalResult.GrantState = GrantState;
	NotifyClient(PlayerState, State->FinalResult);
	return true;
}

const FFrontierRaidPlayerExperienceState* UFrontierRaidExperienceSubsystem::FindPlayerState(
	const AFrontierPlayerState* PlayerState) const
{
	return PlayerExperienceByKey.Find(ResolveParticipantKey(PlayerState));
}

const FFrontierRaidExperienceResult* UFrontierRaidExperienceSubsystem::FindFinalResult(
	const AFrontierPlayerState* PlayerState) const
{
	const FFrontierRaidPlayerExperienceState* State = FindPlayerState(PlayerState);
	return State && State->bRaidResultFinalized ? &State->FinalResult : nullptr;
}

bool UFrontierRaidExperienceSubsystem::HasPendingExperienceGrants() const
{
	for (const TPair<FString, FFrontierRaidPlayerExperienceState>& Pair : PlayerExperienceByKey)
	{
		if (Pair.Value.GrantState == EFrontierExperienceGrantState::GrantQueued)
		{
			return true;
		}
	}
	return false;
}

int64 UFrontierRaidExperienceSubsystem::CalculateSurvivalExperience(
	const double SurvivalSeconds,
	const bool bIsAfk,
	const double IntervalSeconds,
	const int32 ExperiencePerInterval,
	const int32 MaximumIntervals)
{
	if (bIsAfk || SurvivalSeconds < 0.0 || IntervalSeconds <= 0.0
		|| ExperiencePerInterval < 0 || MaximumIntervals <= 0)
	{
		return 0;
	}
	const int64 CompletedIntervals = FMath::FloorToInt64(SurvivalSeconds / IntervalSeconds);
	return FMath::Min<int64>(CompletedIntervals, MaximumIntervals) * ExperiencePerInterval;
}

bool UFrontierRaidExperienceSubsystem::CalculateFinalExperience(
	const EFrontierRaidOutcome Outcome,
	const int64 TemporaryExperience,
	const int32 DeathDivisor,
	int64& OutFinalExperience)
{
	OutFinalExperience = 0;
	if (TemporaryExperience < 0 || DeathDivisor < 1)
	{
		return false;
	}
	if (Outcome == EFrontierRaidOutcome::Extracted)
	{
		OutFinalExperience = TemporaryExperience;
		return true;
	}
	if (Outcome == EFrontierRaidOutcome::Dead)
	{
		OutFinalExperience = TemporaryExperience / DeathDivisor;
		return true;
	}
	return false;
}

TArray<FFrontierAssistAward> UFrontierRaidExperienceSubsystem::DistributeAssistExperience(
	TArray<FFrontierAssistCandidate> Candidates,
	const int32 ExperiencePool)
{
	TArray<FFrontierAssistAward> Awards;
	if (Candidates.IsEmpty() || ExperiencePool <= 0)
	{
		return Awards;
	}

	Candidates.Sort([](const FFrontierAssistCandidate& Left, const FFrontierAssistCandidate& Right)
	{
		if (!FMath::IsNearlyEqual(Left.AccumulatedValidDamage, Right.AccumulatedValidDamage))
		{
			return Left.AccumulatedValidDamage > Right.AccumulatedValidDamage;
		}
		if (!FMath::IsNearlyEqual(Left.LastDamageWorldSeconds, Right.LastDamageWorldSeconds))
		{
			return Left.LastDamageWorldSeconds > Right.LastDamageWorldSeconds;
		}
		return Left.PlayerId < Right.PlayerId;
	});

	const int32 BaseExperience = ExperiencePool / Candidates.Num();
	const int32 Remainder = ExperiencePool % Candidates.Num();
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		FFrontierAssistAward& Award = Awards.AddDefaulted_GetRef();
		Award.PlayerId = Candidates[Index].PlayerId;
		Award.Experience = BaseExperience + (Index < Remainder ? 1 : 0);
	}
	return Awards;
}

TArray<FFrontierAssistCandidate> UFrontierRaidExperienceSubsystem::SelectEligibleAssistCandidates(
	const TArray<FFrontierAssistCandidate>& Candidates,
	const FString& KillerPlayerId,
	const int32 VictimTeamId,
	const double DeathWorldSeconds,
	const double VictimMaximumHealth,
	const double ContributionWindowSeconds,
	const double MinimumDamageRatio)
{
	TArray<FFrontierAssistCandidate> Eligible;
	if (DeathWorldSeconds < 0.0 || VictimMaximumHealth <= 0.0
		|| ContributionWindowSeconds < 0.0 || MinimumDamageRatio < 0.0 || MinimumDamageRatio > 1.0)
	{
		return Eligible;
	}
	const double MinimumDamage = VictimMaximumHealth * MinimumDamageRatio;
	for (const FFrontierAssistCandidate& Candidate : Candidates)
	{
		if (Candidate.PlayerId.IsEmpty() || Candidate.PlayerId == KillerPlayerId
			|| (VictimTeamId > 0 && Candidate.TeamId == VictimTeamId)
			|| Candidate.LastDamageWorldSeconds > DeathWorldSeconds
			|| DeathWorldSeconds - Candidate.LastDamageWorldSeconds > ContributionWindowSeconds
			|| Candidate.AccumulatedValidDamage + KINDA_SMALL_NUMBER < MinimumDamage)
		{
			continue;
		}
		Eligible.Add(Candidate);
	}
	return Eligible;
}

bool UFrontierRaidExperienceSubsystem::TryAcceptEventId(
	TSet<FString>& ProcessedEventIds,
	const FString& EventId)
{
	if (EventId.IsEmpty() || ProcessedEventIds.Contains(EventId))
	{
		return false;
	}
	ProcessedEventIds.Add(EventId);
	return true;
}

bool UFrontierRaidExperienceSubsystem::IsAuthorityWorld() const
{
	return GetWorld() && GetWorld()->GetNetMode() != NM_Client;
}

FString UFrontierRaidExperienceSubsystem::ResolveParticipantKey(const AFrontierPlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return FString();
	}
	if (const FString* Existing = ParticipantKeyByPlayerState.Find(PlayerState))
	{
		return *Existing;
	}
	if (!PlayerState->GetBackendPlayerId().IsEmpty())
	{
		return PlayerState->GetBackendPlayerId();
	}
	return FString::Printf(TEXT("unverified:%u"), PlayerState->GetUniqueID());
}

AFrontierPlayerState* UFrontierRaidExperienceSubsystem::ResolvePlayerState(AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}
	if (AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(Actor))
	{
		return PlayerState;
	}
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		return Pawn->GetPlayerState<AFrontierPlayerState>();
	}
	if (const AController* Controller = Cast<AController>(Actor))
	{
		return Controller->GetPlayerState<AFrontierPlayerState>();
	}
	if (AController* InstigatorController = Actor->GetInstigatorController())
	{
		return InstigatorController->GetPlayerState<AFrontierPlayerState>();
	}
	if (AActor* Owner = Actor->GetOwner())
	{
		return ResolvePlayerState(Owner);
	}
	return nullptr;
}

AFrontierPlayerState* UFrontierRaidExperienceSubsystem::FindRegisteredPlayerState(const FString& ParticipantKey) const
{
	const TWeakObjectPtr<AFrontierPlayerState>* Found = PlayerStateByParticipantKey.Find(ParticipantKey);
	return Found ? Found->Get() : nullptr;
}

bool UFrontierRaidExperienceSubsystem::IsPlayerAlive(const AFrontierPlayerState* PlayerState) const
{
	if (!PlayerState || PlayerState->IsOnlyASpectator())
	{
		return false;
	}
	const AFrontierPlayerCharacter* Character = Cast<AFrontierPlayerCharacter>(PlayerState->GetPawn());
	return Character && !Character->IsDead();
}

void UFrontierRaidExperienceSubsystem::ProcessTargetDeath(
	AFrontierPlayerState* Killer,
	AActor* TargetActor,
	const FString& DeathEventId)
{
	FFrontierRaidPlayerExperienceState* KillerState = Killer
		? PlayerExperienceByKey.Find(ResolveParticipantKey(Killer))
		: nullptr;
	if (!KillerState || KillerState->bRaidResultFinalized)
	{
		return;
	}

	if (AFrontierPlayerState* Victim = ResolvePlayerState(TargetActor))
	{
		AwardPlayerKillAndAssists(Killer, Victim, DeathEventId);
		return;
	}
	if (Cast<AFrontierEnemyCharacter>(TargetActor))
	{
		const bool bBoss = Cast<AFrontierBossEnemyCharacter>(TargetActor) != nullptr;
		KillerState->TryAward(
			DeathEventId,
			bBoss ? EFrontierRaidExperienceCategory::BossMonsterKill : EFrontierRaidExperienceCategory::NormalMonsterKill,
			bBoss
				? GetDefault<UFrontierRaidExperienceSettings>()->BossMonsterKillExperience
				: GetDefault<UFrontierRaidExperienceSettings>()->NormalMonsterKillExperience);
	}
}

void UFrontierRaidExperienceSubsystem::AwardPlayerKillAndAssists(
	AFrontierPlayerState* Killer,
	AFrontierPlayerState* Victim,
	const FString& DeathEventId)
{
	if (!Killer || !Victim || Killer == Victim
		|| (Killer->GetTeamId() > 0 && Killer->GetTeamId() == Victim->GetTeamId()))
	{
		return;
	}

	FFrontierRaidPlayerExperienceState* KillerState = PlayerExperienceByKey.Find(ResolveParticipantKey(Killer));
	if (!KillerState || !KillerState->TryAward(
		DeathEventId,
		EFrontierRaidExperienceCategory::PlayerKill,
		GetDefault<UFrontierRaidExperienceSettings>()->PlayerKillExperience))
	{
		return;
	}

	const UFrontierRaidExperienceSettings* Settings = GetDefault<UFrontierRaidExperienceSettings>();
	TArray<FFrontierAssistCandidate> EligibleCandidates;
	FVictimDamageContributions* Contributions = DamageByVictim.Find(Victim->GetPawn());
	if (Contributions)
	{
		const double DeathTime = GetWorld()->GetTimeSeconds();
		const FString KillerKey = ResolveParticipantKey(Killer);
		TArray<FFrontierAssistCandidate> RecordedCandidates;
		Contributions->ByAttacker.GenerateValueArray(RecordedCandidates);
		EligibleCandidates = SelectEligibleAssistCandidates(
			RecordedCandidates,
			KillerKey,
			Victim->GetTeamId(),
			DeathTime,
			Contributions->MaximumHealth,
			Settings->AssistContributionWindowSeconds,
			Settings->AssistMinimumDamageRatio);
		for (int32 Index = EligibleCandidates.Num() - 1; Index >= 0; --Index)
		{
			if (!FindRegisteredPlayerState(EligibleCandidates[Index].PlayerId))
			{
				EligibleCandidates.RemoveAtSwap(Index);
			}
		}
	}

	for (const FFrontierAssistAward& Award : DistributeAssistExperience(EligibleCandidates, Settings->AssistExperiencePool))
	{
		if (FFrontierRaidPlayerExperienceState* AssistantState = PlayerExperienceByKey.Find(Award.PlayerId))
		{
			AssistantState->TryAward(
				FString::Printf(TEXT("assist:%s:%s"), *DeathEventId, *Award.PlayerId),
				EFrontierRaidExperienceCategory::Assist,
				Award.Experience);
		}
	}
	DamageByVictim.Remove(Victim->GetPawn());
	TryAwardTeamWipe(Killer, Victim);
}

void UFrontierRaidExperienceSubsystem::TryAwardTeamWipe(
	AFrontierPlayerState* Killer,
	AFrontierPlayerState* Victim)
{
	if (!Killer || !Victim || Victim->GetTeamId() <= 0 || Killer->GetTeamId() == Victim->GetTeamId())
	{
		return;
	}
	for (const TPair<TWeakObjectPtr<AFrontierPlayerState>, FString>& Pair : ParticipantKeyByPlayerState)
	{
		const AFrontierPlayerState* Candidate = Pair.Key.Get();
		if (Candidate && Candidate->GetTeamId() == Victim->GetTeamId() && IsPlayerAlive(Candidate))
		{
			return;
		}
	}

	if (FFrontierRaidPlayerExperienceState* KillerState = PlayerExperienceByKey.Find(ResolveParticipantKey(Killer)))
	{
		KillerState->TryAward(
			FString::Printf(TEXT("team-wipe:%s:%d"), *RaidSessionId, Victim->GetTeamId()),
			EFrontierRaidExperienceCategory::TeamWipe,
			GetDefault<UFrontierRaidExperienceSettings>()->TeamWipeExperience);
	}
}

void UFrontierRaidExperienceSubsystem::NotifyClient(
	AFrontierPlayerState* PlayerState,
	const FFrontierRaidExperienceResult& Result) const
{
	if (AFrontierPlayerController* Controller = PlayerState ? Cast<AFrontierPlayerController>(PlayerState->GetOwner()) : nullptr)
	{
		Controller->ClientReceiveRaidExperienceResult(Result);
	}
}
