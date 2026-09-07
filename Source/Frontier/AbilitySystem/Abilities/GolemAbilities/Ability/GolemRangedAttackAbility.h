#pragma once

#include "CoreMinimal.h"
#include "FrontierGameplayAbility.h"
#include "TimerManager.h"
#include "Warning/AttackWarningTypes.h"
#include "GolemRangedAttackAbility.generated.h"

class AFrontierBossEnemyCharacter;
class AGolemStoneProjectile;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UGameplayEffect;
class UMaterialInterface;
class UNiagaraSystem;

UCLASS()
class FRONTIER_API UGolemRangedAttackAbility : public UFrontierGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGolemRangedAttackAbility();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	UFUNCTION(BlueprintCallable, Category="Frontier|Golem|RangedAttack")
	void SpawnHeldProjectileFromNotify();

	UFUNCTION(BlueprintCallable, Category="Frontier|Golem|RangedAttack")
	void ThrowHeldProjectileFromNotify();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageCancelled();

	void HandleMontageTimedOut();
	void ShowAttackWarning(const FVector& WarningLocation, float WarningDuration);
	void HideAttackWarning();

	UPROPERTY(EditDefaultsOnly, Category = "ThrowRock")
	TObjectPtr<UAnimMontage> AttackMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "ThrowRock", meta=(ClampMin="0.0"))
	float MontageTimeoutBuffer = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "ThrowRock")
	TSubclassOf<AGolemStoneProjectile> RockProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "ThrowRock")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "ThrowRock")
	FName ThrowSocketName = TEXT("hand_rSocket");

	UPROPERTY(EditDefaultsOnly, Category = "ThrowRock")
	float ProjectileSpeed = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "ThrowRock")
	float Damage = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "ThrowRock")
	float ForwardFallbackDistance = 1000.f;

	UPROPERTY(EditDefaultsOnly, Category = "ThrowRock|Warning", meta=(ClampMin="0.0"))
	float WarningRadius = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category = "ThrowRock|Warning", meta=(ClampMin="0.0"))
	float MinimumWarningDuration = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "ThrowRock|Warning")
	TObjectPtr<UMaterialInterface> WarningDecalMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "ThrowRock|Warning")
	TObjectPtr<UNiagaraSystem> WarningNiagaraSystem = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierBossEnemyCharacter> CachedBossCharacter;

	UPROPERTY(Transient)
	TObjectPtr<AGolemStoneProjectile> HeldProjectile;

	FTimerHandle MontageTimeoutTimerHandle;
	FGuid ActiveWarningId;
	bool bWarningTransferredToProjectile = false;

	AActor* FindTargetActor() const;

	FVector GetSpawnLocation(AActor* AvatarActor) const;

	FVector GetTargetLocation(AActor* AvatarActor, AActor* TargetActor) const;
};
