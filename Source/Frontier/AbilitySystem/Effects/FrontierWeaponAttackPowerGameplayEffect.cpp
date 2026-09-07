#include "AbilitySystem/Effects/FrontierWeaponAttackPowerGameplayEffect.h"

#include "AbilitySystem/FrontierAttributeSet.h"
#include "Tags/FrontierGameplayTags.h"

UFrontierWeaponAttackPowerGameplayEffect::UFrontierWeaponAttackPowerGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FGameplayModifierInfo AttackPowerModifier;
	AttackPowerModifier.Attribute = UFrontierAttributeSet::GetAttackPowerAttribute();
	AttackPowerModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCallerAttackPower;
	SetByCallerAttackPower.DataTag = FFrontierGameplayTags::Get().DataAttackPower;
	AttackPowerModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerAttackPower);

	Modifiers.Add(AttackPowerModifier);
}
