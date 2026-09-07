#include "Team/FrontierTeamVisualDataAsset.h"

UMaterialInterface* UFrontierTeamVisualDataAsset::GetMaterialForTeam(const int32 TeamId) const
{
	const FFrontierTeamVisualEntry* Entry = TeamVisuals.FindByPredicate([TeamId](const FFrontierTeamVisualEntry& Candidate)
	{
		return Candidate.TeamId == TeamId;
	});

	return Entry ? Entry->Material.Get() : nullptr;
}

FLinearColor UFrontierTeamVisualDataAsset::GetColorForTeam(const int32 TeamId) const
{
	const FFrontierTeamVisualEntry* Entry = TeamVisuals.FindByPredicate([TeamId](const FFrontierTeamVisualEntry& Candidate)
	{
		return Candidate.TeamId == TeamId;
	});

	return Entry ? Entry->TeamColor : FLinearColor::White;
}
