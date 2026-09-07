#include "GolemRangedAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "../ETC/GolemStoneProjectile.h"
#include "Character/FrontierBossEnemyCharacter.h"
#include "Frontier.h"
#include "Game/FrontierGameState.h"
#include "Tags/FrontierGameplayTags.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

UGolemRangedAttackAbility::UGolemRangedAttackAbility()
{
	FRONTIER_LOG_FUNC();
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.AbilityBossRangedSkill);
	SetAssetTags(AssetTags);
	
	ActivationOwnedTags.AddTag(Tags.StateCombatSkill);
	ActivationBlockedTags.AddTag(Tags.StateDead);
	ActivationBlockedTags.AddTag(Tags.StateCCStun);
	ActivationBlockedTags.AddTag(Tags.StateActionAttacking);
	ActivationBlockedTags.AddTag(Tags.StateCombatSkill);
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

bool UGolemRangedAttackAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	FRONTIER_LOG_FUNC();
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AFrontierBossEnemyCharacter* BossCharacter = ActorInfo ? Cast<AFrontierBossEnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	return BossCharacter && !BossCharacter->IsDead() && BossCharacter->HasCombatTarget();
}

void UGolemRangedAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	FRONTIER_LOG_FUNC();
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	CachedBossCharacter = ActorInfo ? Cast<AFrontierBossEnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!CachedBossCharacter || !AttackMontage)
	{
		FRONTIER_LOG(Warning, TEXT("Golem ranged attack activation failed. Boss=%s Montage=%s"),
			*GetNameSafe(CachedBossCharacter.Get()),
			*GetNameSafe(AttackMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FRONTIER_LOG(Warning, TEXT("Golem ranged attack activation failed because CommitAbility was rejected."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CachedBossCharacter->FaceCombatTarget();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		AttackMontage);

	if (!MontageTask)
	{
		FRONTIER_LOG(Warning, TEXT("Golem ranged attack activation failed because montage task creation failed."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UGolemRangedAttackAbility::HandleMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UGolemRangedAttackAbility::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGolemRangedAttackAbility::HandleMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &UGolemRangedAttackAbility::HandleMontageCancelled);
	MontageTask->ReadyForActivation();

	const float MontageLength = FMath::Max(AttackMontage->GetPlayLength(), 0.1f);
	if (UWorld* World = CachedBossCharacter->GetWorld())
	{
		World->GetTimerManager().SetTimer(
			MontageTimeoutTimerHandle,
			this,
			&UGolemRangedAttackAbility::HandleMontageTimedOut,
			MontageLength + MontageTimeoutBuffer,
			false);
	}
}

void UGolemRangedAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	FRONTIER_LOG(Log, TEXT("Golem ranged attack ability ended. Boss=%s WasCancelled=%d HeldProjectile=%s"),
		*GetNameSafe(CachedBossCharacter.Get()),
		bWasCancelled ? 1 : 0,
		*GetNameSafe(HeldProjectile.Get()));

	if (CachedBossCharacter)
	{
		if (UWorld* World = CachedBossCharacter->GetWorld())
		{
			World->GetTimerManager().ClearTimer(MontageTimeoutTimerHandle);
		}

		if (USkeletalMeshComponent* MeshComponent = CachedBossCharacter->GetMesh())
		{
			if (UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance())
			{
				AnimInstance->Montage_Stop(0.15f, AttackMontage);
			}
		}
	}

	if (bWasCancelled && HeldProjectile)
	{
		HeldProjectile->Destroy();
	}

	if (!bWarningTransferredToProjectile)
	{
		HideAttackWarning();
	}

	HeldProjectile = nullptr;
	CachedBossCharacter = nullptr;
	ActiveWarningId.Invalidate();
	bWarningTransferredToProjectile = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGolemRangedAttackAbility::HandleMontageCompleted()
{
	FRONTIER_LOG_FUNC();
	K2_EndAbility();
}

void UGolemRangedAttackAbility::HandleMontageCancelled()
{
	
	K2_CancelAbility();
}

void UGolemRangedAttackAbility::HandleMontageTimedOut()
{
	FRONTIER_LOG(Warning, TEXT("Golem ranged attack montage timed out. Cancelling ability. Boss=%s Montage=%s"),
		*GetNameSafe(CachedBossCharacter.Get()),
		*GetNameSafe(AttackMontage));

	K2_CancelAbility();
}

void UGolemRangedAttackAbility::ShowAttackWarning(const FVector& WarningLocation, const float WarningDuration)
{
	

	if (!CachedBossCharacter || !CachedBossCharacter->HasAuthority() || ActiveWarningId.IsValid())
	{
		return;
	}

	AFrontierGameState* FrontierGameState = CachedBossCharacter->GetWorld()
		? CachedBossCharacter->GetWorld()->GetGameState<AFrontierGameState>()
		: nullptr;
	if (!FrontierGameState)
	{
		return;
	}

	ActiveWarningId = FGuid::NewGuid();
	FAttackWarningData WarningData;
	WarningData.WarningLocation = WarningLocation;
	WarningData.WarningRotation = FRotator::ZeroRotator;
	WarningData.WarningRadius = WarningRadius;
	WarningData.WarningDuration = FMath::Max(WarningDuration, MinimumWarningDuration);
	WarningData.WarningShapeType = EAttackWarningShapeType::Circle;
	WarningData.WarningDecalMaterial = WarningDecalMaterial;
	WarningData.WarningNiagaraSystem = WarningNiagaraSystem;
	WarningData.bAttachToGround = true;
	WarningData.bDestroyOnImpact = true;
	FrontierGameState->MulticastShowAttackWarning(ActiveWarningId, WarningData);
}

void UGolemRangedAttackAbility::HideAttackWarning()
{
	if (!CachedBossCharacter || !CachedBossCharacter->HasAuthority() || !ActiveWarningId.IsValid())
	{
		return;
	}

	AFrontierGameState* FrontierGameState = CachedBossCharacter->GetWorld()
		? CachedBossCharacter->GetWorld()->GetGameState<AFrontierGameState>()
		: nullptr;
	if (FrontierGameState)
	{
		FrontierGameState->MulticastHideAttackWarning(ActiveWarningId);
	}
	ActiveWarningId.Invalidate();
}

void UGolemRangedAttackAbility::SpawnHeldProjectileFromNotify()
{
	

	if (!CachedBossCharacter || !RockProjectileClass || HeldProjectile)
	{
		FRONTIER_LOG(Warning, TEXT("Golem spawn projectile notify ignored. Boss=%s ProjectileClass=%s ExistingProjectile=%s"),
			*GetNameSafe(CachedBossCharacter.Get()),
			*GetNameSafe(RockProjectileClass.Get()),
			*GetNameSafe(HeldProjectile.Get()));
		return;
	}

	UWorld* World = CachedBossCharacter->GetWorld();
	USkeletalMeshComponent* MeshComponent = CachedBossCharacter->GetMesh();
	if (!World || !MeshComponent)
	{
		return;
	}

	const FVector SpawnLocation = GetSpawnLocation(CachedBossCharacter);
	const FRotator SpawnRotation = CachedBossCharacter->GetActorRotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = CachedBossCharacter;
	SpawnParams.Instigator = CachedBossCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	HeldProjectile = World->SpawnActor<AGolemStoneProjectile>(
		RockProjectileClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams);

	if (!HeldProjectile)
	{
		return;
	}

	const FGameplayTag DamageTypeTag = FFrontierGameplayTags::Get().DamageTypeBlunt.IsValid()
		? FFrontierGameplayTags::Get().DamageTypeBlunt
		: FFrontierGameplayTags::Get().DamageTypePhysical;

	HeldProjectile->InitProjectile(CachedBossCharacter, CachedBossCharacter, DamageEffectClass, Damage, DamageTypeTag);
	HeldProjectile->SetHeldProjectileState(true);
	HeldProjectile->AttachToComponent(
		MeshComponent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		ThrowSocketName);

	FRONTIER_LOG(Log, TEXT("Golem held projectile spawned. Boss=%s Projectile=%s Socket=%s"),
		*GetNameSafe(CachedBossCharacter.Get()),
		*GetNameSafe(HeldProjectile.Get()),
		*ThrowSocketName.ToString());
}

void UGolemRangedAttackAbility::ThrowHeldProjectileFromNotify()
{
	

	if (!CachedBossCharacter)
	{
		return;
	}

	if (!HeldProjectile)
	{
		SpawnHeldProjectileFromNotify();
	}

	if (!HeldProjectile)
	{
		return;
	}

	AActor* TargetActor = FindTargetActor();
	const FVector SpawnLocation = HeldProjectile->GetActorLocation();
	const FVector TargetLocation = GetTargetLocation(CachedBossCharacter, TargetActor);
	const float EstimatedTravelTime = ProjectileSpeed > 0.0f
		? FVector::Dist(SpawnLocation, TargetLocation) / ProjectileSpeed
		: MinimumWarningDuration;
	ShowAttackWarning(TargetLocation, EstimatedTravelTime);
	if (ActiveWarningId.IsValid())
	{
		HeldProjectile->SetImpactWarningId(ActiveWarningId);
		bWarningTransferredToProjectile = true;
		ActiveWarningId.Invalidate();
	}

	HeldProjectile->LaunchToTarget(TargetLocation, ProjectileSpeed);

	FRONTIER_LOG(Log, TEXT("Golem held projectile thrown. Boss=%s Target=%s Projectile=%s Spawn=%s TargetLocation=%s"),
		*GetNameSafe(CachedBossCharacter.Get()),
		*GetNameSafe(TargetActor),
		*GetNameSafe(HeldProjectile.Get()),
		*SpawnLocation.ToString(),
		*TargetLocation.ToString());

	HeldProjectile = nullptr;
}

AActor* UGolemRangedAttackAbility::FindTargetActor() const
{
	

	return CachedBossCharacter ? CachedBossCharacter->GetCombatTarget() : nullptr;
}

FVector UGolemRangedAttackAbility::GetSpawnLocation(AActor* AvatarActor) const
{
	if (!AvatarActor)
	{
		return FVector::ZeroVector;
	}

	ACharacter* Character = Cast<ACharacter>(AvatarActor);
	if (Character && Character->GetMesh())
	{
		if (Character->GetMesh()->DoesSocketExist(ThrowSocketName))
		{
			return Character->GetMesh()->GetSocketLocation(ThrowSocketName);
		}
	}

	return AvatarActor->GetActorLocation() + FVector(0.f, 0.f, 80.f);
}

FVector UGolemRangedAttackAbility::GetTargetLocation(AActor* AvatarActor, AActor* TargetActor) const
{
	if (TargetActor)
	{
		return TargetActor->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	}

	if (AvatarActor)
	{
		return AvatarActor->GetActorLocation()
			+ AvatarActor->GetActorForwardVector() * ForwardFallbackDistance;
	}

	return FVector::ZeroVector;
}
