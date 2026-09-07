#include "AbilitySystem/FrontierAttributeSet.h"

#include "Character/FrontierBaseCharacter.h"
#include "Tags/FrontierGameplayTags.h"
#include "Frontier.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UFrontierAttributeSet::UFrontierAttributeSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitAttackPower(10.0f);
	InitDefense(0.0f);
	InitFireAttackPower(0.0f);
	InitIceAttackPower(0.0f);
	InitLightningAttackPower(0.0f);
	InitPoisonAttackPower(0.0f);
	InitFireResistance(0.0f);
	InitIceResistance(0.0f);
	InitLightningResistance(0.0f);
	InitPoisonResistance(0.0f);
	InitSwordAttackPower(0.0f);
	InitAxeAttackPower(0.0f);
	InitSwordAttackSpeedBonus(0.0f);
	InitAxeAttackSpeedBonus(0.0f);
	InitMoveSpeedBonus(0.0f);
	InitJumpPowerBonus(0.0f);
	InitLobbyStorageSlotBonus(0.0f);
	InitRaidInventorySlotBonus(0.0f);
	InitStamina(100.0f);
	InitMaxStamina(100.0f);
	InitDamage(0.0f);
}

void UFrontierAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, Health, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, MaxHealth, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, AttackPower, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, Defense, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, FireAttackPower, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, IceAttackPower, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, LightningAttackPower, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, PoisonAttackPower, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, FireResistance, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, IceResistance, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, LightningResistance, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, PoisonResistance, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, SwordAttackPower, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, AxeAttackPower, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, SwordAttackSpeedBonus, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, AxeAttackSpeedBonus, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, MoveSpeedBonus, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, JumpPowerBonus, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, LobbyStorageSlotBonus, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, RaidInventorySlotBonus, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, Stamina, COND_Dynamic, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFrontierAttributeSet, MaxStamina, COND_Dynamic, REPNOTIFY_Always);
}

void UFrontierAttributeSet::GetReplicatedCustomConditionState(FCustomPropertyConditionState& OutActiveState) const
{
	Super::GetReplicatedCustomConditionState(OutActiveState);

	const ELifetimeCondition AttributeCondition = GetOwningActor()->IsA<APlayerState>()
		? COND_OwnerOnly
		: COND_None;
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, Health, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, MaxHealth, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, AttackPower, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, Defense, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, FireAttackPower, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, IceAttackPower, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, LightningAttackPower, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, PoisonAttackPower, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, FireResistance, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, IceResistance, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, LightningResistance, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, PoisonResistance, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, SwordAttackPower, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, AxeAttackPower, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, SwordAttackSpeedBonus, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, AxeAttackSpeedBonus, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, MoveSpeedBonus, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, JumpPowerBonus, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, LobbyStorageSlotBonus, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, RaidInventorySlotBonus, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, Stamina, AttributeCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(UFrontierAttributeSet, MaxStamina, AttributeCondition);
}

void UFrontierAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetAttackPowerAttribute()
		|| Attribute == GetDefenseAttribute()
		|| Attribute == GetFireAttackPowerAttribute()
		|| Attribute == GetIceAttackPowerAttribute()
		|| Attribute == GetLightningAttackPowerAttribute()
		|| Attribute == GetPoisonAttackPowerAttribute()
		|| Attribute == GetFireResistanceAttribute()
		|| Attribute == GetIceResistanceAttribute()
		|| Attribute == GetLightningResistanceAttribute()
		|| Attribute == GetPoisonResistanceAttribute()
		|| Attribute == GetSwordAttackPowerAttribute()
		|| Attribute == GetAxeAttackPowerAttribute()
		|| Attribute == GetSwordAttackSpeedBonusAttribute()
		|| Attribute == GetAxeAttackSpeedBonusAttribute()
		|| Attribute == GetMoveSpeedBonusAttribute()
		|| Attribute == GetJumpPowerBonusAttribute()
		|| Attribute == GetLobbyStorageSlotBonusAttribute()
		|| Attribute == GetRaidInventorySlotBonusAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void UFrontierAttributeSet::PostAttributeChange(
	const FGameplayAttribute& Attribute,
	const float OldValue,
	const float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		if (GetHealth() > NewValue)
		{
			SetHealth(NewValue);
		}
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		if (GetStamina() > NewValue)
		{
			SetStamina(NewValue);
		}
	}
}

void UFrontierAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float PendingDamage = GetDamage();
		SetDamage(0.0f);

		if (PendingDamage > 0.0f)
		{
			FGameplayTagContainer EffectAssetTags;
			Data.EffectSpec.GetAllAssetTags(EffectAssetTags);
			const FGameplayTag ElementalRootTag = FGameplayTag::RequestGameplayTag(TEXT("Elemental"), false);
			const FGameplayTag DamageTypeRootTag = FGameplayTag::RequestGameplayTag(TEXT("Damage.Type"), false);
			const FGameplayTag HitReactionRootTag = FGameplayTag::RequestGameplayTag(TEXT("HitReaction"), false);
			FGameplayTag DamageTypeTag;
			FGameplayTag HitReactionTag;
			const float RawHitReactionLevel = Data.EffectSpec.GetSetByCallerMagnitude(
				FFrontierGameplayTags::Get().DataHitReactionLevel,
				false,
				0.0f);
			const EFrontierHitReactionLevel HitReactionLevel = static_cast<EFrontierHitReactionLevel>(
				FMath::Clamp(
					FMath::RoundToInt(RawHitReactionLevel),
					static_cast<int32>(EFrontierHitReactionLevel::None),
					static_cast<int32>(EFrontierHitReactionLevel::KnockBack)));
			for (const FGameplayTag& AssetTag : EffectAssetTags)
			{
				if (DamageTypeRootTag.IsValid() && AssetTag.MatchesTag(DamageTypeRootTag))
				{
					DamageTypeTag = AssetTag;
				}
				else if (!DamageTypeTag.IsValid()
					&& ElementalRootTag.IsValid()
					&& AssetTag.MatchesTag(ElementalRootTag))
				{
					DamageTypeTag = AssetTag;
				}

				if (HitReactionRootTag.IsValid() && AssetTag.MatchesTag(HitReactionRootTag))
				{
					HitReactionTag = AssetTag;
				}
			}

			SetHealth(GetHealth() - PendingDamage);

			if (AFrontierBaseCharacter* TargetCharacter = Cast<AFrontierBaseCharacter>(Data.Target.GetAvatarActor()))
			{
				const FGameplayEffectContextHandle& EffectContext = Data.EffectSpec.GetContext();
				const FHitResult* DamageHitResult = EffectContext.GetHitResult();
				TargetCharacter->RecordLastDamageImpactPoint(
					DamageHitResult ? DamageHitResult->ImpactPoint : FVector::ZeroVector,
					DamageHitResult != nullptr);
				const AActor* DamageSource = EffectContext.GetEffectCauser();
				if (!DamageSource)
				{
					DamageSource = Cast<AActor>(EffectContext.GetSourceObject());
				}
				TargetCharacter->RecordLastDamageSource(DamageSource);
				const FVector DamageOrigin = DamageSource
					? DamageSource->GetActorLocation()
					: TargetCharacter->GetActorLocation();

				if (GetHealth() > 0.0f)
				{
					TargetCharacter->HandleDamageReceivedWithReaction(
						PendingDamage,
						DamageTypeTag,
						HitReactionTag,
						HitReactionLevel,
						Data.EffectSpec.GetSetByCallerMagnitude(FFrontierGameplayTags::Get().DataHitReactionStaggerDuration, false, 0.35f),
						Data.EffectSpec.GetSetByCallerMagnitude(FFrontierGameplayTags::Get().DataHitReactionKnockbackHorizontalStrength, false, 650.0f),
						Data.EffectSpec.GetSetByCallerMagnitude(FFrontierGameplayTags::Get().DataHitReactionKnockbackVerticalStrength, false, 150.0f),
						DamageOrigin);
				}

				if (GetHealth() <= 0.0f)
				{
					TargetCharacter->RecordLastDamageReaction(
						HitReactionTag,
						HitReactionLevel,
						Data.EffectSpec.GetSetByCallerMagnitude(FFrontierGameplayTags::Get().DataHitReactionStaggerDuration, false, 0.35f),
						Data.EffectSpec.GetSetByCallerMagnitude(FFrontierGameplayTags::Get().DataHitReactionKnockbackHorizontalStrength, false, 650.0f),
						Data.EffectSpec.GetSetByCallerMagnitude(FFrontierGameplayTags::Get().DataHitReactionKnockbackVerticalStrength, false, 150.0f),
						DamageOrigin);
					TargetCharacter->Die();
				}
			}

		}
	}

	ClampHealth();
	ClampStamina();
}

void UFrontierAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, Health, OldValue);
}

void UFrontierAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, MaxHealth, OldValue);
}

void UFrontierAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, AttackPower, OldValue);
}

void UFrontierAttributeSet::OnRep_Defense(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, Defense, OldValue);
}

void UFrontierAttributeSet::OnRep_FireAttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, FireAttackPower, OldValue);
}

void UFrontierAttributeSet::OnRep_IceAttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, IceAttackPower, OldValue);
}

void UFrontierAttributeSet::OnRep_LightningAttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, LightningAttackPower, OldValue);
}

void UFrontierAttributeSet::OnRep_PoisonAttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, PoisonAttackPower, OldValue);
}

void UFrontierAttributeSet::OnRep_FireResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, FireResistance, OldValue);
}

void UFrontierAttributeSet::OnRep_IceResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, IceResistance, OldValue);
}

void UFrontierAttributeSet::OnRep_LightningResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, LightningResistance, OldValue);
}

void UFrontierAttributeSet::OnRep_PoisonResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, PoisonResistance, OldValue);
}

void UFrontierAttributeSet::OnRep_SwordAttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, SwordAttackPower, OldValue);
}

void UFrontierAttributeSet::OnRep_AxeAttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, AxeAttackPower, OldValue);
}

void UFrontierAttributeSet::OnRep_SwordAttackSpeedBonus(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, SwordAttackSpeedBonus, OldValue);
}

void UFrontierAttributeSet::OnRep_AxeAttackSpeedBonus(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, AxeAttackSpeedBonus, OldValue);
}

void UFrontierAttributeSet::OnRep_MoveSpeedBonus(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, MoveSpeedBonus, OldValue);
}

void UFrontierAttributeSet::OnRep_JumpPowerBonus(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, JumpPowerBonus, OldValue);
}

void UFrontierAttributeSet::OnRep_LobbyStorageSlotBonus(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, LobbyStorageSlotBonus, OldValue);
}

void UFrontierAttributeSet::OnRep_RaidInventorySlotBonus(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, RaidInventorySlotBonus, OldValue);
}

void UFrontierAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, Stamina, OldValue);
}

void UFrontierAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFrontierAttributeSet, MaxStamina, OldValue);
}

void UFrontierAttributeSet::ClampHealth()
{
	SetMaxHealth(FMath::Max(GetMaxHealth(), 1.0f));
	SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
}

void UFrontierAttributeSet::ClampStamina()
{
	SetMaxStamina(FMath::Max(GetMaxStamina(), 0.0f));
	SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));
}


