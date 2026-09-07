#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FrontierGameplayAbility_AreaSkill.h"
#include "Combat/FrontierHitReactionTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Warning/AttackWarningTypes.h"
#include "GA_BurningSlam.generated.h"

class AFrontierPlayerCharacter;
class UAnimMontage;
class UGameplayEffect;
class UMaterialInterface;
class UNiagaraSystem;

UCLASS()
class FRONTIER_API UGA_BurningSlam : public UFrontierGameplayAbility_AreaSkill
{
	GENERATED_BODY()

public:
	UGA_BurningSlam();

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

	virtual bool CanConfirmAreaSkill() override;
	virtual void OnAreaSkillConfirmed() override;
	virtual void OnAreaSkillCancelled() override;
	virtual void AdjustAreaTargetDistanceInput(float InputAxis) override;

	bool ResolveTargetLocation(FVector& OutActorLocation, FVector& OutGroundLocation) const;
	void ShowTargetWarning();
	void UpdateTargetWarning();
	void HideTargetWarning();
	void ShowOrUpdateTrajectoryPreview();
	void HideTrajectoryPreview();
	void BuildSlamTrajectoryPoints(TArray<FVector>& OutPoints) const;
	void TickTargeting();
	bool RefreshTargetLocation();
	void StartSlamMovement();
	void TickSlamMovement();
	void FinishSlamMovement(bool bInterrupted);
	FVector EvaluateSlamMovementLocation(const FVector& PathStart, const FVector& PathTarget, float Alpha) const;
	void ApplyImpactDamage();
	void ApplySlamCollisionOverrides();
	void RestoreSlamCollisionOverrides();
	void RestoreMovementMode();
	void RestoreAnimationMode();

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam")
	TObjectPtr<UAnimMontage> SlamMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam", meta=(ClampMin="0.0"))
	float TargetDistance = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam", meta=(ClampMin="0.0"))
	float MinTargetDistance = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam", meta=(ClampMin="0.0"))
	float MaxTargetDistance = 900.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam")
	float TargetDistanceInputScale = 40.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam", meta=(ClampMin="0.01"))
	float TargetingUpdateInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam", meta=(ClampMin="0.0"))
	float ImpactRadius = 220.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam|Targeting")
	bool bCanOver = false;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam", meta=(ClampMin="0.0"))
	float BaseDamage = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam", meta=(ClampMin="0.0"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam", meta=(ClampMin="0.01"))
	float SlamMoveDuration = 0.45f;

	/** Fraction of the movement duration used to reach the target horizontally. */
	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam|Movement", meta=(ClampMin="0.1", ClampMax="1.0"))
	float HorizontalTravelEndFraction = 0.7f;

	/** Fraction of the movement duration at which the downward slam begins. */
	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam|Movement", meta=(ClampMin="0.1", ClampMax="0.9"))
	float SlamApexFraction = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam", meta=(ClampMin="0.0"))
	float JumpHeight = 160.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam", meta=(ClampMin="0.01"))
	float MovementTickInterval = 0.016f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam", meta=(ClampMin="0.0"))
	float ConfirmTimeout = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam")
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam|HitReaction")
	FGameplayTag HitReactionTag;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam|HitReaction")
	EFrontierHitReactionLevel HitReactionLevel = EFrontierHitReactionLevel::KnockBack;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam|HitReaction", meta=(ClampMin="0.0"))
	float StaggerDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam|HitReaction", meta=(ClampMin="0.0"))
	float KnockbackHorizontalStrength = 650.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam|HitReaction", meta=(ClampMin="0.0"))
	float KnockbackVerticalStrength = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam|HitReaction")
	EFrontierHitReactionLevel HitReactionResistance = EFrontierHitReactionLevel::Stagger;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam")
	EFrontierElementalType ElementalType = EFrontierElementalType::Fire;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam")
	bool bDrawDebugImpact = false;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam|Warning")
	TObjectPtr<UMaterialInterface> WarningDecalMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam|Warning")
	TObjectPtr<UNiagaraSystem> WarningNiagaraSystem = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam|Trajectory")
	TObjectPtr<UNiagaraSystem> TrajectoryNiagaraSystem = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam|Trajectory", meta=(ClampMin="2", ClampMax="64"))
	int32 TrajectoryPointCount = 16;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Skill|BurningSlam|Trajectory", meta=(ClampMin="0.0"))
	float TrajectoryStartForwardOffset = 50.0f;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierPlayerCharacter> CachedPlayerCharacter = nullptr;

	FVector StartLocation = FVector::ZeroVector;
	FVector TargetActorLocation = FVector::ZeroVector;
	FVector TargetGroundLocation = FVector::ZeroVector;
	float MovementStartTimeSeconds = 0.0f;
	float CurrentTargetDistance = 0.0f;
	float ResolvedMaxTargetDistance = 0.0f;
	float ResolvedImpactRadius = 0.0f;
	float ResolvedDamageMultiplier = 1.0f;
	bool bConfirmed = false;
	bool bImpactDamageApplied = false;
	bool bAnimationModeOverridden = false;
	FGuid ActiveWarningId;
	FGuid ActiveTrajectoryId;
	FTimerHandle MovementTimerHandle;
	FTimerHandle ConfirmTimeoutTimerHandle;
	FTimerHandle TargetingUpdateTimerHandle;
	TEnumAsByte<EMovementMode> CachedMovementMode = MOVE_Walking;
	TEnumAsByte<ECollisionResponse> CachedEnemyCollisionResponse = ECR_Block;
	TEnumAsByte<ECollisionResponse> CachedPlayerCollisionResponse = ECR_Block;
	bool bSlamCollisionResponsesOverridden = false;
};
