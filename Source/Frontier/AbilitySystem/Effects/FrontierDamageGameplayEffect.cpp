#include "AbilitySystem/Effects/FrontierDamageGameplayEffect.h"

#include "AbilitySystem/FrontierAttributeSet.h"
#include "Tags/FrontierGameplayTags.h"

UFrontierDamageGameplayEffect::UFrontierDamageGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = UFrontierAttributeSet::GetDamageAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCallerDamage;
	SetByCallerDamage.DataTag = FFrontierGameplayTags::Get().DataDamage;
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDamage);

	Modifiers.Add(DamageModifier);
}
