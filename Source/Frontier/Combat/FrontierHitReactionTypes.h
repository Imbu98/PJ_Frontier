#pragma once

#include "CoreMinimal.h"
#include "FrontierHitReactionTypes.generated.h"

/** Ordered reaction strength used by both incoming hits and attack super armor. */
UENUM(BlueprintType)
enum class EFrontierHitReactionLevel : uint8
{
	None = 0,
	Light = 1,
	Stagger = 2,
	KnockBack = 3
};
