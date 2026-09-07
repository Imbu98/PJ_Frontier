#include "AbilitySystem/Effects/FrontierAggregatedStatsGameplayEffect.h"

#include "AbilitySystem/FrontierAttributeSet.h"
#include "Tags/FrontierGameplayTags.h"

UFrontierAggregatedStatsGameplayEffect::UFrontierAggregatedStatsGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	const auto AddSetByCallerModifier = [this](const FGameplayAttribute& Attribute, const FGameplayTag DataTag)
	{
		FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;

		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = DataTag;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	};

	AddSetByCallerModifier(UFrontierAttributeSet::GetAttackPowerAttribute(), Tags.DataAttackPower);
	AddSetByCallerModifier(UFrontierAttributeSet::GetDefenseAttribute(), Tags.DataDefense);
	AddSetByCallerModifier(UFrontierAttributeSet::GetFireAttackPowerAttribute(), Tags.DataFireAttackPower);
	AddSetByCallerModifier(UFrontierAttributeSet::GetIceAttackPowerAttribute(), Tags.DataIceAttackPower);
	AddSetByCallerModifier(UFrontierAttributeSet::GetLightningAttackPowerAttribute(), Tags.DataLightningAttackPower);
	AddSetByCallerModifier(UFrontierAttributeSet::GetPoisonAttackPowerAttribute(), Tags.DataPoisonAttackPower);
	AddSetByCallerModifier(UFrontierAttributeSet::GetFireResistanceAttribute(), Tags.DataFireResistance);
	AddSetByCallerModifier(UFrontierAttributeSet::GetIceResistanceAttribute(), Tags.DataIceResistance);
	AddSetByCallerModifier(UFrontierAttributeSet::GetLightningResistanceAttribute(), Tags.DataLightningResistance);
	AddSetByCallerModifier(UFrontierAttributeSet::GetPoisonResistanceAttribute(), Tags.DataPoisonResistance);
	AddSetByCallerModifier(UFrontierAttributeSet::GetSwordAttackPowerAttribute(), Tags.DataSwordAttackPower);
	AddSetByCallerModifier(UFrontierAttributeSet::GetAxeAttackPowerAttribute(), Tags.DataAxeAttackPower);
	AddSetByCallerModifier(UFrontierAttributeSet::GetMaxHealthAttribute(), Tags.DataMaxHealthBonus);
	AddSetByCallerModifier(UFrontierAttributeSet::GetMaxStaminaAttribute(), Tags.DataMaxStaminaBonus);
	AddSetByCallerModifier(UFrontierAttributeSet::GetSwordAttackSpeedBonusAttribute(), Tags.DataSwordAttackSpeedBonus);
	AddSetByCallerModifier(UFrontierAttributeSet::GetAxeAttackSpeedBonusAttribute(), Tags.DataAxeAttackSpeedBonus);
	AddSetByCallerModifier(UFrontierAttributeSet::GetMoveSpeedBonusAttribute(), Tags.DataMoveSpeedBonus);
	AddSetByCallerModifier(UFrontierAttributeSet::GetJumpPowerBonusAttribute(), Tags.DataJumpPowerBonus);
	AddSetByCallerModifier(UFrontierAttributeSet::GetLobbyStorageSlotBonusAttribute(), Tags.DataLobbyStorageSlotBonus);
	AddSetByCallerModifier(UFrontierAttributeSet::GetRaidInventorySlotBonusAttribute(), Tags.DataRaidInventorySlotBonus);
}
