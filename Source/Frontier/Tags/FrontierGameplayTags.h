#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UGameplayTagsManager;

struct FFrontierGameplayTags
{
public:
	static void InitializeNativeGameplayTags();
	static const FFrontierGameplayTags& Get();

	FGameplayTag StateAlive;
	FGameplayTag StateDead;
	FGameplayTag StateActionAttacking;
	FGameplayTag StateCombatSkill;
	FGameplayTag StateCombatSkillDash;
	FGameplayTag StateCCStun;
	FGameplayTag AbilityAttackPrimary;
	FGameplayTag AbilityBossDashSkill;
	FGameplayTag AbilityBossRangedSkill;
	FGameplayTag SkillParameterAttackCount;
	FGameplayTag SkillParameterRadius;
	FGameplayTag SkillParameterTravelDistance;
	FGameplayTag SkillParameterDamageMultiplier;
	FGameplayTag SkillParameterDuration;
	FGameplayTag SkillParameterDamageInterval;
	FGameplayTag SkillParameterSlowMultiplier;
	FGameplayTag WeaponTypeUnarmed;
	FGameplayTag WeaponTypeSword;
	FGameplayTag DataHealthDelta;
	FGameplayTag DataStaminaDelta;
	FGameplayTag WeaponTypeAxe;
	FGameplayTag WeaponTypeBow;
	FGameplayTag WeaponTypeSpear;
	FGameplayTag DataDamage;
	FGameplayTag DataHitReactionLevel;
	FGameplayTag DataHitReactionStaggerDuration;
	FGameplayTag DataHitReactionKnockbackHorizontalStrength;
	FGameplayTag DataHitReactionKnockbackVerticalStrength;
	FGameplayTag DataAttackPower;
	FGameplayTag DataDefense;
	FGameplayTag StatAttackPower;
	FGameplayTag StatDefense;
	FGameplayTag DataFireAttackPower;
	FGameplayTag DataIceAttackPower;
	FGameplayTag DataLightningAttackPower;
	FGameplayTag DataPoisonAttackPower;
	FGameplayTag DataFireResistance;
	FGameplayTag DataIceResistance;
	FGameplayTag DataLightningResistance;
	FGameplayTag DataPoisonResistance;
	FGameplayTag StatFireAttackPower;
	FGameplayTag StatIceAttackPower;
	FGameplayTag StatLightningAttackPower;
	FGameplayTag StatPoisonAttackPower;
	FGameplayTag StatFireResistance;
	FGameplayTag StatIceResistance;
	FGameplayTag StatLightningResistance;
	FGameplayTag StatPoisonResistance;
	FGameplayTag DataSwordAttackPower;
	FGameplayTag DataAxeAttackPower;
	FGameplayTag DataMaxHealthBonus;
	FGameplayTag DataMaxStaminaBonus;
	FGameplayTag DataSwordAttackSpeedBonus;
	FGameplayTag DataAxeAttackSpeedBonus;
	FGameplayTag DataMoveSpeedBonus;
	FGameplayTag DataJumpPowerBonus;
	FGameplayTag DataLobbyStorageSlotBonus;
	FGameplayTag DataRaidInventorySlotBonus;
	FGameplayTag StatSwordAttackPower;
	FGameplayTag StatAxeAttackPower;
	FGameplayTag StatMaxHealthBonus;
	FGameplayTag StatMaxStaminaBonus;
	FGameplayTag StatSwordAttackSpeedBonus;
	FGameplayTag StatAxeAttackSpeedBonus;
	FGameplayTag StatMoveSpeedBonus;
	FGameplayTag StatJumpPowerBonus;
	FGameplayTag StatLobbyStorageSlotBonus;
	FGameplayTag StatRaidInventorySlotBonus;
	FGameplayTag SkillTreeCategoryCommon;
	FGameplayTag SkillTreeCategoryUtility;
	FGameplayTag SkillTreeCategorySword;
	FGameplayTag SkillTreeCategoryAxe;
	FGameplayTag SkillTreeCategoryBow;
	FGameplayTag SkillTreeCategorySpear;
	FGameplayTag SkillTreeCategoryFire;
	FGameplayTag SkillTreeCategoryIce;
	FGameplayTag SkillTreeCategoryLightning;
	FGameplayTag SkillTreeCategoryPoison;
	FGameplayTag SkillTreeNodeCommonAttack;
	FGameplayTag SkillTreeNodeCommonDefense;
	FGameplayTag SkillTreeNodeFireResistance;
	FGameplayTag SkillTreeNodeIceResistance;
	FGameplayTag SkillTreeNodeLightningResistance;
	FGameplayTag SkillTreeNodePoisonResistance;
	FGameplayTag SkillTreeNodeMaxHealth;
	FGameplayTag SkillTreeNodeMaxStamina;
	FGameplayTag SkillTreeNodeSwordAttackSpeed;
	FGameplayTag SkillTreeNodeAxeAttackSpeed;
	FGameplayTag SkillTreeNodeMoveSpeed;
	FGameplayTag SkillTreeNodeJumpPower;
	FGameplayTag SkillTreeNodeLobbyStorage;
	FGameplayTag SkillTreeNodeRaidInventory;
	FGameplayTag DamageTypeNormal;
	FGameplayTag DamageTypePhysical;
	FGameplayTag DamageTypeBlunt;
	FGameplayTag DamageTypeSlash;
	FGameplayTag DamageTypePierce;
	FGameplayTag DamageTypeFire;
	FGameplayTag DamageTypeIce;
	FGameplayTag DamageTypeLightning;
	FGameplayTag DamageTypePoison;
	FGameplayTag HitReactionLight;
	FGameplayTag HitReactionStagger;
	FGameplayTag HitReactionKnockback;
	FGameplayTag GameplayCueHitNormal;
	FGameplayTag GameplayCueHitPhysical;
	FGameplayTag GameplayCueHitBlunt;
	FGameplayTag GameplayCueHitSlash;
	FGameplayTag GameplayCueHitPierce;
	FGameplayTag GameplayCueHitFire;
	FGameplayTag GameplayCueHitIce;
	FGameplayTag GameplayCueHitLightning;
	FGameplayTag GameplayCueHitPoison;
	FGameplayTag ItemTypeEquipment;
	FGameplayTag ItemTypeConsumable;
	FGameplayTag ItemTypeMisc;
	FGameplayTag ItemRarityCommon;
	FGameplayTag ItemRarityRare;
	FGameplayTag ItemRarityEpic;
	FGameplayTag ItemRarityLegendary;
	

private:
	void AddAllTags(UGameplayTagsManager& Manager);

	bool bInitialized = false;
};


