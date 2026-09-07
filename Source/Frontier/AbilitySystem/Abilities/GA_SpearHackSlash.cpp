#include "AbilitySystem/Abilities/GA_SpearHackSlash.h"

#include "Tags/FrontierGameplayTags.h"

UGA_SpearHackSlash::UGA_SpearHackSlash()
{
	RequiredWeaponTypeTag = FFrontierGameplayTags::Get().WeaponTypeSpear;
}

