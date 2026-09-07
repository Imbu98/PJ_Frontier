#include "AbilitySystem/Effects/FrontierHealingGameplayEffect.h"

#include "AbilitySystem/FrontierAttributeSet.h"
#include "Tags/FrontierGameplayTags.h"

UFrontierHealingGameplayEffect::UFrontierHealingGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo HealModifier;
	HealModifier.Attribute = UFrontierAttributeSet::GetHealthAttribute();
	HealModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCallerHeal;
	SetByCallerHeal.DataTag = FFrontierGameplayTags::Get().DataHealthDelta;
	HealModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerHeal);

	Modifiers.Add(HealModifier);
}
