#include "AbilitySystem/Abilities/GolemAbilities/Ability/GA_BossDashAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/FrontierBaseCharacter.h"
#include "Character/FrontierBossEnemyCharacter.h"
#include "Combat/FrontierDamageStatics.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Frontier.h"
#include "Game/FrontierGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Tags/FrontierGameplayTags.h"

UGA_BossDashAttack::UGA_BossDashAttack()
{
	FRONTIER_LOG_FUNC();
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.AbilityBossDashSkill);
	SetAssetTags(AssetTags);
	
	ActivationOwnedTags.AddTag(Tags.StateCombatSkill);
	ActivationOwnedTags.AddTag(Tags.StateCombatSkillDash);
	ActivationBlockedTags.AddTag(Tags.StateDead);
	ActivationBlockedTags.AddTag(Tags.StateCCStun);
	ActivationBlockedTags.AddTag(Tags.StateActionAttacking);
	ActivationBlockedTags.AddTag(Tags.StateCombatSkill);

	DamageTypeTag = Tags.DamageTypeBlunt;
}

bool UGA_BossDashAttack::CanActivateAbility(
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
	return BossCharacter
		&& BossCharacter->HasAuthority()
		&& !BossCharacter->IsDead()
		&& BossCharacter->HasCombatTarget()
		&& !BossCharacter->IsBossActionPlaying();
}

void UGA_BossDashAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	FRONTIER_LOG_FUNC();
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	CachedBossCharacter = ActorInfo ? Cast<AFrontierBossEnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!CachedBossCharacter || !DashMontage)
	{
		FRONTIER_LOG(Warning, TEXT("Boss dash activation failed. Boss=%s Montage=%s"),
			*GetNameSafe(CachedBossCharacter.Get()),
			*GetNameSafe(DashMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	StartLocation = CachedBossCharacter->GetActorLocation();
	if (!ResolveImpactLocation(ImpactLocation))
	{
		FRONTIER_LOG(Warning, TEXT("Boss dash activation failed because impact location could not be resolved. Boss=%s"),
			*GetNameSafe(CachedBossCharacter.Get()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FRONTIER_LOG(Warning, TEXT("Boss dash activation failed because CommitAbility was rejected."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ShowAttackWarning();

	const FVector Direction = (ImpactLocation - StartLocation).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		CachedBossCharacter->SetActorRotation(Direction.Rotation());
	}

	CachedBossCharacter->OnArcMoveFinished.AddDynamic(this, &UGA_BossDashAttack::HandleArcMoveFinished);

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		DashMontage);
	if (!MontageTask)
	{
		FRONTIER_LOG(Warning, TEXT("Boss dash activation failed because montage task creation failed."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UGA_BossDashAttack::HandleMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UGA_BossDashAttack::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_BossDashAttack::HandleMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_BossDashAttack::HandleMontageCancelled);
	MontageTask->ReadyForActivation();

	FRONTIER_LOG(Log, TEXT("Boss dash activated and waiting for anim notify. Boss=%s Start=%s Impact=%s Duration=%.2f"),
		*GetNameSafe(CachedBossCharacter.Get()),
		*StartLocation.ToCompactString(),
		*ImpactLocation.ToCompactString(),
		DashDuration);
}

void UGA_BossDashAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	FRONTIER_LOG(Log, TEXT("Boss dash ability ended. Boss=%s WasCancelled=%d DamageApplied=%d"),
		*GetNameSafe(CachedBossCharacter.Get()),
		bWasCancelled ? 1 : 0,
		bImpactDamageApplied ? 1 : 0);

	if (CachedBossCharacter)
	{
		HideAttackWarning();
		CachedBossCharacter->OnArcMoveFinished.RemoveDynamic(this, &UGA_BossDashAttack::HandleArcMoveFinished);
		if (bWasCancelled)
		{
			CachedBossCharacter->StopArcMove(true);
		}
	}

	CachedBossCharacter = nullptr;
	bImpactDamageApplied = false;
	bDashMoveStarted = false;
	ActiveWarningId.Invalidate();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_BossDashAttack::StartDashMoveFromNotify()
{
	FRONTIER_LOG_FUNC();
	if (bDashMoveStarted)
	{
		FRONTIER_LOG(Log, TEXT("Boss dash move notify ignored because movement already started. Boss=%s"),
			*GetNameSafe(CachedBossCharacter.Get()));
		return;
	}

	if (!CachedBossCharacter || !CachedBossCharacter->HasAuthority())
	{
		return;
	}

	bDashMoveStarted = true;
	if (!CachedBossCharacter->StartArcMoveToLocation(StartLocation, ImpactLocation, JumpHeight, DashDuration))
	{
		FRONTIER_LOG(Warning, TEXT("Boss dash move notify failed because arc movement could not start. Boss=%s"),
			*GetNameSafe(CachedBossCharacter.Get()));
		K2_CancelAbility();
		return;
	}

	FRONTIER_LOG(Log, TEXT("Boss dash movement started from anim notify. Boss=%s Start=%s Impact=%s Duration=%.2f"),
		*GetNameSafe(CachedBossCharacter.Get()),
		*StartLocation.ToCompactString(),
		*ImpactLocation.ToCompactString(),
		DashDuration);
}

void UGA_BossDashAttack::HandleMontageCompleted()
{
	FRONTIER_LOG_FUNC();
	if (!bDashMoveStarted)
	{
		FRONTIER_LOG(Warning, TEXT("Boss dash montage completed before dash movement notify. Cancelling ability. Boss=%s"),
			*GetNameSafe(CachedBossCharacter.Get()));
		K2_CancelAbility();
	}
}

void UGA_BossDashAttack::HandleMontageCancelled()
{
	FRONTIER_LOG_FUNC();
	K2_CancelAbility();
}

void UGA_BossDashAttack::HandleArcMoveFinished(const bool bInterrupted)
{
	FRONTIER_LOG(Log, TEXT("Boss dash arc move finished. Boss=%s Interrupted=%d"),
		*GetNameSafe(CachedBossCharacter.Get()),
		bInterrupted ? 1 : 0);

	if (bInterrupted)
	{
		K2_CancelAbility();
		return;
	}

	ApplyImpactDamage();
	K2_EndAbility();
}

void UGA_BossDashAttack::HandleLandingFromNotify()
{
	if (!CachedBossCharacter || !CachedBossCharacter->HasAuthority() || !bDashMoveStarted)
	{
		return;
	}

	if (!CachedBossCharacter->IsArcMoveReadyForLandingNotify())
	{
		return;
	}

	if (!CachedBossCharacter->PrepareArcMoveLandingFromNotify())
	{
		return;
	}

	// Resolve the impact sphere while character collision is still ignored.
	ApplyImpactDamage();
	CachedBossCharacter->CompleteArcMoveFromNotify();
}

bool UGA_BossDashAttack::ResolveImpactLocation(FVector& OutImpactLocation) const
{
	FRONTIER_LOG_FUNC();
	if (!CachedBossCharacter || !CachedBossCharacter->HasCombatTarget() || !CachedBossCharacter->GetWorld())
	{
		return false;
	}

	const AActor* TargetActor = CachedBossCharacter->GetCombatTarget();
	const float CapsuleHalfHeight = CachedBossCharacter->GetCapsuleComponent()
		? CachedBossCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		: 0.0f;

	FVector DesiredImpactLocation = TargetActor->GetActorLocation()
		+ TargetActor->GetVelocity() * FVector(1.0f, 1.0f, 0.0f) * TargetPredictionTime;
	const FVector ToTarget = DesiredImpactLocation - StartLocation;
	const FVector ToTarget2D(ToTarget.X, ToTarget.Y, 0.0f);
	if (ToTarget2D.Size() > MaxDashDistance)
	{
		DesiredImpactLocation = StartLocation + ToTarget2D.GetSafeNormal() * MaxDashDistance;
	}

	UWorld* World = CachedBossCharacter->GetWorld();
	if (UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		FNavLocation ProjectedLocation;
		if (NavigationSystem->ProjectPointToNavigation(DesiredImpactLocation, ProjectedLocation, FVector(300.0f, 300.0f, 800.0f)))
		{
			DesiredImpactLocation = ProjectedLocation.Location;
		}
	}

	const FVector TraceStart = DesiredImpactLocation + FVector(0.0f, 0.0f, 1000.0f);
	const FVector TraceEnd = DesiredImpactLocation - FVector(0.0f, 0.0f, 3000.0f);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(BossDashImpactFloorTrace),
		false,
		CachedBossCharacter);

	FHitResult FloorHit;

	if (World->LineTraceSingleByObjectType(
		FloorHit,
		TraceStart,
		TraceEnd,
		ObjectQueryParams,
		QueryParams))
	{
		DesiredImpactLocation =
			FloorHit.Location
			+ FVector(0.0f, 0.0f, CapsuleHalfHeight);
	}

	OutImpactLocation = DesiredImpactLocation;
	FRONTIER_LOG(Log, TEXT("Resolved boss dash impact actor location. Impact=%s CapsuleHalfHeight=%.2f"),
		*OutImpactLocation.ToCompactString(),
		CapsuleHalfHeight);
	return true;
}

FVector UGA_BossDashAttack::GetWarningLocation() const
{
	if (!CachedBossCharacter || !CachedBossCharacter->GetCapsuleComponent())
	{
		return ImpactLocation;
	}

	const float CapsuleHalfHeight = CachedBossCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	return ImpactLocation - FVector(0.0f, 0.0f, CapsuleHalfHeight - 5.0f);
}

void UGA_BossDashAttack::ShowAttackWarning()
{
	FRONTIER_LOG_FUNC();
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
	WarningData.WarningLocation = GetWarningLocation();
	WarningData.WarningRotation = FRotator::ZeroRotator;
	WarningData.WarningRadius = ImpactRadius;
	WarningData.WarningDuration = FMath::Max(DashDuration + 1.0f, 1.0f);
	WarningData.WarningShapeType = EAttackWarningShapeType::Circle;
	WarningData.WarningDecalMaterial = WarningDecalMaterial;
	WarningData.WarningNiagaraSystem = WarningNiagaraSystem;
	WarningData.bAttachToGround = true;
	WarningData.bDestroyOnImpact = true;
	FrontierGameState->MulticastShowAttackWarning(ActiveWarningId, WarningData);
}

void UGA_BossDashAttack::HideAttackWarning()
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

void UGA_BossDashAttack::ApplyImpactDamage()
{
	FRONTIER_LOG_FUNC();
	if (bImpactDamageApplied || !CachedBossCharacter || !CachedBossCharacter->HasAuthority())
	{
		return;
	}
	bImpactDamageApplied = true;
	HideAttackWarning();

	UWorld* World = CachedBossCharacter->GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel3);

	FCollisionShape CollisionShape = FCollisionShape::MakeSphere(ImpactRadius);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossDashImpactOverlap), false, CachedBossCharacter);
	World->OverlapMultiByObjectType(
		OverlapResults,
		ImpactLocation,
		FQuat::Identity,
		ObjectQueryParams,
		CollisionShape,
		QueryParams);

	TSet<AActor*> DamagedActors;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* HitActor = OverlapResult.GetActor();
		AFrontierBaseCharacter* TargetCharacter = Cast<AFrontierBaseCharacter>(HitActor);
		if (!TargetCharacter
			|| TargetCharacter == CachedBossCharacter
			|| TargetCharacter->GetTeam() == EFrontierTeam::Monster
			|| DamagedActors.Contains(HitActor))
		{
			continue;
		}

		FFrontierDamageRequest DamageRequest;
		DamageRequest.BaseDamage = ImpactDamage;
		DamageRequest.DamageMultiplier = 1.0f;
		DamageRequest.DamageTypeTag = DamageTypeTag.IsValid() ? DamageTypeTag : FFrontierGameplayTags::Get().DamageTypeBlunt;
		DamageRequest.DamageEffectClass = DamageEffectClass;
		const FFrontierDamageResult DamageResult = UFrontierDamageStatics::ApplyDamage(CachedBossCharacter, TargetCharacter, DamageRequest);
		if (DamageResult.bApplied)
		{
			DamagedActors.Add(HitActor);
		}
	}

	if (bDrawDebugImpact)
	{
		DrawDebugSphere(World, ImpactLocation, ImpactRadius, 24, FColor::Red, false, 2.0f);
	}

	FRONTIER_LOG(Log, TEXT("Boss dash impact damage finished. Boss=%s ImpactLocation=%s Radius=%.2f DamagedCount=%d"),
		*GetNameSafe(CachedBossCharacter.Get()),
		*ImpactLocation.ToCompactString(),
		ImpactRadius,
		DamagedActors.Num());
}
