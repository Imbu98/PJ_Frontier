#include "Progression/FrontierRaidExperienceTypes.h"

void FFrontierRaidPlayerExperienceState::Initialize(
	const FString& InPlayerId,
	const FString& InRaidSessionId,
	const double InRaidStartedWorldSeconds)
{
	*this = FFrontierRaidPlayerExperienceState();
	PlayerId = InPlayerId;
	RaidSessionId = InRaidSessionId;
	RaidStartedWorldSeconds = InRaidStartedWorldSeconds;
	SourceId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	GrantState = EFrontierExperienceGrantState::NotFinalized;
}

bool FFrontierRaidPlayerExperienceState::TryAward(
	const FString& EventId,
	const EFrontierRaidExperienceCategory Category,
	const int32 Amount)
{
	if (bRaidResultFinalized || Amount < 0 || EventId.IsEmpty() || ProcessedExperienceEventIds.Contains(EventId))
	{
		return false;
	}
	ProcessedExperienceEventIds.Add(EventId);
	switch (Category)
	{
	case EFrontierRaidExperienceCategory::NormalMonsterKill: Breakdown.NormalMonsterKillExperience += Amount; break;
	case EFrontierRaidExperienceCategory::BossMonsterKill: Breakdown.BossMonsterKillExperience += Amount; break;
	case EFrontierRaidExperienceCategory::PlayerKill: Breakdown.PlayerKillExperience += Amount; break;
	case EFrontierRaidExperienceCategory::TeamWipe: Breakdown.TeamWipeExperience += Amount; break;
	case EFrontierRaidExperienceCategory::ChestSearch: Breakdown.ChestSearchExperience += Amount; break;
	case EFrontierRaidExperienceCategory::Assist: Breakdown.AssistExperience += Amount; break;
	case EFrontierRaidExperienceCategory::Survival: Breakdown.SurvivalExperience += Amount; break;
	default: return false;
	}
	return true;
}
