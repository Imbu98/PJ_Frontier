#include "AbilitySystem/Effects/FrontierStaminaGameplayEffect.h"

#include "AbilitySystem/FrontierAttributeSet.h"
#include "Tags/FrontierGameplayTags.h"

UFrontierStaminaGameplayEffect::UFrontierStaminaGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo StaminaModifier;
	StaminaModifier.Attribute = UFrontierAttributeSet::GetStaminaAttribute();
	StaminaModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCallerStamina;
	SetByCallerStamina.DataTag = FFrontierGameplayTags::Get().DataStaminaDelta;
	StaminaModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerStamina);
	Modifiers.Add(StaminaModifier);
}
