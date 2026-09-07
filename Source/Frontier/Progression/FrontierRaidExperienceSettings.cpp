#include "Progression/FrontierRaidExperienceSettings.h"

bool UFrontierRaidExperienceSettings::Validate(FString& OutError) const
{
	OutError.Reset();
	if (NormalMonsterKillExperience < 0 || BossMonsterKillExperience < 0 || PlayerKillExperience < 0
		|| TeamWipeExperience < 0 || ChestSearchExperience < 0 || AssistExperiencePool < 0
		|| SurvivalExperiencePerInterval < 0)
	{
		OutError = TEXT("Raid experience awards cannot be negative.");
		return false;
	}
	if (AssistContributionWindowSeconds < 0.0f
		|| AssistMinimumDamageRatio < 0.0f || AssistMinimumDamageRatio > 1.0f
		|| SurvivalIntervalSeconds <= 0.0f || MaximumSurvivalIntervals < 0 || DeathExperienceDivisor < 1)
	{
		OutError = TEXT("Raid experience timing, assist ratio, or divisor configuration is invalid.");
		return false;
	}
	return true;
}
