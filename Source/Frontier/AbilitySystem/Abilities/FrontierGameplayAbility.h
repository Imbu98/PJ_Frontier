#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Combat/FrontierDamageStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FrontierGameplayAbility.generated.h"

class AFrontierPlayerCharacter;
class USceneComponent;

UCLASS()
class FRONTIER_API UFrontierGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UFrontierGameplayAbility();

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

	UFUNCTION(BlueprintCallable, Category="Frontier|Ability")
	FFrontierDamageResult ApplyDamageToTargetActor(AActor* TargetActor, float BaseDamage, FGameplayTag DamageTypeTag, float DamageMultiplier = 1.0f, EFrontierElementalType ElementalType = EFrontierElementalType::Normal) const;

	UFUNCTION(BlueprintCallable, Category="Frontier|Ability")
	TArray<FFrontierDamageResult> ApplySphereTraceDamageFromWeaponSockets(
		USceneComponent* WeaponComponent,
		FName StartSocketName,
		FName EndSocketName,
		float TraceRadius,
		float BaseDamage,
		FGameplayTag DamageTypeTag,
		EFrontierElementalType ElementalType = EFrontierElementalType::Normal) const;

protected:
	bool ShouldAdjustDirectionWhileCasting() const { return bCanAdjustDirectionWhileCasting; }
	FVector GetCastingFacingDirection() const { return CastingFacingDirection; }

	/** Rotates a player avatar to the controller camera yaw when the ability starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Casting")
	bool bFaceCameraDirectionOnActivation = true;

	/** When false, movement input is locked until the ability ends or is cancelled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Casting")
	bool bCanMoveWhileCasting = true;

	/** When false, the initial cast direction is fixed for the rest of the ability. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Casting")
	bool bCanAdjustDirectionWhileCasting = true;

private:
	void ApplyCastingControls(const FGameplayAbilityActorInfo* ActorInfo);
	void RestoreCastingControls();

	UPROPERTY(Transient)
	TObjectPtr<AFrontierPlayerCharacter> CastingPlayerCharacter;

	TEnumAsByte<EMovementMode> CachedCastingMovementMode = MOVE_Walking;
	uint8 CachedCastingCustomMovementMode = 0;
	FVector CastingFacingDirection = FVector::ForwardVector;
	bool bCastingMovementLocked = false;
};


