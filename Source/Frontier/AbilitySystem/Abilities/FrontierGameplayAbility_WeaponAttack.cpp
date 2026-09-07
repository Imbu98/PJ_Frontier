#include "AbilitySystem/Abilities/FrontierGameplayAbility_WeaponAttack.h"

#include "Tags/FrontierGameplayTags.h"

UFrontierGameplayAbility_WeaponAttack::UFrontierGameplayAbility_WeaponAttack()
{
	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.AbilityAttackPrimary);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(Tags.StateActionAttacking);
	ActivationBlockedTags.AddTag(Tags.StateDead);
	ActivationBlockedTags.AddTag(Tags.StateCCStun);
	ActivationBlockedTags.AddTag(Tags.StateActionAttacking);
}

void UFrontierGameplayAbility_WeaponAttack::HandleAttackInputPressed()
{
}

void UFrontierGameplayAbility_WeaponAttack::HandleAttackInputReleased()
{
}
