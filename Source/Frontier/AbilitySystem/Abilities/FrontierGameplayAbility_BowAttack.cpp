#include "AbilitySystem/Abilities/FrontierGameplayAbility_BowAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "AbilitySystem/FrontierAttributeSet.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Components/FrontierEquipmentComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "FrontierPlayerController.h"
#include "Frontier.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Weapons/FrontierArrowProjectile.h"
#include "Weapons/FrontierBowWeaponBase.h"

UFrontierGameplayAbility_BowAttack::UFrontierGameplayAbility_BowAttack()
{
}

bool UFrontierGameplayAbility_BowAttack::CanActivateAbility(
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

	const AFrontierPlayerCharacter* PlayerCharacter = ActorInfo
		? Cast<AFrontierPlayerCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	const UFrontierEquipmentComponent* Equipment = PlayerCharacter ? PlayerCharacter->GetEquipmentComponent() : nullptr;
	const UFrontierWeaponDataAsset* WeaponData = Equipment ? Equipment->GetCurrentWeaponData() : nullptr;
	const AFrontierBowWeaponBase* BowWeapon = Equipment
		? Cast<AFrontierBowWeaponBase>(Equipment->GetCurrentWeapon())
		: nullptr;
	const UFrontierAttributeSet* Attributes = PlayerCharacter ? PlayerCharacter->GetFrontierAttributeSet() : nullptr;
	const UCharacterMovementComponent* Movement = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;

	return PlayerCharacter
		&& !PlayerCharacter->IsDead()
		&& !PlayerCharacter->IsAttackDelayActive()
		&& Movement
		&& !Movement->IsFalling()
		&& BowWeapon
		&& WeaponData
		&& !WeaponData->BowAttack.ArrowProjectileClass.IsNull()
		&& Attributes
		&& Attributes->GetStamina() >= WeaponData->BowAttack.StaminaCost;
}

void UFrontierGameplayAbility_BowAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AFrontierPlayerCharacter* PlayerCharacter = ActorInfo
		? Cast<AFrontierPlayerCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	if (!PlayerCharacter || !ResolveBowData(PlayerCharacter) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (PlayerCharacter->HasAuthority())
	{
		if (UFrontierAbilitySystemComponent* ASC = PlayerCharacter->GetFrontierAbilitySystemComponent())
		{
			ASC->ApplyStaminaDelta(-BowData.StaminaCost);
		}
	}

	bArrowReleased = false;
	DrawStartedAtSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	PlayerCharacter->SetNockedArrowVisual(
		true,
		BowData.ArrowMesh.LoadSynchronous(),
		BowData.NockedArrowCharacterSocketName,
		BowData.NockedArrowRelativeTransform);
	if (CachedWeapon)
	{
		CachedWeapon->SetBowChargeProgress(0.0f);
	}
	PlayerCharacter->SetAiming(true);
	PlayerCharacter->SetAttackUseControllerRotationInternal(true);
	PlayerCharacter->SetAttackMovementSpeedMultiplierInternal(0.6f);
	PlayerCharacter->SetCurrentAttackAnimationModeInternal(EFrontierAttackAnimationMode::UpperBodyOnly);
	UpdateChargeWidget();
	CachedWeapon->PlayAttackSound(EFrontierWeaponAttackSound::BowDraw);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ChargeWidgetTimerHandle,
			this,
			&UFrontierGameplayAbility_BowAttack::UpdateChargeWidget,
			0.02f,
			true);
	}

	PlayDrawMontage();
}

void UFrontierGameplayAbility_BowAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (CachedWeapon && !bArrowReleased)
	{
		CachedWeapon->StopBowDrawSound();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeWidgetTimerHandle);
	}
	if (AFrontierPlayerController* Controller = CachedPlayerCharacter
		? Cast<AFrontierPlayerController>(CachedPlayerCharacter->GetController())
		: nullptr)
	{
		Controller->SetWeaponAttackCharge(0.0f);
	}
	if (CachedPlayerCharacter)
	{
		if (CachedWeapon)
		{
			CachedWeapon->SetBowChargeProgress(0.0f);
		}
		CachedPlayerCharacter->SetNockedArrowVisual(false, nullptr, NAME_None, FTransform::Identity);
		CachedPlayerCharacter->SetAiming(false);
		CachedPlayerCharacter->SetAttackUseControllerRotationInternal(false);
		CachedPlayerCharacter->SetAttackMovementSpeedMultiplierInternal(1.0f);
		CachedPlayerCharacter->SetCurrentAttackAnimationModeInternal(EFrontierAttackAnimationMode::UpperBodyOnly);
	}

	CachedPlayerCharacter = nullptr;
	CachedWeapon = nullptr;
	DrawMontage = nullptr;
	ReleaseMontage = nullptr;
	BowData = FFrontierBowAttackData();
	DrawStartedAtSeconds = 0.0f;
	bArrowReleased = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UFrontierGameplayAbility_BowAttack::HandleAttackInputReleased()
{
	ReleaseArrow();
}

bool UFrontierGameplayAbility_BowAttack::ResolveBowData(AFrontierPlayerCharacter* PlayerCharacter)
{
	UFrontierEquipmentComponent* Equipment = PlayerCharacter ? PlayerCharacter->GetEquipmentComponent() : nullptr;
	UFrontierWeaponDataAsset* WeaponData = Equipment ? Equipment->GetCurrentWeaponData() : nullptr;
	CachedWeapon = Equipment ? Cast<AFrontierBowWeaponBase>(Equipment->GetCurrentWeapon()) : nullptr;
	if (!WeaponData || !CachedWeapon || WeaponData->BowAttack.ArrowProjectileClass.IsNull())
	{
		return false;
	}

	CachedPlayerCharacter = PlayerCharacter;
	BowData = WeaponData->BowAttack;
	DrawMontage = BowData.DrawMontage.LoadSynchronous();
	ReleaseMontage = BowData.ReleaseMontage.LoadSynchronous();
	return true;
}

void UFrontierGameplayAbility_BowAttack::PlayDrawMontage()
{
	if (!DrawMontage)
	{
		return;
	}

	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		DrawMontage);
	Task->OnCompleted.AddDynamic(this, &UFrontierGameplayAbility_BowAttack::HandleDrawCompleted);
	Task->OnInterrupted.AddDynamic(this, &UFrontierGameplayAbility_BowAttack::HandleMontageCancelled);
	Task->OnCancelled.AddDynamic(this, &UFrontierGameplayAbility_BowAttack::HandleMontageCancelled);
	Task->ReadyForActivation();
}

void UFrontierGameplayAbility_BowAttack::PlayReleaseMontage()
{
	if (!ReleaseMontage)
	{
		HandleReleaseCompleted();
		return;
	}

	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ReleaseMontage);
	Task->OnCompleted.AddDynamic(this, &UFrontierGameplayAbility_BowAttack::HandleReleaseCompleted);
	Task->OnInterrupted.AddDynamic(this, &UFrontierGameplayAbility_BowAttack::HandleReleaseCompleted);
	Task->OnCancelled.AddDynamic(this, &UFrontierGameplayAbility_BowAttack::HandleReleaseCompleted);
	Task->ReadyForActivation();
}

void UFrontierGameplayAbility_BowAttack::HandleDrawCompleted()
{
	// The AnimBP Aim Offset owns the sustained aiming pose after the pull montage blends out.
}

void UFrontierGameplayAbility_BowAttack::HandleMontageCancelled()
{
	if (!bArrowReleased)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UFrontierGameplayAbility_BowAttack::HandleReleaseCompleted()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

float UFrontierGameplayAbility_BowAttack::GetChargeProgress() const
{
	const float Elapsed = GetWorld() ? GetWorld()->GetTimeSeconds() - DrawStartedAtSeconds : 0.0f;
	return FMath::Clamp(Elapsed / FMath::Max(BowData.FullDrawSeconds, 0.01f), 0.0f, 1.0f);
}

void UFrontierGameplayAbility_BowAttack::UpdateChargeWidget()
{
	const float ChargeProgress = GetChargeProgress();
	if (CachedWeapon)
	{
		CachedWeapon->SetBowChargeProgress(ChargeProgress);
	}

	AFrontierPlayerController* Controller = CachedPlayerCharacter
		? Cast<AFrontierPlayerController>(CachedPlayerCharacter->GetController())
		: nullptr;
	if (Controller && Controller->IsLocalController())
	{
		Controller->SetWeaponAttackCharge(ChargeProgress);
	}
}

void UFrontierGameplayAbility_BowAttack::ReleaseArrow()
{
	if (bArrowReleased || !CachedPlayerCharacter || !CachedWeapon)
	{
		return;
	}

	bArrowReleased = true;
	const float ChargeProgress = GetChargeProgress();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeWidgetTimerHandle);
	}
	if (AFrontierPlayerController* Controller = Cast<AFrontierPlayerController>(CachedPlayerCharacter->GetController()))
	{
		Controller->SetWeaponAttackCharge(0.0f);
	}
	CachedPlayerCharacter->SetNockedArrowVisual(false, nullptr, NAME_None, FTransform::Identity);
	CachedPlayerCharacter->SetAiming(false);
	if (UFrontierAbilitySystemComponent* ASC = CachedPlayerCharacter->GetFrontierAbilitySystemComponent())
	{
		ASC->CurrentMontageStop(0.08f);
	}
	CachedWeapon->PlayAttackSound(EFrontierWeaponAttackSound::BowRelease);
	CachedWeapon->PlayAttackSound(EFrontierWeaponAttackSound::AttackVoice);

	if (CachedPlayerCharacter->HasAuthority())
	{
		UWorld* World = GetWorld();
		USkeletalMeshComponent* WeaponMesh = CachedWeapon->GetWeaponMesh();
		const TSubclassOf<AFrontierArrowProjectile> ProjectileClass = BowData.ArrowProjectileClass.LoadSynchronous();
		if (World && WeaponMesh && ProjectileClass)
		{
			const FVector SpawnLocation = WeaponMesh->DoesSocketExist(BowData.ArrowSpawnSocketName)
				? WeaponMesh->GetSocketLocation(BowData.ArrowSpawnSocketName)
				: WeaponMesh->GetComponentLocation();
			const FVector AimDirection = ResolveAimDirection(SpawnLocation);
			const FTransform SpawnTransform(AimDirection.Rotation(), SpawnLocation);
			AFrontierArrowProjectile* Arrow = World->SpawnActorDeferred<AFrontierArrowProjectile>(
				ProjectileClass,
				SpawnTransform,
				CachedPlayerCharacter,
				CachedPlayerCharacter,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Arrow)
			{
				FRONTIER_LOG(Log, TEXT("Arrow projectile spawned. Arrow=%s SpawnLocation=%s Socket=%s"),
					*GetNameSafe(Arrow),
					*SpawnLocation.ToCompactString(),
					*BowData.ArrowSpawnSocketName.ToString());
				FFrontierDamageRequest DamageRequest;
				DamageRequest.BaseDamage = BowData.BaseDamage;
				DamageRequest.DamageMultiplier = FMath::Lerp(
					BowData.BaseDamageMultiplier,
					BowData.MaximumDamageMultiplier,
					ChargeProgress);
				DamageRequest.DamageTypeTag = BowData.DamageTypeTag;
				DamageRequest.HitReactionTag = BowData.HitReactionTag;
				DamageRequest.HitReactionLevel = BowData.HitReactionLevel;
				DamageRequest.StaggerDuration = BowData.StaggerDuration;
				DamageRequest.KnockbackHorizontalStrength = BowData.KnockbackHorizontalStrength;
				DamageRequest.KnockbackVerticalStrength = BowData.KnockbackVerticalStrength;
				if (const UFrontierEquipmentComponent* Equipment = CachedPlayerCharacter->GetEquipmentComponent())
				{
					DamageRequest.ElementalType = Equipment->GetCurrentWeaponElementalType();
				}
				DamageRequest.DamageEffectClass = BowData.DamageEffectClass.LoadSynchronous();
				for (const TSoftClassPtr<UGameplayEffect>& EffectClass : BowData.AdditionalHitEffectClasses)
				{
					if (const TSubclassOf<UGameplayEffect> LoadedClass = EffectClass.LoadSynchronous())
					{
						DamageRequest.AdditionalEffectClasses.Add(LoadedClass);
					}
				}

				Arrow->InitializeArrow(
					CachedPlayerCharacter,
					BowData.ArrowMesh.LoadSynchronous(),
					DamageRequest,
					BowData.ProjectileGravityScale);
				UGameplayStatics::FinishSpawningActor(Arrow, SpawnTransform);

				const float RangeMultiplier = FMath::Lerp(1.0f, BowData.MaximumRangeMultiplier, ChargeProgress);
				const float SpeedMultiplier = FMath::Sqrt(RangeMultiplier);
				Arrow->Launch(
					AimDirection,
					BowData.BaseProjectileSpeed * SpeedMultiplier,
					BowData.BaseProjectileRange * RangeMultiplier);
			}
			else
			{
				FRONTIER_LOG(Warning, TEXT("Arrow projectile spawn failed. Class=%s"), *GetNameSafe(ProjectileClass));
			}
		}
		else
		{
			FRONTIER_LOG(
				Warning,
				TEXT("Arrow projectile launch skipped. World=%d WeaponMesh=%d ProjectileClass=%s"),
				World ? 1 : 0,
				WeaponMesh ? 1 : 0,
				*GetNameSafe(ProjectileClass));
		}
	}

	PlayReleaseMontage();
}

FVector UFrontierGameplayAbility_BowAttack::ResolveAimDirection(const FVector& SpawnLocation) const
{
	APlayerController* Controller = CachedPlayerCharacter
		? Cast<APlayerController>(CachedPlayerCharacter->GetController())
		: nullptr;
	FVector ViewLocation = CachedPlayerCharacter ? CachedPlayerCharacter->GetPawnViewLocation() : SpawnLocation;
	FRotator ViewRotation = CachedPlayerCharacter ? CachedPlayerCharacter->GetActorRotation() : FRotator::ZeroRotator;
	if (Controller)
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}

	const FVector ViewDirection = ViewRotation.Vector();
	const FVector TraceEnd = ViewLocation + (ViewDirection * BowData.BaseProjectileRange * BowData.MaximumRangeMultiplier);
	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BowAim), true, CachedPlayerCharacter);
	const bool bHit = GetWorld() && GetWorld()->LineTraceSingleByChannel(
		HitResult,
		ViewLocation,
		TraceEnd,
		ECC_Visibility,
		QueryParams);
	const FVector AimPoint = bHit ? HitResult.ImpactPoint : TraceEnd;
	return (AimPoint - SpawnLocation).GetSafeNormal(KINDA_SMALL_NUMBER, ViewDirection);
}
