#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FrontierGameplayAbility.h"
#include "FrontierGameplayAbility_EnemyAttack.generated.h"

class AFrontierEnemyCharacter;

UCLASS()
class FRONTIER_API UFrontierGameplayAbility_EnemyAttack : public UFrontierGameplayAbility
{
	GENERATED_BODY()

public:
	UFrontierGameplayAbility_EnemyAttack();

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

protected:
	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageCancelled();

	UPROPERTY(Transient)
	TObjectPtr<AFrontierEnemyCharacter> CachedEnemyCharacter;

	UPROPERTY(Transient)
	FTimerHandle AttackAbilityTimeoutTimerHandle;
};
