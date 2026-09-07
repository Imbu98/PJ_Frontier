#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FrontierGameplayAbility.h"
#include "AbilitySystem/Abilities/FrontierIceFloorSpawnAbilityInterface.h"
#include "Combat/FrontierHitReactionTypes.h"
#include "Weapons/FrontierWeaponDataAsset.h"
#include "GA_IceFloor.generated.h"

class AFrontierIceFloorActor;
class AFrontierPlayerCharacter;
class UAnimMontage;
class UGameplayEffect;

UCLASS()
class FRONTIER_API UGA_IceFloor : public UFrontierGameplayAbility, public IFrontierIceFloorSpawnAbilityInterface
{
	GENERATED_BODY()

public:
	UGA_IceFloor();
	virtual void SpawnIceFloorFromNotify() override;

	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	void HandleCastMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void SpawnIceFloor();
	FVector ResolveFloorLocation() const;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|IceFloor")
	TObjectPtr<UAnimMontage> CastMontage;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|IceFloor")
	TSubclassOf<AFrontierIceFloorActor> IceFloorActorClass;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|IceFloor")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|IceFloor", meta=(ClampMin="0.0"))
	float BaseRadius = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|IceFloor", meta=(ClampMin="0.1"))
	float BaseDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|IceFloor", meta=(ClampMin="0.05"))
	float DamageInterval = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|IceFloor", meta=(ClampMin="0.0"))
	float BaseDamageMultiplier = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|IceFloor", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SlowMultiplier = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|IceFloor")
	EFrontierAttackAnimationMode AttackAnimationMode = EFrontierAttackAnimationMode::FullBody;

	/** Incoming reactions at or below this level do not interrupt or replace the cast montage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Skill|IceFloor")
	EFrontierHitReactionLevel HitReactionResistance = EFrontierHitReactionLevel::Stagger;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierPlayerCharacter> CachedPlayerCharacter;

	TWeakObjectPtr<AFrontierIceFloorActor> ActiveIceFloor;
	float ResolvedRadius = 0.0f;
	float ResolvedDuration = 0.0f;
	float ResolvedDamageMultiplier = 0.0f;
	float ResolvedDamageInterval = 0.0f;
	float ResolvedSlowMultiplier = 1.0f;
	bool bIceFloorSpawned = false;
};
