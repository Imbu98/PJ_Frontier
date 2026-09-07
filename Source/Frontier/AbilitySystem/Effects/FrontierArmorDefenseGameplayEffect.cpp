#include "AbilitySystem/Effects/FrontierArmorDefenseGameplayEffect.h"

#include "AbilitySystem/FrontierAttributeSet.h"
#include "Tags/FrontierGameplayTags.h"

UFrontierArmorDefenseGameplayEffect::UFrontierArmorDefenseGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FGameplayModifierInfo DefenseModifier;
	DefenseModifier.Attribute = UFrontierAttributeSet::GetDefenseAttribute();
	DefenseModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCallerDefense;
	SetByCallerDefense.DataTag = FFrontierGameplayTags::Get().DataDefense;
	DefenseModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDefense);

	Modifiers.Add(DefenseModifier);
}
