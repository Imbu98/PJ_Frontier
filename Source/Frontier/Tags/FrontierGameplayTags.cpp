#include "Tags/FrontierGameplayTags.h"

#include "GameplayTagsManager.h"

namespace
{
	FFrontierGameplayTags GFrontierGameplayTags;
}

void FFrontierGameplayTags::InitializeNativeGameplayTags()
{
	if (!GFrontierGameplayTags.bInitialized)
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		GFrontierGameplayTags.AddAllTags(Manager);
		GFrontierGameplayTags.bInitialized = true;
	}
}

const FFrontierGameplayTags& FFrontierGameplayTags::Get()
{
	InitializeNativeGameplayTags();
	return GFrontierGameplayTags;
}

void FFrontierGameplayTags::AddAllTags(UGameplayTagsManager& Manager)
{
	StateAlive = Manager.AddNativeGameplayTag(TEXT("State.Alive"), TEXT("Living actor state."));
	StateDead = Manager.AddNativeGameplayTag(TEXT("State.Dead"), TEXT("Dead actor state."));
	StateActionAttacking = Manager.AddNativeGameplayTag(TEXT("State.Action.Attacking"), TEXT("Actor is performing an attack action."));
	StateCombatSkill = Manager.AddNativeGameplayTag(TEXT("State.Combat.Skill"), TEXT("Actor is performing a combat skill action."));
	StateCombatSkillDash = Manager.AddNativeGameplayTag(TEXT("State.Combat.Skill.Dash"), TEXT("Actor is performing a boss dash skill action."));
	StateCCStun = Manager.AddNativeGameplayTag(TEXT("State.CC.Stun"), TEXT("Actor is stunned and cannot act."));
	AbilityAttackPrimary = Manager.AddNativeGameplayTag(TEXT("Ability.Attack.Primary"), TEXT("Primary attack ability tag."));
	AbilityBossDashSkill = Manager.AddNativeGameplayTag(TEXT("Ability.Boss.DashSkill"), TEXT("Boss dash skill ability tag."));
	AbilityBossRangedSkill = Manager.AddNativeGameplayTag(TEXT("Ability.Boss.RangedSkill"), TEXT("Boss ranged skill ability tag."));
	SkillParameterAttackCount = Manager.AddNativeGameplayTag(TEXT("Skill.Parameter.AttackCount"), TEXT("Skill attack pulse count parameter."));
	SkillParameterRadius = Manager.AddNativeGameplayTag(TEXT("Skill.Parameter.Radius"), TEXT("Skill effect radius parameter."));
	SkillParameterTravelDistance = Manager.AddNativeGameplayTag(TEXT("Skill.Parameter.TravelDistance"), TEXT("Skill travel distance parameter."));
	SkillParameterDamageMultiplier = Manager.AddNativeGameplayTag(TEXT("Skill.Parameter.DamageMultiplier"), TEXT("Skill damage multiplier parameter."));
	SkillParameterDuration = Manager.AddNativeGameplayTag(TEXT("Skill.Parameter.Duration"), TEXT("Skill duration parameter."));
	SkillParameterDamageInterval = Manager.AddNativeGameplayTag(TEXT("Skill.Parameter.DamageInterval"), TEXT("Skill periodic damage interval parameter."));
	SkillParameterSlowMultiplier = Manager.AddNativeGameplayTag(TEXT("Skill.Parameter.SlowMultiplier"), TEXT("Skill movement speed multiplier parameter."));
	WeaponTypeUnarmed = Manager.AddNativeGameplayTag(TEXT("Weapon.Type.Unarmed"), TEXT("Default unarmed weapon type."));
	WeaponTypeSword = Manager.AddNativeGameplayTag(TEXT("Weapon.Type.Sword"), TEXT("Default sword weapon type."));
	DataHealthDelta = Manager.AddNativeGameplayTag(TEXT("Data.HealthDelta"), TEXT("SetByCaller health delta. Positive heals and negative damages."));
	DataStaminaDelta = Manager.AddNativeGameplayTag(TEXT("Data.StaminaDelta"), TEXT("SetByCaller stamina delta."));
	WeaponTypeAxe = Manager.AddNativeGameplayTag(TEXT("Weapon.Type.Axe"), TEXT("Axe weapon type."));
	WeaponTypeBow = Manager.AddNativeGameplayTag(TEXT("Weapon.Type.Bow"), TEXT("Bow weapon type."));
	WeaponTypeSpear = Manager.AddNativeGameplayTag(TEXT("Weapon.Type.Spear"), TEXT("Spear weapon type."));
	DataDamage = Manager.AddNativeGameplayTag(TEXT("Data.Damage"), TEXT("SetByCaller damage payload."));
	DataHitReactionLevel = Manager.AddNativeGameplayTag(TEXT("Data.HitReactionLevel"), TEXT("SetByCaller hit reaction strength."));
	DataHitReactionStaggerDuration = Manager.AddNativeGameplayTag(TEXT("Data.HitReaction.StaggerDuration"), TEXT("Attack-owned stagger duration."));
	DataHitReactionKnockbackHorizontalStrength = Manager.AddNativeGameplayTag(TEXT("Data.HitReaction.KnockbackHorizontalStrength"), TEXT("Attack-owned horizontal knockback strength."));
	DataHitReactionKnockbackVerticalStrength = Manager.AddNativeGameplayTag(TEXT("Data.HitReaction.KnockbackVerticalStrength"), TEXT("Attack-owned vertical knockback strength."));
	DataAttackPower = Manager.AddNativeGameplayTag(TEXT("Data.AttackPower"), TEXT("SetByCaller attack power payload."));
	DataDefense = Manager.AddNativeGameplayTag(TEXT("Data.Defense"), TEXT("SetByCaller defense payload."));
	StatAttackPower = Manager.AddNativeGameplayTag(TEXT("Stat.AttackPower"), TEXT("Item attack power stat."));
	StatDefense = Manager.AddNativeGameplayTag(TEXT("Stat.Defense"), TEXT("Item defense stat."));
	DataFireAttackPower = Manager.AddNativeGameplayTag(TEXT("Data.AttackPower.Fire"), TEXT("SetByCaller fire attack power payload."));
	DataIceAttackPower = Manager.AddNativeGameplayTag(TEXT("Data.AttackPower.Ice"), TEXT("SetByCaller ice attack power payload."));
	DataLightningAttackPower = Manager.AddNativeGameplayTag(TEXT("Data.AttackPower.Lightning"), TEXT("SetByCaller lightning attack power payload."));
	DataPoisonAttackPower = Manager.AddNativeGameplayTag(TEXT("Data.AttackPower.Poison"), TEXT("SetByCaller poison attack power payload."));
	DataFireResistance = Manager.AddNativeGameplayTag(TEXT("Data.Resistance.Fire"), TEXT("SetByCaller fire resistance payload."));
	DataIceResistance = Manager.AddNativeGameplayTag(TEXT("Data.Resistance.Ice"), TEXT("SetByCaller ice resistance payload."));
	DataLightningResistance = Manager.AddNativeGameplayTag(TEXT("Data.Resistance.Lightning"), TEXT("SetByCaller lightning resistance payload."));
	DataPoisonResistance = Manager.AddNativeGameplayTag(TEXT("Data.Resistance.Poison"), TEXT("SetByCaller poison resistance payload."));
	StatFireAttackPower = Manager.AddNativeGameplayTag(TEXT("Stat.AttackPower.Fire"), TEXT("Fire attack power stat."));
	StatIceAttackPower = Manager.AddNativeGameplayTag(TEXT("Stat.AttackPower.Ice"), TEXT("Ice attack power stat."));
	StatLightningAttackPower = Manager.AddNativeGameplayTag(TEXT("Stat.AttackPower.Lightning"), TEXT("Lightning attack power stat."));
	StatPoisonAttackPower = Manager.AddNativeGameplayTag(TEXT("Stat.AttackPower.Poison"), TEXT("Poison attack power stat."));
	StatFireResistance = Manager.AddNativeGameplayTag(TEXT("Stat.Resistance.Fire"), TEXT("Fire resistance stat."));
	StatIceResistance = Manager.AddNativeGameplayTag(TEXT("Stat.Resistance.Ice"), TEXT("Ice resistance stat."));
	StatLightningResistance = Manager.AddNativeGameplayTag(TEXT("Stat.Resistance.Lightning"), TEXT("Lightning resistance stat."));
	StatPoisonResistance = Manager.AddNativeGameplayTag(TEXT("Stat.Resistance.Poison"), TEXT("Poison resistance stat."));
	DataSwordAttackPower = Manager.AddNativeGameplayTag(TEXT("Data.AttackPower.Sword"), TEXT("SetByCaller sword attack power payload."));
	DataAxeAttackPower = Manager.AddNativeGameplayTag(TEXT("Data.AttackPower.Axe"), TEXT("SetByCaller axe attack power payload."));
	DataMaxHealthBonus = Manager.AddNativeGameplayTag(TEXT("Data.Passive.MaxHealthBonus"), TEXT("SetByCaller maximum health bonus."));
	DataMaxStaminaBonus = Manager.AddNativeGameplayTag(TEXT("Data.Passive.MaxStaminaBonus"), TEXT("SetByCaller maximum stamina bonus."));
	DataSwordAttackSpeedBonus = Manager.AddNativeGameplayTag(TEXT("Data.Passive.AttackSpeed.Sword"), TEXT("SetByCaller sword attack speed ratio bonus."));
	DataAxeAttackSpeedBonus = Manager.AddNativeGameplayTag(TEXT("Data.Passive.AttackSpeed.Axe"), TEXT("SetByCaller axe attack speed ratio bonus."));
	DataMoveSpeedBonus = Manager.AddNativeGameplayTag(TEXT("Data.Passive.MoveSpeedBonus"), TEXT("SetByCaller move speed ratio bonus."));
	DataJumpPowerBonus = Manager.AddNativeGameplayTag(TEXT("Data.Passive.JumpPowerBonus"), TEXT("SetByCaller jump power ratio bonus."));
	DataLobbyStorageSlotBonus = Manager.AddNativeGameplayTag(TEXT("Data.Passive.LobbyStorageSlotBonus"), TEXT("SetByCaller lobby storage slot bonus."));
	DataRaidInventorySlotBonus = Manager.AddNativeGameplayTag(TEXT("Data.Passive.RaidInventorySlotBonus"), TEXT("SetByCaller raid inventory slot bonus."));
	StatSwordAttackPower = Manager.AddNativeGameplayTag(TEXT("Stat.AttackPower.Sword"), TEXT("Sword-only attack power stat."));
	StatMoveSpeedBonus = Manager.AddNativeGameplayTag(TEXT("Stat.Passive.MoveSpeedBonus"), TEXT("Additive move speed ratio bonus."));
	StatJumpPowerBonus = Manager.AddNativeGameplayTag(TEXT("Stat.Passive.JumpPowerBonus"), TEXT("Additive jump power ratio bonus."));
	StatLobbyStorageSlotBonus = Manager.AddNativeGameplayTag(TEXT("Stat.Passive.LobbyStorageSlotBonus"), TEXT("Permanent lobby storage slot bonus."));
	StatRaidInventorySlotBonus = Manager.AddNativeGameplayTag(TEXT("Stat.Passive.RaidInventorySlotBonus"), TEXT("Permanent raid inventory slot bonus."));
	StatAxeAttackPower = Manager.AddNativeGameplayTag(TEXT("Stat.AttackPower.Axe"), TEXT("Axe-only attack power stat."));
	StatMaxHealthBonus = Manager.AddNativeGameplayTag(TEXT("Stat.Passive.MaxHealthBonus"), TEXT("Additive maximum health bonus."));
	StatMaxStaminaBonus = Manager.AddNativeGameplayTag(TEXT("Stat.Passive.MaxStaminaBonus"), TEXT("Additive maximum stamina bonus."));
	StatSwordAttackSpeedBonus = Manager.AddNativeGameplayTag(TEXT("Stat.Passive.AttackSpeed.Sword"), TEXT("Additive sword attack speed ratio bonus."));
	StatAxeAttackSpeedBonus = Manager.AddNativeGameplayTag(TEXT("Stat.Passive.AttackSpeed.Axe"), TEXT("Additive axe attack speed ratio bonus."));
	SkillTreeCategoryCommon = Manager.AddNativeGameplayTag(TEXT("SkillTree.Category.Common"), TEXT("Common skill tree category."));
	SkillTreeCategoryUtility = Manager.AddNativeGameplayTag(TEXT("SkillTree.Category.Utility"), TEXT("Utility skill tree category."));
	SkillTreeCategorySword = Manager.AddNativeGameplayTag(TEXT("SkillTree.Category.Weapon.Sword"), TEXT("Sword skill tree category."));
	SkillTreeCategoryAxe = Manager.AddNativeGameplayTag(TEXT("SkillTree.Category.Weapon.Axe"), TEXT("Axe skill tree category."));
	SkillTreeCategoryBow = Manager.AddNativeGameplayTag(TEXT("SkillTree.Category.Weapon.Bow"), TEXT("Bow skill tree category. Visual-only nodes are allowed."));
	SkillTreeCategorySpear = Manager.AddNativeGameplayTag(TEXT("SkillTree.Category.Weapon.Spear"), TEXT("Spear skill tree category. Visual-only nodes are allowed."));
	SkillTreeCategoryFire = Manager.AddNativeGameplayTag(TEXT("SkillTree.Category.Fire"), TEXT("Fire skill tree category."));
	SkillTreeCategoryIce = Manager.AddNativeGameplayTag(TEXT("SkillTree.Category.Ice"), TEXT("Ice skill tree category."));
	SkillTreeCategoryLightning = Manager.AddNativeGameplayTag(TEXT("SkillTree.Category.Lightning"), TEXT("Lightning skill tree category."));
	SkillTreeCategoryPoison = Manager.AddNativeGameplayTag(TEXT("SkillTree.Category.Poison"), TEXT("Poison skill tree category."));
	SkillTreeNodeCommonAttack = Manager.AddNativeGameplayTag(TEXT("SkillTree.Node.Common.AttackTraining"), TEXT("Common attack training prerequisite node."));
	SkillTreeNodeCommonDefense = Manager.AddNativeGameplayTag(TEXT("SkillTree.Node.Common.DefenseTraining"), TEXT("Common defense training node."));
	SkillTreeNodeFireResistance = Manager.AddNativeGameplayTag(TEXT("SkillTree.Node.Fire.Resistance"), TEXT("Fire resistance node."));
	SkillTreeNodeIceResistance = Manager.AddNativeGameplayTag(TEXT("SkillTree.Node.Ice.Resistance"), TEXT("Ice resistance node."));
	SkillTreeNodeLightningResistance = Manager.AddNativeGameplayTag(TEXT("SkillTree.Node.Lightning.Resistance"), TEXT("Lightning resistance node."));
	SkillTreeNodePoisonResistance = Manager.AddNativeGameplayTag(TEXT("SkillTree.Node.Poison.Resistance"), TEXT("Poison resistance node."));
	SkillTreeNodeMaxHealth = Manager.AddNativeGameplayTag(TEXT("SkillTree.Node.Vitality.MaxHealth"), TEXT("Maximum health node."));
	SkillTreeNodeMaxStamina = Manager.AddNativeGameplayTag(TEXT("SkillTree.Node.Vitality.MaxStamina"), TEXT("Maximum stamina node."));
	SkillTreeNodeSwordAttackSpeed = Manager.AddNativeGameplayTag(TEXT("SkillTree.Node.Weapon.SwordAttackSpeed"), TEXT("Sword attack speed node."));
	SkillTreeNodeAxeAttackSpeed = Manager.AddNativeGameplayTag(TEXT("SkillTree.Node.Weapon.AxeAttackSpeed"), TEXT("Axe attack speed node."));
	SkillTreeNodeMoveSpeed = Manager.AddNativeGameplayTag(TEXT("SkillTree.Node.Utility.MoveSpeed"), TEXT("Move speed node."));
	SkillTreeNodeJumpPower = Manager.AddNativeGameplayTag(TEXT("SkillTree.Node.Utility.JumpPower"), TEXT("Jump power node."));
	SkillTreeNodeLobbyStorage = Manager.AddNativeGameplayTag(TEXT("SkillTree.Node.Utility.LobbyStorage"), TEXT("Lobby storage capacity node."));
	SkillTreeNodeRaidInventory = Manager.AddNativeGameplayTag(TEXT("SkillTree.Node.Utility.RaidInventory"), TEXT("Raid inventory capacity node."));

	// Data-driven weapon/element attack nodes. They intentionally do not create GAS attributes.
	static const TCHAR* WeaponNames[] = {TEXT("Sword"), TEXT("Axe"), TEXT("Bow"), TEXT("Spear")};
	static const TCHAR* ElementNames[] = {TEXT("Normal"), TEXT("Fire"), TEXT("Ice"), TEXT("Poison"), TEXT("Lightning")};
	for (const TCHAR* WeaponName : WeaponNames)
	{
		for (const TCHAR* ElementName : ElementNames)
		{
			const FString NodeTagName = FString::Printf(
				TEXT("SkillTree.Node.Weapon.%s.%s.AttackPower"),
				WeaponName,
				ElementName);
			Manager.AddNativeGameplayTag(
				FName(*NodeTagName),
				TEXT("Weapon-family and attack-element conditional skill tree node."));
		}
	}
	DamageTypeNormal = Manager.AddNativeGameplayTag(TEXT("Damage.Type.Normal"), TEXT("Default normal damage type."));
	DamageTypePhysical = Manager.AddNativeGameplayTag(TEXT("Damage.Type.Physical"), TEXT("Physical damage type."));
	DamageTypeBlunt = Manager.AddNativeGameplayTag(TEXT("Damage.Type.Blunt"), TEXT("Blunt damage type."));
	DamageTypeSlash = Manager.AddNativeGameplayTag(TEXT("Damage.Type.Slash"), TEXT("Slash damage type."));
	DamageTypePierce = Manager.AddNativeGameplayTag(TEXT("Damage.Type.Pierce"), TEXT("Pierce damage type."));
	DamageTypeFire = Manager.AddNativeGameplayTag(TEXT("Damage.Type.Fire"), TEXT("Fire damage type."));
	DamageTypeIce = Manager.AddNativeGameplayTag(TEXT("Damage.Type.Ice"), TEXT("Ice damage type."));
	DamageTypeLightning = Manager.AddNativeGameplayTag(TEXT("Damage.Type.Lightning"), TEXT("Lightning damage type."));
	DamageTypePoison = Manager.AddNativeGameplayTag(TEXT("Damage.Type.Poison"), TEXT("Poison damage type."));
	HitReactionLight = Manager.AddNativeGameplayTag(TEXT("HitReaction.Light"), TEXT("Cosmetic light hit reaction."));
	HitReactionStagger = Manager.AddNativeGameplayTag(TEXT("HitReaction.Stagger"), TEXT("Interrupts and briefly staggers the target."));
	HitReactionKnockback = Manager.AddNativeGameplayTag(TEXT("HitReaction.Knockback"), TEXT("Launches the target away from the damage source."));
	GameplayCueHitNormal = Manager.AddNativeGameplayTag(TEXT("GameplayCue.Hit.Normal"), TEXT("Default hit gameplay cue."));
	GameplayCueHitPhysical = Manager.AddNativeGameplayTag(TEXT("GameplayCue.Hit.Physical"), TEXT("Physical hit gameplay cue."));
	GameplayCueHitBlunt = Manager.AddNativeGameplayTag(TEXT("GameplayCue.Hit.Blunt"), TEXT("Blunt hit gameplay cue."));
	GameplayCueHitSlash = Manager.AddNativeGameplayTag(TEXT("GameplayCue.Hit.Slash"), TEXT("Slash hit gameplay cue."));
	GameplayCueHitPierce = Manager.AddNativeGameplayTag(TEXT("GameplayCue.Hit.Pierce"), TEXT("Pierce hit gameplay cue."));
	GameplayCueHitFire = Manager.AddNativeGameplayTag(TEXT("GameplayCue.Hit.Fire"), TEXT("Fire hit gameplay cue."));
	GameplayCueHitIce = Manager.AddNativeGameplayTag(TEXT("GameplayCue.Hit.Ice"), TEXT("Ice hit gameplay cue."));
	GameplayCueHitLightning = Manager.AddNativeGameplayTag(TEXT("GameplayCue.Hit.Lightning"), TEXT("Lightning hit gameplay cue."));
	GameplayCueHitPoison = Manager.AddNativeGameplayTag(TEXT("GameplayCue.Hit.Poison"), TEXT("Poison hit gameplay cue."));
	ItemTypeEquipment = Manager.AddNativeGameplayTag(TEXT("Item.Type.Equipment"), TEXT("Equipment loot item type."));
	ItemTypeConsumable = Manager.AddNativeGameplayTag(TEXT("Item.Type.Consumable"), TEXT("Consumable loot item type."));
	ItemTypeMisc = Manager.AddNativeGameplayTag(TEXT("Item.Type.Misc"), TEXT("Miscellaneous loot item type."));
	ItemRarityCommon = Manager.AddNativeGameplayTag(TEXT("Item.Rarity.Common"), TEXT("Common item rarity."));
	ItemRarityRare = Manager.AddNativeGameplayTag(TEXT("Item.Rarity.Rare"), TEXT("Rare item rarity."));
	ItemRarityEpic = Manager.AddNativeGameplayTag(TEXT("Item.Rarity.Epic"), TEXT("Epic item rarity."));
	ItemRarityLegendary = Manager.AddNativeGameplayTag(TEXT("Item.Rarity.Legendary"), TEXT("Legendary item rarity."));
}


