#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FrontierGameplayAbility_WeaponAttack.h"
#include "Weapons/FrontierWeaponDataAsset.h"
#include "FrontierGameplayAbility_BowAttack.generated.h"

class AFrontierPlayerCharacter;
class AFrontierBowWeaponBase;
class UAnimMontage;

UCLASS()
class FRONTIER_API UFrontierGameplayAbility_BowAttack : public UFrontierGameplayAbility_WeaponAttack
{
	GENERATED_BODY()

public:
	UFrontierGameplayAbility_BowAttack();

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

	virtual void HandleAttackInputReleased() override;

protected:
	UFUNCTION()
	void HandleDrawCompleted();

	UFUNCTION()
	void HandleMontageCancelled();

	UFUNCTION()
	void HandleReleaseCompleted();

	void PlayDrawMontage();
	void PlayReleaseMontage();
	void UpdateChargeWidget();
	void ReleaseArrow();
	bool ResolveBowData(AFrontierPlayerCharacter* PlayerCharacter);
	float GetChargeProgress() const;
	FVector ResolveAimDirection(const FVector& SpawnLocation) const;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierPlayerCharacter> CachedPlayerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierBowWeaponBase> CachedWeapon;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> DrawMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ReleaseMontage;

	FFrontierBowAttackData BowData;
	FTimerHandle ChargeWidgetTimerHandle;
	float DrawStartedAtSeconds = 0.0f;
	bool bArrowReleased = false;
};
