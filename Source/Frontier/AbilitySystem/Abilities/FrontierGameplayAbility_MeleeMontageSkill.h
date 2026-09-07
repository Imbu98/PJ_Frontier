#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FrontierGameplayAbility.h"
#include "AbilitySystem/Abilities/FrontierMeleeTraceAbilityInterface.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Weapons/FrontierWeaponDataAsset.h"
#include "FrontierGameplayAbility_MeleeMontageSkill.generated.h"

class AFrontierBaseCharacter;
class AFrontierWeaponBase;
class UAnimMontage;
class UGameplayEffect;

/** Plays one melee skill montage and delegates hit timing to the existing melee trace notify state. */
UCLASS(Abstract)
class FRONTIER_API UFrontierGameplayAbility_MeleeMontageSkill : public UFrontierGameplayAbility, public IFrontierMeleeTraceAbilityInterface
{
	GENERATED_BODY()

public:
	UFrontierGameplayAbility_MeleeMontageSkill();

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

	virtual void BeginAttackTraceWindow(const FFrontierMeleeTraceOverrides& TraceOverrides) override;
	virtual void TickAttackTraceWindow(const FFrontierMeleeTraceOverrides& TraceOverrides) override;
	virtual void EndAttackTraceWindow() override;

protected:
	void FinishAbility();
	bool ResolveRuntimeData(AFrontierBaseCharacter* SourceCharacter);
	void ExecuteAttackTrace(const FFrontierMeleeTraceOverrides& TraceOverrides);
	void LockCharacterMovementIfNeeded();
	void UnlockCharacterMovementIfNeeded();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Skill|MeleeMontage")
	FFrontierAttackActionData AttackData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Skill|MeleeMontage")
	FGameplayTag RequiredWeaponTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Skill|MeleeMontage", meta=(ClampMin="0.0"))
	float MontageEndBuffer = 0.1f;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierBaseCharacter> CachedSourceCharacter;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierWeaponBase> CachedWeapon;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> LoadedAttackMontage;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> LoadedDamageEffectClass;

	UPROPERTY(Transient)
	TArray<TSubclassOf<UGameplayEffect>> LoadedAdditionalHitEffectClasses;

	EFrontierElementalType CachedWeaponElementalType = EFrontierElementalType::Normal;
	TEnumAsByte<EMovementMode> CachedMovementMode = MOVE_Walking;
	uint8 CachedCustomMovementMode = 0;
	bool bMovementLocked = false;
	FTimerHandle AbilityEndTimerHandle;
};
