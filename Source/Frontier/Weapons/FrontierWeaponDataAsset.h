#pragma once

#include "CoreMinimal.h"
#include "Combat/FrontierHitReactionTypes.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "FrontierWeaponDataAsset.generated.h"

class USkeletalMesh;
class UStaticMesh;
class UAnimMontage;
class UAnimInstance;
class UGameplayEffect;
class UGameplayAbility;
class AFrontierArrowProjectile;

UENUM(BlueprintType)
enum class EFrontierAttackAnimationMode : uint8
{
	UpperBodyOnly UMETA(DisplayName="Upper Body Only"),
	FullBody UMETA(DisplayName="Full Body")
};

USTRUCT(BlueprintType)
struct FFrontierWeaponHitReactMontageEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|HitReact")
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|HitReact")
	TSoftObjectPtr<UAnimMontage> HitReactMontage;
};

USTRUCT(BlueprintType)
struct FFrontierAttackActionData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	TSoftObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	TSoftClassPtr<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	TArray<TSoftClassPtr<UGameplayEffect>> AdditionalHitEffectClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack", meta=(ClampMin="0.0"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack", meta=(ClampMin="0.0"))
	float AttackRange = 175.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack", meta=(ClampMin="0.0"))
	float HitRadius = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack", meta=(ClampMin="0.0"))
	float StaminaCost = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	FGameplayTag HitReactionTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	EFrontierHitReactionLevel HitReactionLevel = EFrontierHitReactionLevel::Light;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack", meta=(ClampMin="0.0"))
	float StaggerDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack", meta=(ClampMin="0.0"))
	float KnockbackHorizontalStrength = 650.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack", meta=(ClampMin="0.0"))
	float KnockbackVerticalStrength = 150.0f;

	/** Incoming reactions at or below this level are ignored while this montage plays. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	EFrontierHitReactionLevel HitReactionResistance = EFrontierHitReactionLevel::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	FName TraceStartSocketName = TEXT("startSocket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	FName TraceEndSocketName = TEXT("endSocket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	bool bDrawDebugTrace = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	bool bLockMovementDuringAttack = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack", meta=(ClampMin="0.0", ClampMax="1.0"))
	float AttackMovementSpeedMultiplier = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	EFrontierAttackAnimationMode AttackAnimationMode = EFrontierAttackAnimationMode::UpperBodyOnly;
};

USTRUCT(BlueprintType)
struct FFrontierBowAttackData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow|Animation", meta=(DisplayName="Pull Montage"))
	TSoftObjectPtr<UAnimMontage> DrawMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow|Animation")
	TSoftObjectPtr<UAnimMontage> ReleaseMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow")
	TSoftClassPtr<AFrontierArrowProjectile> ArrowProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow")
	TSoftObjectPtr<UStaticMesh> ArrowMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow|Nocked Arrow")
	FName NockedArrowCharacterSocketName = TEXT("ArrowNockSocket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow|Nocked Arrow")
	FTransform NockedArrowRelativeTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow")
	FName ArrowSpawnSocketName = TEXT("ArrowSocket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow")
	TSoftClassPtr<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow")
	TArray<TSoftClassPtr<UGameplayEffect>> AdditionalHitEffectClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow", meta=(ClampMin="0.01"))
	float FullDrawSeconds = 1.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow", meta=(ClampMin="0.0"))
	float BaseDamage = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow", meta=(ClampMin="0.0"))
	float BaseDamageMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow", meta=(ClampMin="1.0"))
	float MaximumDamageMultiplier = 1.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow", meta=(ClampMin="1.0"))
	float MaximumRangeMultiplier = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow", meta=(ClampMin="1.0"))
	float BaseProjectileSpeed = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow", meta=(ClampMin="1.0"))
	float BaseProjectileRange = 3500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow", meta=(ClampMin="0.0"))
	float ProjectileGravityScale = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow", meta=(ClampMin="0.0"))
	float StaminaCost = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow")
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow")
	FGameplayTag HitReactionTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow")
	EFrontierHitReactionLevel HitReactionLevel = EFrontierHitReactionLevel::Light;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow|HitReaction", meta=(ClampMin="0.0"))
	float StaggerDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow|HitReaction", meta=(ClampMin="0.0"))
	float KnockbackHorizontalStrength = 650.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow|HitReaction", meta=(ClampMin="0.0"))
	float KnockbackVerticalStrength = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow")
	EFrontierHitReactionLevel HitReactionResistance = EFrontierHitReactionLevel::None;

};

UCLASS(BlueprintType)
class FRONTIER_API UFrontierWeaponDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFrontierWeaponDataAsset();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	FGameplayTag WeaponTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	TSoftObjectPtr<USkeletalMesh> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	FName CharacterAttachSocketName = TEXT("WeaponSocket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	FTransform EquipOffset = FTransform::Identity;

	/**
	 * Anim Blueprint class that implements the Animation Layer Interface used by
	 * the character's main Anim Blueprint. It is linked while this weapon is active.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation")
	TSoftClassPtr<UAnimInstance> AnimLayerClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	TSoftClassPtr<UGameplayAbility> AttackAbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	TArray<FFrontierAttackActionData> BasicComboAttacks;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Bow")
	FFrontierBowAttackData BowAttack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|HitReact")
	TSoftObjectPtr<UAnimMontage> DefaultHitReactMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|HitReact")
	TArray<FFrontierWeaponHitReactMontageEntry> HitReactMontagesByDamageType;
};
