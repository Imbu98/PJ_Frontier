#include "AbilitySystem/Abilities/GA_ThrowAxe.h"

#include "AbilitySystem/Abilities/FrontierThrownAxeProjectile.h"
#include "Animation/AnimMontage.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Components/FrontierEquipmentComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Frontier.h"
#include "FrontierPlayerController.h"
#include "Tags/FrontierGameplayTags.h"
#include "Weapons/FrontierWeaponBase.h"
#include "Weapons/FrontierWeaponDataAsset.h"

UGA_ThrowAxe::UGA_ThrowAxe()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	ProjectileClass = AFrontierThrownAxeProjectile::StaticClass();

	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	ActivationOwnedTags.AddTag(Tags.StateCombatSkill);
	ActivationBlockedTags.AddTag(Tags.StateDead);
	ActivationBlockedTags.AddTag(Tags.StateCCStun);
	ActivationBlockedTags.AddTag(Tags.StateActionAttacking);
	ActivationBlockedTags.AddTag(Tags.StateCombatSkill);
	DamageTypeTag = Tags.DamageTypeSlash;
}

bool UGA_ThrowAxe::CanActivateAbility(
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

	AFrontierPlayerCharacter* PlayerCharacter = nullptr;
	USkeletalMesh* WeaponMesh = nullptr;
	TArray<UMaterialInterface*> WeaponMaterials;
	FGameplayTag WeaponTypeTag;
	return ResolveThrowContext(ActorInfo, PlayerCharacter, WeaponMesh, WeaponMaterials, WeaponTypeTag);
}

void UGA_ThrowAxe::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AFrontierPlayerCharacter* PlayerCharacter = nullptr;
	USkeletalMesh* WeaponMesh = nullptr;
	TArray<UMaterialInterface*> WeaponMaterials;
	FGameplayTag WeaponTypeTag;
	if (!ProjectileClass || !ThrowMontage || !ResolveThrowContext(ActorInfo, PlayerCharacter, WeaponMesh, WeaponMaterials, WeaponTypeTag))
	{
			
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FRONTIER_LOG(Warning, TEXT("Throw axe activation failed because CommitAbility was rejected."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CachedPlayerCharacter = PlayerCharacter;
	bProjectileThrown = false;
	bWeaponHiddenForThrow = false;
	ShowTrajectoryPreview(PlayerCharacter);

	PlayerCharacter->SetMontageHitReactionResistance(ThrowMontage, HitReactionResistance);
	PlayerCharacter->SetAttackUseControllerRotationInternal(ShouldAdjustDirectionWhileCasting());
	PlayerCharacter->SetCurrentAttackAnimationModeInternal(EFrontierAttackAnimationMode::UpperBodyOnly);
	PlayerCharacter->MulticastPlaySkillMontage(ThrowMontage, EFrontierAttackAnimationMode::UpperBodyOnly);

	if (UWorld* World = GetWorld())
	{
		const float MontageLength = FMath::Max(ThrowMontage->GetPlayLength(), 0.1f);
		World->GetTimerManager().ClearTimer(FinishAbilityTimerHandle);
		World->GetTimerManager().SetTimer(FinishAbilityTimerHandle, this, &UGA_ThrowAxe::FinishThrowAbility, MontageLength + 0.1f, false);
		World->GetTimerManager().ClearTimer(TrajectoryUpdateTimerHandle);
		World->GetTimerManager().SetTimer(TrajectoryUpdateTimerHandle, this, &UGA_ThrowAxe::UpdateTrajectoryPreview, TrajectoryUpdateInterval, true);
	}

	FRONTIER_LOG(Log, TEXT("Throw axe montage started. Player=%s Montage=%s WeaponType=%s WeaponElement=%d"),
		*GetNameSafe(PlayerCharacter),
		*GetNameSafe(ThrowMontage),
		*WeaponTypeTag.ToString(),
		static_cast<int32>(ElementalType));
}

void UGA_ThrowAxe::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FinishAbilityTimerHandle);
		World->GetTimerManager().ClearTimer(TrajectoryUpdateTimerHandle);
	}

	RestoreHiddenWeapon();
	HideTrajectoryPreview();

	if (CachedPlayerCharacter)
	{
		CachedPlayerCharacter->ClearMontageHitReactionResistance(ThrowMontage);
		CachedPlayerCharacter->SetAttackUseControllerRotationInternal(false);
		CachedPlayerCharacter->SetCurrentAttackAnimationModeInternal(EFrontierAttackAnimationMode::UpperBodyOnly);
	}

	CachedPlayerCharacter = nullptr;
	bProjectileThrown = false;
	bWeaponHiddenForThrow = false;
	ActiveTrajectoryId.Invalidate();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_ThrowAxe::FinishThrowAbility()
{
	if (IsActive())
	{
		K2_EndAbility();
	}
}

void UGA_ThrowAxe::ThrowAxeFromNotify()
{
	FRONTIER_LOG_FUNC();
	if (!CachedPlayerCharacter || !CachedPlayerCharacter->HasAuthority() || bProjectileThrown)
	{
		return;
	}

	AFrontierPlayerCharacter* PlayerCharacter = nullptr;
	USkeletalMesh* WeaponMesh = nullptr;
	TArray<UMaterialInterface*> WeaponMaterials;
	FGameplayTag WeaponTypeTag;
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ResolveThrowContext(ActorInfo, PlayerCharacter, WeaponMesh, WeaponMaterials, WeaponTypeTag))
	{
		FRONTIER_LOG(Warning, TEXT("Throw axe notify failed because context could not be resolved. CachedPlayer=%s"),
			*GetNameSafe(CachedPlayerCharacter.Get()));
		K2_CancelAbility();
		return;
	}

	UWorld* World = PlayerCharacter->GetWorld();
	if (!World)
	{
		K2_CancelAbility();
		return;
	}

	FRotator AimRotation = PlayerCharacter->GetActorRotation();
	
	// if (const AController* Controller = PlayerCharacter->GetController())
	// {
	// 	AimRotation = Controller->GetControlRotation();
	// }

	const FRotator YawOnlyRotation(0.0f, AimRotation.Yaw, 0.0f);
	const FVector Forward = YawOnlyRotation.Vector().GetSafeNormal2D();
	if (!Forward.IsNearlyZero())
	{
		PlayerCharacter->SetActorRotation(Forward.Rotation());
	}

	const FVector SpawnLocation = PlayerCharacter->GetActorLocation()
		+ Forward * SpawnForwardOffset
		+ FVector(0.0f, 0.0f, SpawnHeightOffset);
	const FRotator SpawnRotation = Forward.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = PlayerCharacter;
	SpawnParams.Instigator = PlayerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AFrontierThrownAxeProjectile* Projectile = World->SpawnActor<AFrontierThrownAxeProjectile>(
		ProjectileClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams);

	if (!Projectile)
	{
		FRONTIER_LOG(Warning, TEXT("Throw axe activation failed because projectile could not be spawned. Player=%s"),
			*GetNameSafe(PlayerCharacter));
		K2_CancelAbility();
		return;
	}

	Projectile->InitProjectile(
		PlayerCharacter,
		WeaponMesh,
		WeaponMaterials,
		BaseDamage,
		DamageMultiplier,
		DamageTypeTag.IsValid() ? DamageTypeTag : FFrontierGameplayTags::Get().DamageTypeSlash,
		ElementalType,
		DamageEffectClass);
	Projectile->LaunchInDirection(Forward, ProjectileSpeed);
	HideTrajectoryPreview();

	if (AFrontierWeaponBase* CurrentWeapon = PlayerCharacter->GetCurrentWeapon())
	{
		CurrentWeapon->SetWeaponActive(false);
		bWeaponHiddenForThrow = true;
		if (WeaponReappearDelay <= 0.0f)
		{
			RestoreHiddenWeapon();
		}
		else
		{
			World->GetTimerManager().ClearTimer(RestoreWeaponTimerHandle);
			World->GetTimerManager().SetTimer(RestoreWeaponTimerHandle, this, &UGA_ThrowAxe::RestoreHiddenWeapon, WeaponReappearDelay, false);
		}
	}

	bProjectileThrown = true;

	FRONTIER_LOG(Log, TEXT("Throw axe activated. Player=%s Projectile=%s WeaponType=%s WeaponElement=%d"),
		*GetNameSafe(PlayerCharacter),
		*GetNameSafe(Projectile),
		*WeaponTypeTag.ToString(),
		static_cast<int32>(ElementalType));
}

bool UGA_ThrowAxe::ResolveThrowContext(
	const FGameplayAbilityActorInfo* ActorInfo,
	AFrontierPlayerCharacter*& OutPlayerCharacter,
	USkeletalMesh*& OutWeaponMesh,
	TArray<UMaterialInterface*>& OutWeaponMaterials,
	FGameplayTag& OutWeaponTypeTag) const
{
	OutPlayerCharacter = ActorInfo ? Cast<AFrontierPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	OutWeaponMesh = nullptr;
	OutWeaponMaterials.Reset();
	OutWeaponTypeTag = FGameplayTag();

	if (!OutPlayerCharacter || !OutPlayerCharacter->HasAuthority() || OutPlayerCharacter->IsDead())
	{
		return false;
	}

	const UFrontierEquipmentComponent* EquipmentComponent = OutPlayerCharacter->GetEquipmentComponent();
	const AFrontierWeaponBase* CurrentWeapon = EquipmentComponent ? EquipmentComponent->GetCurrentWeapon() : nullptr;
	const UFrontierWeaponDataAsset* WeaponData = EquipmentComponent ? EquipmentComponent->GetCurrentWeaponData() : nullptr;
	const USkeletalMeshComponent* SourceWeaponMeshComponent = CurrentWeapon ? CurrentWeapon->GetWeaponMesh() : nullptr;
	if (!WeaponData || !CurrentWeapon || !SourceWeaponMeshComponent)
	{
		return false;
	}

	OutWeaponTypeTag = WeaponData->WeaponTypeTag;

	if (bRequireAxeWeapon && !OutWeaponTypeTag.MatchesTag(FFrontierGameplayTags::Get().WeaponTypeAxe))
	{
		FRONTIER_LOG(Warning, TEXT("Throw axe rejected because current weapon is not an axe. Player=%s WeaponType=%s"),
			*GetNameSafe(OutPlayerCharacter),
			*OutWeaponTypeTag.ToString());
		return false;
	}

	OutWeaponMesh = SourceWeaponMeshComponent->GetSkeletalMeshAsset();
	if (!OutWeaponMesh)
	{
		return false;
	}

	for (int32 MaterialIndex = 0; MaterialIndex < SourceWeaponMeshComponent->GetNumMaterials(); ++MaterialIndex)
	{
		OutWeaponMaterials.Add(SourceWeaponMeshComponent->GetMaterial(MaterialIndex));
	}

	return true;
}

void UGA_ThrowAxe::RestoreHiddenWeapon()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RestoreWeaponTimerHandle);
	}

	if (!bWeaponHiddenForThrow)
	{
		return;
	}

	if (CachedPlayerCharacter)
	{
		if (AFrontierWeaponBase* CurrentWeapon = CachedPlayerCharacter->GetCurrentWeapon())
		{
			CurrentWeapon->SetWeaponActive(true);
		}
	}

	bWeaponHiddenForThrow = false;
}

void UGA_ThrowAxe::ShowTrajectoryPreview(AFrontierPlayerCharacter* PlayerCharacter)
{
	if (!PlayerCharacter || !PlayerCharacter->HasAuthority() || !TrajectoryNiagaraSystem)
	{
		return;
	}

	AFrontierPlayerController* PlayerController = Cast<AFrontierPlayerController>(PlayerCharacter->GetController());
	if (!PlayerController)
	{
		return;
	}

	if (!ActiveTrajectoryId.IsValid())
	{
		ActiveTrajectoryId = FGuid::NewGuid();
	}

	TArray<FVector> TrajectoryPoints;
	BuildThrowTrajectoryPoints(PlayerCharacter, TrajectoryPoints);
	if (TrajectoryPoints.Num() < 2)
	{
		return;
	}

	PlayerController->ClientShowLocalSkillTrajectory(
		ActiveTrajectoryId,
		TrajectoryNiagaraSystem,
		TrajectoryPoints,
		TrajectoryPreviewDuration);
}

void UGA_ThrowAxe::UpdateTrajectoryPreview()
{
	if (!CachedPlayerCharacter || bProjectileThrown)
	{
		return;
	}

	ShowTrajectoryPreview(CachedPlayerCharacter);
}

void UGA_ThrowAxe::HideTrajectoryPreview()
{
	if (!CachedPlayerCharacter || !CachedPlayerCharacter->HasAuthority() || !ActiveTrajectoryId.IsValid())
	{
		return;
	}

	if (AFrontierPlayerController* PlayerController = Cast<AFrontierPlayerController>(CachedPlayerCharacter->GetController()))
	{
		PlayerController->ClientHideLocalSkillTrajectory(ActiveTrajectoryId);
	}

	ActiveTrajectoryId.Invalidate();
}

void UGA_ThrowAxe::BuildThrowTrajectoryPoints(const AFrontierPlayerCharacter* PlayerCharacter, TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	if (!PlayerCharacter)
	{
		return;
	}

	const FRotator YawOnlyRotation(0.0f, PlayerCharacter->GetActorRotation().Yaw, 0.0f);
	const FVector Forward = YawOnlyRotation.Vector().GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		return;
	}

	const FVector StartLocation = PlayerCharacter->GetActorLocation()
		+ Forward * (SpawnForwardOffset + TrajectoryStartForwardOffset)
		+ FVector(0.0f, 0.0f, SpawnHeightOffset);
	const float SafePreviewDistance = FMath::Max(TrajectoryPreviewDistance, 1.0f);
	const int32 SafePointCount = FMath::Max(2, TrajectoryPointCount);
	OutPoints.Reserve(SafePointCount);

	for (int32 PointIndex = 0; PointIndex < SafePointCount; ++PointIndex)
	{
		const float Alpha = SafePointCount > 1
			? static_cast<float>(PointIndex) / static_cast<float>(SafePointCount - 1)
			: 1.0f;
		OutPoints.Add(StartLocation + Forward * SafePreviewDistance * Alpha);
	}
}
