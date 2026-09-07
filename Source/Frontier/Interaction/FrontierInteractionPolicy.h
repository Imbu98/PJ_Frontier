#pragma once

#include "CoreMinimal.h"

/** Server-authoritative rules shared by interaction runtime code and automation tests. */
struct FRONTIER_API FFrontierInteractionChannelContext
{
	bool bHasAuthority = false;
	bool bHasPendingInteraction = false;
	bool bTargetValid = false;
	bool bTargetIsExtractionZone = false;
	bool bPlayerValid = false;
	bool bPlayerDead = false;
	bool bTargetCanInteract = false;
	bool bPlayerInsideExtractionZone = false;
};

struct FRONTIER_API FFrontierInteractionPolicy
{
	static bool CanBeginChannel(const FFrontierInteractionChannelContext& Context)
	{
		return Context.bHasAuthority
			&& !Context.bHasPendingInteraction
			&& Context.bTargetValid
			&& !Context.bTargetIsExtractionZone
			&& Context.bPlayerValid
			&& !Context.bPlayerDead
			&& Context.bTargetCanInteract
			&& !Context.bPlayerInsideExtractionZone;
	}

	static bool HasExceededMovementTolerance(
		const FVector& StartLocation,
		const FVector& CurrentLocation,
		const float MovementTolerance)
	{
		return FVector::DistSquared(StartLocation, CurrentLocation)
			> FMath::Square(FMath::Max(0.0f, MovementTolerance));
	}

	static bool ShouldRetryWithAuthoritativeTarget(
		const bool bFocusedTargetStarted,
		const bool bHasPendingInteraction,
		const bool bPlayerInsideExtractionZone)
	{
		return !bFocusedTargetStarted
			&& !bHasPendingInteraction
			&& !bPlayerInsideExtractionZone;
	}
};
