#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FrontierGameplayAbility.h"
#include "Combat/FrontierHitReactionTypes.h"
#include "GA_ThrowAxe.generated.h"

class AFrontierPlayerCharacter;
class AFrontierThrownAxeProjectile;
class UGameplayEffect;
class UMaterialInterface;
class UAnimMontage;
class UNiagaraSystem;
class USkeletalMesh;

UCLASS()
class FRONTIER_API UGA_ThrowAxe : public UFrontierGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_ThrowAxe();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	void ThrowAxeFromNotify();

protected:
	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe")
	TObjectPtr<UAnimMontage> ThrowMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe")
	TSubclassOf<AFrontierThrownAxeProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe", meta=(ClampMin="0.0"))
	float ProjectileSpeed = 1800.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe", meta=(ClampMin="0.0"))
	float SpawnForwardOffset = 120.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe")
	float SpawnHeightOffset = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe", meta=(ClampMin="0.0"))
	float BaseDamage = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe", meta=(ClampMin="0.0"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe")
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe")
	EFrontierElementalType ElementalType = EFrontierElementalType::Normal;

	/** Incoming reactions at or below this level do not interrupt or replace the throw montage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Skill|ThrowAxe")
	EFrontierHitReactionLevel HitReactionResistance = EFrontierHitReactionLevel::Stagger;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe")
	bool bRequireAxeWeapon = true;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe", meta=(ClampMin="0.0"))
	float WeaponReappearDelay = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe|Trajectory")
	TObjectPtr<UNiagaraSystem> TrajectoryNiagaraSystem = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe|Trajectory", meta=(ClampMin="0.0"))
	float TrajectoryPreviewDistance = 1200.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe|Trajectory", meta=(ClampMin="2", ClampMax="64"))
	int32 TrajectoryPointCount = 8;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe|Trajectory", meta=(ClampMin="0.0"))
	float TrajectoryPreviewDuration = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe|Trajectory", meta=(ClampMin="0.01"))
	float TrajectoryUpdateInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|ThrowAxe|Trajectory", meta=(ClampMin="0.0"))
	float TrajectoryStartForwardOffset = 50.0f;

	bool ResolveThrowContext(
		const FGameplayAbilityActorInfo* ActorInfo,
		AFrontierPlayerCharacter*& OutPlayerCharacter,
		USkeletalMesh*& OutWeaponMesh,
		TArray<UMaterialInterface*>& OutWeaponMaterials,
		FGameplayTag& OutWeaponTypeTag) const;

	void RestoreHiddenWeapon();
	void FinishThrowAbility();
	void ShowTrajectoryPreview(AFrontierPlayerCharacter* PlayerCharacter);
	void UpdateTrajectoryPreview();
	void HideTrajectoryPreview();
	void BuildThrowTrajectoryPoints(const AFrontierPlayerCharacter* PlayerCharacter, TArray<FVector>& OutPoints) const;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierPlayerCharacter> CachedPlayerCharacter = nullptr;

	FTimerHandle RestoreWeaponTimerHandle;
	FTimerHandle FinishAbilityTimerHandle;
	FTimerHandle TrajectoryUpdateTimerHandle;
	FGuid ActiveTrajectoryId;
	bool bProjectileThrown = false;
	bool bWeaponHiddenForThrow = false;
};
