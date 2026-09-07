#include "AbilitySystem/Abilities/FrontierGameplayAbility_MeleeMontageSkill.h"

#include "Animation/AnimMontage.h"
#include "Character/FrontierBaseCharacter.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Components/FrontierCombatComponent.h"
#include "Components/FrontierEquipmentComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Frontier.h"
#include "Tags/FrontierGameplayTags.h"
#include "TimerManager.h"
#include "Weapons/FrontierWeaponBase.h"

UFrontierGameplayAbility_MeleeMontageSkill::UFrontierGameplayAbility_MeleeMontageSkill()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	ActivationOwnedTags.AddTag(Tags.StateCombatSkill);
	ActivationBlockedTags.AddTag(Tags.StateDead);
	ActivationBlockedTags.AddTag(Tags.StateCCStun);
	ActivationBlockedTags.AddTag(Tags.StateActionAttacking);
	ActivationBlockedTags.AddTag(Tags.StateCombatSkill);

	AttackData.DamageTypeTag = Tags.DamageTypeNormal;
	AttackData.HitReactionTag = Tags.HitReactionLight;
	AttackData.HitReactionResistance = EFrontierHitReactionLevel::Stagger;
}

bool UFrontierGameplayAbility_MeleeMontageSkill::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AFrontierBaseCharacter* SourceCharacter = ActorInfo
		? Cast<AFrontierBaseCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	const UFrontierEquipmentComponent* EquipmentComponent = SourceCharacter
		? SourceCharacter->FindComponentByClass<UFrontierEquipmentComponent>()
		: nullptr;
	const UFrontierWeaponDataAsset* WeaponData = EquipmentComponent
		? EquipmentComponent->GetCurrentWeaponData()
		: nullptr;
	const UCharacterMovementComponent* MovementComponent = SourceCharacter
		? SourceCharacter->GetCharacterMovement()
		: nullptr;

	return SourceCharacter
		&& SourceCharacter->HasAuthority()
		&& !SourceCharacter->IsDead()
		&& MovementComponent
		&& !MovementComponent->IsFalling()
		&& WeaponData
		&& RequiredWeaponTypeTag.IsValid()
		&& WeaponData->WeaponTypeTag.MatchesTag(RequiredWeaponTypeTag)
		&& !AttackData.AttackMontage.IsNull();
}

void UFrontierGameplayAbility_MeleeMontageSkill::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AFrontierBaseCharacter* SourceCharacter = ActorInfo
		? Cast<AFrontierBaseCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	if (!ResolveRuntimeData(SourceCharacter) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FRONTIER_LOG(Warning, TEXT("SpearHackSlash activation failed because runtime attack data could not be resolved or committed. Character=%s"),
			*GetNameSafe(SourceCharacter));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (UFrontierCombatComponent* CombatComponent = CachedSourceCharacter->GetCombatComponent())
	{
		CombatComponent->ResetHitActorsThisAttack();
	}

	CachedSourceCharacter->SetMontageHitReactionResistance(LoadedAttackMontage, AttackData.HitReactionResistance);
	if (AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(CachedSourceCharacter))
	{
		PlayerCharacter->SetAttackUseControllerRotationInternal(ShouldAdjustDirectionWhileCasting());
		PlayerCharacter->SetAttackMovementSpeedMultiplierInternal(AttackData.AttackMovementSpeedMultiplier);
		PlayerCharacter->SetCurrentAttackAnimationModeInternal(AttackData.AttackAnimationMode);
		PlayerCharacter->MulticastPlaySkillMontage(LoadedAttackMontage, AttackData.AttackAnimationMode);
	}
	else
	{
		CachedSourceCharacter->PlayAnimMontage(LoadedAttackMontage);
	}

	LockCharacterMovementIfNeeded();

	if (UWorld* World = GetWorld())
	{
		const float EndDelay = FMath::Max(LoadedAttackMontage->GetPlayLength(), 0.1f) + MontageEndBuffer;
		World->GetTimerManager().SetTimer(
			AbilityEndTimerHandle,
			this,
			&UFrontierGameplayAbility_MeleeMontageSkill::FinishAbility,
			EndDelay,
			false);
	}
}

void UFrontierGameplayAbility_MeleeMontageSkill::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AbilityEndTimerHandle);
	}

	UnlockCharacterMovementIfNeeded();
	if (CachedSourceCharacter)
	{
		CachedSourceCharacter->ClearMontageHitReactionResistance(LoadedAttackMontage);
	}
	if (AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(CachedSourceCharacter))
	{
		PlayerCharacter->SetAttackUseControllerRotationInternal(false);
		PlayerCharacter->SetAttackMovementSpeedMultiplierInternal(1.0f);
		PlayerCharacter->SetCurrentAttackAnimationModeInternal(EFrontierAttackAnimationMode::UpperBodyOnly);
	}

	CachedSourceCharacter = nullptr;
	CachedWeapon = nullptr;
	LoadedAttackMontage = nullptr;
	LoadedDamageEffectClass = nullptr;
	LoadedAdditionalHitEffectClasses.Reset();
	CachedWeaponElementalType = EFrontierElementalType::Normal;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UFrontierGameplayAbility_MeleeMontageSkill::BeginAttackTraceWindow(const FFrontierMeleeTraceOverrides& TraceOverrides)
{
	if (TraceOverrides.bResetHitActorsOnBegin && CachedSourceCharacter && CachedSourceCharacter->HasAuthority())
	{
		if (UFrontierCombatComponent* CombatComponent = CachedSourceCharacter->GetCombatComponent())
		{
			CombatComponent->ResetHitActorsThisAttack();
		}
	}
}

void UFrontierGameplayAbility_MeleeMontageSkill::TickAttackTraceWindow(const FFrontierMeleeTraceOverrides& TraceOverrides)
{
	ExecuteAttackTrace(TraceOverrides);
}

void UFrontierGameplayAbility_MeleeMontageSkill::EndAttackTraceWindow()
{
}

void UFrontierGameplayAbility_MeleeMontageSkill::FinishAbility()
{
	if (IsActive())
	{
		K2_EndAbility();
	}
}

bool UFrontierGameplayAbility_MeleeMontageSkill::ResolveRuntimeData(AFrontierBaseCharacter* SourceCharacter)
{
	CachedSourceCharacter = SourceCharacter;
	LoadedAdditionalHitEffectClasses.Reset();
	if (!CachedSourceCharacter)
	{
		return false;
	}

	UFrontierEquipmentComponent* EquipmentComponent = CachedSourceCharacter->FindComponentByClass<UFrontierEquipmentComponent>();
	const UFrontierWeaponDataAsset* WeaponData = EquipmentComponent ? EquipmentComponent->GetCurrentWeaponData() : nullptr;
	if (!WeaponData || !RequiredWeaponTypeTag.IsValid() || !WeaponData->WeaponTypeTag.MatchesTag(RequiredWeaponTypeTag))
	{
		return false;
	}

	CachedWeapon = EquipmentComponent->GetCurrentWeapon();
	CachedWeaponElementalType = EquipmentComponent->GetCurrentWeaponElementalType();
	LoadedAttackMontage = AttackData.AttackMontage.LoadSynchronous();
	LoadedDamageEffectClass = AttackData.DamageEffectClass.IsNull()
		? nullptr
		: AttackData.DamageEffectClass.LoadSynchronous();

	for (const TSoftClassPtr<UGameplayEffect>& EffectClass : AttackData.AdditionalHitEffectClasses)
	{
		if (!EffectClass.IsNull())
		{
			if (TSubclassOf<UGameplayEffect> LoadedClass = EffectClass.LoadSynchronous())
			{
				LoadedAdditionalHitEffectClasses.Add(LoadedClass);
			}
		}
	}

	return CachedWeapon
		&& LoadedAttackMontage
		&& AttackData.DamageMultiplier > 0.0f
		&& AttackData.HitRadius > 0.0f;
}

void UFrontierGameplayAbility_MeleeMontageSkill::ExecuteAttackTrace(const FFrontierMeleeTraceOverrides& TraceOverrides)
{
	if (!CachedSourceCharacter || !CachedSourceCharacter->HasAuthority())
	{
		return;
	}

	UFrontierCombatComponent* CombatComponent = CachedSourceCharacter->GetCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	const FGameplayTag DefaultDamageTypeTag = AttackData.DamageTypeTag.IsValid()
		? AttackData.DamageTypeTag
		: FFrontierGameplayTags::Get().DamageTypeNormal;
	const FGameplayTag DamageTypeTag = TraceOverrides.bOverrideDamageType && TraceOverrides.DamageTypeTag.IsValid()
		? TraceOverrides.DamageTypeTag
		: DefaultDamageTypeTag;
	const float DamageMultiplier = TraceOverrides.bOverrideDamageMultiplier
		? FMath::Max(TraceOverrides.DamageMultiplier, 0.0f)
		: AttackData.DamageMultiplier;
	const FGameplayTag HitReactionTag = TraceOverrides.bOverrideHitReaction
		? TraceOverrides.HitReactionTag
		: AttackData.HitReactionTag;
	const EFrontierHitReactionLevel HitReactionLevel = TraceOverrides.bOverrideHitReaction
		? TraceOverrides.HitReactionLevel
		: AttackData.HitReactionLevel;
	const float StaggerDuration = TraceOverrides.bOverrideHitReaction
		? TraceOverrides.StaggerDuration
		: AttackData.StaggerDuration;
	const float KnockbackHorizontalStrength = TraceOverrides.bOverrideHitReaction
		? TraceOverrides.KnockbackHorizontalStrength
		: AttackData.KnockbackHorizontalStrength;
	const float KnockbackVerticalStrength = TraceOverrides.bOverrideHitReaction
		? TraceOverrides.KnockbackVerticalStrength
		: AttackData.KnockbackVerticalStrength;
	auto ApplySocketTrace = [&](USceneComponent* TraceComponent)
	{
		CombatComponent->ApplySphereTraceDamageFromSockets(
			TraceComponent,
			AttackData.TraceStartSocketName,
			AttackData.TraceEndSocketName,
			AttackData.HitRadius,
			0.0f,
			DamageTypeTag,
			LoadedDamageEffectClass,
			&LoadedAdditionalHitEffectClasses,
			DamageMultiplier,
			AttackData.bDrawDebugTrace,
			CachedWeaponElementalType,
			HitReactionTag,
			HitReactionLevel,
			StaggerDuration,
			KnockbackHorizontalStrength,
			KnockbackVerticalStrength);
	};

	if (CachedWeapon && CachedWeapon->GetWeaponMesh())
	{
		USkeletalMeshComponent* WeaponMesh = CachedWeapon->GetWeaponMesh();
		if (WeaponMesh->DoesSocketExist(AttackData.TraceStartSocketName)
			&& WeaponMesh->DoesSocketExist(AttackData.TraceEndSocketName))
		{
			ApplySocketTrace(WeaponMesh);
			return;
		}
	}

	if (USkeletalMeshComponent* CharacterMesh = CachedSourceCharacter->GetMesh())
	{
		if (CharacterMesh->DoesSocketExist(AttackData.TraceStartSocketName)
			&& CharacterMesh->DoesSocketExist(AttackData.TraceEndSocketName))
		{
			ApplySocketTrace(CharacterMesh);
			return;
		}
	}

	const FVector TraceStart = CachedSourceCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
	const FVector TraceEnd = TraceStart + CachedSourceCharacter->GetActorForwardVector().GetSafeNormal() * AttackData.AttackRange;
	CombatComponent->ApplySphereTraceDamageBetweenPoints(
		TraceStart,
		TraceEnd,
		AttackData.HitRadius,
		0.0f,
		DamageTypeTag,
		LoadedDamageEffectClass,
		&LoadedAdditionalHitEffectClasses,
		DamageMultiplier,
		AttackData.bDrawDebugTrace,
		CachedWeaponElementalType,
		HitReactionTag,
		HitReactionLevel,
		StaggerDuration,
		KnockbackHorizontalStrength,
		KnockbackVerticalStrength);
}

void UFrontierGameplayAbility_MeleeMontageSkill::LockCharacterMovementIfNeeded()
{
	bMovementLocked = false;
	if (!CachedSourceCharacter || !AttackData.bLockMovementDuringAttack)
	{
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = CachedSourceCharacter->GetCharacterMovement())
	{
		CachedMovementMode = MovementComponent->MovementMode;
		CachedCustomMovementMode = MovementComponent->CustomMovementMode;
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
		bMovementLocked = true;
	}
}

void UFrontierGameplayAbility_MeleeMontageSkill::UnlockCharacterMovementIfNeeded()
{
	if (!bMovementLocked || !CachedSourceCharacter)
	{
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = CachedSourceCharacter->GetCharacterMovement())
	{
		MovementComponent->SetMovementMode(CachedMovementMode, CachedCustomMovementMode);
	}
	bMovementLocked = false;
}
