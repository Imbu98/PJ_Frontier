#include "Weapons/FrontierWeaponDataAsset.h"

#include "AbilitySystem/Abilities/FrontierGameplayAbility_PlayerAttack.h"

UFrontierWeaponDataAsset::UFrontierWeaponDataAsset()
{
	AttackAbilityClass = UFrontierGameplayAbility_PlayerAttack::StaticClass();
}
