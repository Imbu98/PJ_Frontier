#include "AbilitySystem/Abilities/FrontierGameplayAbility.h"

#include "Character/FrontierBaseCharacter.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Components/FrontierCombatComponent.h"
#include "Frontier.h"
#include "GameFramework/Controller.h"

UFrontierGameplayAbility::UFrontierGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UFrontierGameplayAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	ApplyCastingControls(ActorInfo);
}

void UFrontierGameplayAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	RestoreCastingControls();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UFrontierGameplayAbility::ApplyCastingControls(const FGameplayAbilityActorInfo* ActorInfo)
{
	RestoreCastingControls();
	CastingPlayerCharacter = ActorInfo ? Cast<AFrontierPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!CastingPlayerCharacter)
	{
		return;
	}

	CastingFacingDirection = CastingPlayerCharacter->GetActorForwardVector().GetSafeNormal2D();
	if (const AController* Controller = CastingPlayerCharacter->GetController())
	{
		const FRotator CameraYaw(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		CastingFacingDirection = CameraYaw.Vector().GetSafeNormal2D();
		if (bFaceCameraDirectionOnActivation && !CastingFacingDirection.IsNearlyZero())
		{
			CastingPlayerCharacter->SetActorRotation(CastingFacingDirection.Rotation());
		}
	}

	CastingPlayerCharacter->SetAttackUseControllerRotationInternal(bCanAdjustDirectionWhileCasting);

	if (!bCanMoveWhileCasting)
	{
		if (UCharacterMovementComponent* MovementComponent = CastingPlayerCharacter->GetCharacterMovement())
		{
			CachedCastingMovementMode = MovementComponent->MovementMode;
			CachedCastingCustomMovementMode = MovementComponent->CustomMovementMode;
			MovementComponent->StopMovementImmediately();
			MovementComponent->DisableMovement();
			bCastingMovementLocked = true;
		}
	}
}

void UFrontierGameplayAbility::RestoreCastingControls()
{
	if (CastingPlayerCharacter)
	{
		CastingPlayerCharacter->SetAttackUseControllerRotationInternal(false);
		if (bCastingMovementLocked)
		{
			if (UCharacterMovementComponent* MovementComponent = CastingPlayerCharacter->GetCharacterMovement())
			{
				MovementComponent->SetMovementMode(CachedCastingMovementMode, CachedCastingCustomMovementMode);
			}
		}
	}

	CastingPlayerCharacter = nullptr;
	bCastingMovementLocked = false;
	CachedCastingMovementMode = MOVE_Walking;
	CachedCastingCustomMovementMode = 0;
	CastingFacingDirection = FVector::ForwardVector;
}

FFrontierDamageResult UFrontierGameplayAbility::ApplyDamageToTargetActor(AActor* TargetActor, const float BaseDamage, const FGameplayTag DamageTypeTag, const float DamageMultiplier, const EFrontierElementalType ElementalType) const
{
	FFrontierDamageRequest DamageRequest;
	DamageRequest.BaseDamage = BaseDamage;
	DamageRequest.DamageMultiplier = DamageMultiplier;
	DamageRequest.DamageTypeTag = DamageTypeTag;
	DamageRequest.ElementalType = ElementalType;

	return UFrontierDamageStatics::ApplyDamage(GetAvatarActorFromActorInfo(), TargetActor, DamageRequest);
}

TArray<FFrontierDamageResult> UFrontierGameplayAbility::ApplySphereTraceDamageFromWeaponSockets(
	USceneComponent* WeaponComponent,
	const FName StartSocketName,
	const FName EndSocketName,
	const float TraceRadius,
	const float BaseDamage,
	const FGameplayTag DamageTypeTag,
	const EFrontierElementalType ElementalType) const
{
	if (AFrontierBaseCharacter* SourceCharacter = Cast<AFrontierBaseCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UFrontierCombatComponent* CombatComponent = SourceCharacter->GetCombatComponent())
		{
			return CombatComponent->ApplySphereTraceDamageFromSockets(
				WeaponComponent,
				StartSocketName,
				EndSocketName,
				TraceRadius,
				BaseDamage,
				DamageTypeTag,
				nullptr,
				nullptr,
				1.0f,
				false,
				ElementalType);
		}
	}

	FRONTIER_LOG(Warning, TEXT("GameplayAbility sphere trace damage failed because combat component could not be resolved."));
	return {};
}


