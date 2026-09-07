#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FrontierGameplayAbility.h"
#include "FrontierGameplayAbility_WeaponAttack.generated.h"

/** Common input surface for weapon-selected primary attack abilities. */
UCLASS(Abstract)
class FRONTIER_API UFrontierGameplayAbility_WeaponAttack : public UFrontierGameplayAbility
{
	GENERATED_BODY()

public:
	UFrontierGameplayAbility_WeaponAttack();

	virtual void HandleAttackInputPressed();
	virtual void HandleAttackInputReleased();
};
