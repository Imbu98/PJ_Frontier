#pragma once

#include "CoreMinimal.h"
#include "Combat/FrontierDamageStatics.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "../Frontier.h"
#include "FrontierCombatComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFrontierAttackEvent, int32, AttackCounter);

class UFrontierAbilitySystemComponent;
class USceneComponent;
class UGameplayEffect;

UCLASS(ClassGroup=(Frontier), meta=(BlueprintSpawnableComponent))
class FRONTIER_API UFrontierCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFrontierCombatComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category="Combat")
	void RequestPrimaryAttack();

	UFUNCTION(BlueprintCallable, Category="Combat")
	void RequestPrimaryAttackWithNotify();

	UFUNCTION(BlueprintPure, Category="Combat")
	bool CanRequestPrimaryAttack() const;
	
	FFrontierDamageResult ApplyDamageToTarget(AActor* TargetActor, float BaseDamage, FGameplayTag DamageTypeTag, float DamageMultiplier = 1.0f, EFrontierElementalType ElementalType = EFrontierElementalType::Normal, FGameplayTag HitReactionTag = FGameplayTag(), EFrontierHitReactionLevel HitReactionLevel = EFrontierHitReactionLevel::None, float StaggerDuration = 0.35f, float KnockbackHorizontalStrength = 650.0f, float KnockbackVerticalStrength = 150.0f, FVector HitLocation = FVector::ZeroVector);
	FFrontierDamageResult ApplyDamageToTarget(AActor* TargetActor, float BaseDamage, FGameplayTag DamageTypeTag, TSubclassOf<UGameplayEffect> DamageEffectClass, float DamageMultiplier = 1.0f, EFrontierElementalType ElementalType = EFrontierElementalType::Normal, FGameplayTag HitReactionTag = FGameplayTag(), EFrontierHitReactionLevel HitReactionLevel = EFrontierHitReactionLevel::None, float StaggerDuration = 0.35f, float KnockbackHorizontalStrength = 650.0f, float KnockbackVerticalStrength = 150.0f, FVector HitLocation = FVector::ZeroVector);
	FFrontierDamageResult ApplyDamageToTarget(AActor* TargetActor, float BaseDamage, FGameplayTag DamageTypeTag, TSubclassOf<UGameplayEffect> DamageEffectClass, const TArray<TSubclassOf<UGameplayEffect>>* AdditionalEffectClasses, float DamageMultiplier = 1.0f, EFrontierElementalType ElementalType = EFrontierElementalType::Normal, FGameplayTag HitReactionTag = FGameplayTag(), EFrontierHitReactionLevel HitReactionLevel = EFrontierHitReactionLevel::None, float StaggerDuration = 0.35f, float KnockbackHorizontalStrength = 650.0f, float KnockbackVerticalStrength = 150.0f, FVector HitLocation = FVector::ZeroVector);

	TArray<FFrontierDamageResult> ApplySphereTraceDamageFromSockets(
		USceneComponent* TraceComponent,
		FName StartSocketName,
		FName EndSocketName,
		float TraceRadius,
		float BaseDamage,
		FGameplayTag DamageTypeTag,
		TSubclassOf<UGameplayEffect> DamageEffectClass = nullptr,
		const TArray<TSubclassOf<UGameplayEffect>>* AdditionalEffectClasses = nullptr,
		float DamageMultiplier = 1.0f,
		bool bDrawDebugTrace = false,
		EFrontierElementalType ElementalType = EFrontierElementalType::Normal,
		FGameplayTag HitReactionTag = FGameplayTag(),
		EFrontierHitReactionLevel HitReactionLevel = EFrontierHitReactionLevel::None,
		float StaggerDuration = 0.35f,
		float KnockbackHorizontalStrength = 650.0f,
		float KnockbackVerticalStrength = 150.0f,
		FVector HitLocation = FVector::ZeroVector);

	TArray<FFrontierDamageResult> ApplySphereTraceDamageBetweenPoints(
		const FVector& TraceStart,
		const FVector& TraceEnd,
		float TraceRadius,
		float BaseDamage,
		FGameplayTag DamageTypeTag,
		TSubclassOf<UGameplayEffect> DamageEffectClass = nullptr,
		const TArray<TSubclassOf<UGameplayEffect>>* AdditionalEffectClasses = nullptr,
		float DamageMultiplier = 1.0f,
		bool bDrawDebugTrace = false,
		EFrontierElementalType ElementalType = EFrontierElementalType::Normal,
		FGameplayTag HitReactionTag = FGameplayTag(),
		EFrontierHitReactionLevel HitReactionLevel = EFrontierHitReactionLevel::None,
		float StaggerDuration = 0.35f,
		float KnockbackHorizontalStrength = 650.0f,
		float KnockbackVerticalStrength = 150.0f,
		FVector HitLocation = FVector::ZeroVector);

	UFUNCTION(BlueprintCallable, Category="Combat")
	void ResetHitActorsThisAttack();

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FFrontierAttackEvent OnAttackPredicted;

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FFrontierAttackEvent OnAttackConfirmed;

protected:
	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable)
	void Server_RequestPrimaryAttack();

	UFUNCTION(Server, Reliable)
	void Server_RequestPrimaryAttackWithNotify();

	UFUNCTION()
	void OnRep_AttackCounter();

private:
	void HandlePrimaryAttackConfirmed(bool bExecuteDefaultAttack);
	void PerformDefaultPrimaryAttack();
	UFrontierAbilitySystemComponent* GetFrontierAbilitySystemComponent() const;

	UPROPERTY(EditDefaultsOnly, Category="Combat|Primary Attack", meta=(ClampMin="0.0"))
	float PrimaryAttackDamage = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category="Combat|Primary Attack", meta=(ClampMin="0.0"))
	float PrimaryAttackRange = 175.0f;

	UPROPERTY(EditDefaultsOnly, Category="Combat|Primary Attack", meta=(ClampMin="0.0"))
	float PrimaryAttackRadius = 60.0f;

	UPROPERTY(ReplicatedUsing=OnRep_AttackCounter)
	int32 AttackCounter = 0;

	UPROPERTY()
	TSet<TObjectPtr<AActor>> HitActorsThisAttack;
};


