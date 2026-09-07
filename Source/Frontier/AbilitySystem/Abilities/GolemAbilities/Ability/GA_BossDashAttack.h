#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FrontierGameplayAbility.h"
#include "Warning/AttackWarningTypes.h"
#include "GA_BossDashAttack.generated.h"

class AFrontierBossEnemyCharacter;
class UAnimMontage;
class UGameplayEffect;
class UAbilityTask_PlayMontageAndWait;
class UMaterialInterface;
class UNiagaraSystem;

UCLASS()
class FRONTIER_API UGA_BossDashAttack : public UFrontierGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_BossDashAttack();

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

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Dash")
	void StartDashMoveFromNotify();

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Dash")
	void HandleLandingFromNotify();

protected:
	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageCancelled();

	UFUNCTION()
	void HandleArcMoveFinished(bool bInterrupted);

	bool ResolveImpactLocation(FVector& OutImpactLocation) const;
	FVector GetWarningLocation() const;
	void ShowAttackWarning();
	void HideAttackWarning();
	void ApplyImpactDamage();

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Boss|Dash")
	TObjectPtr<UAnimMontage> DashMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Boss|Dash", meta=(ClampMin="0.0"))
	float MaxDashDistance = 1500.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Boss|Dash", meta=(ClampMin="0.0"))
	float TargetPredictionTime = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Boss|Dash", meta=(ClampMin="0.01"))
	float DashDuration = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Boss|Dash", meta=(ClampMin="0.0"))
	float JumpHeight = 500.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Boss|Dash", meta=(ClampMin="0.0"))
	float ImpactRadius = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Boss|Dash", meta=(ClampMin="0.0"))
	float ImpactDamage = 35.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Boss|Dash")
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Boss|Dash")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Boss|Dash")
	bool bDrawDebugImpact = false;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Boss|Dash|Warning")
	TObjectPtr<UMaterialInterface> WarningDecalMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Boss|Dash|Warning")
	TObjectPtr<UNiagaraSystem> WarningNiagaraSystem = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierBossEnemyCharacter> CachedBossCharacter;

	UPROPERTY(Transient)
	FVector StartLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector ImpactLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	bool bImpactDamageApplied = false;

	UPROPERTY(Transient)
	bool bDashMoveStarted = false;

	UPROPERTY(Transient)
	FGuid ActiveWarningId;
};
