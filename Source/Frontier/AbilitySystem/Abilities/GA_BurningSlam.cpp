#include "AbilitySystem/Abilities/GA_BurningSlam.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Combat/FrontierDamageStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/FrontierCombatComponent.h"
#include "DrawDebugHelpers.h"
#include "Frontier.h"
#include "FrontierPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "Skill/FrontierSkillDataSubsystem.h"
#include "Tags/FrontierGameplayTags.h"

UGA_BurningSlam::UGA_BurningSlam()
{
	FRONTIER_LOG_FUNC();
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	ActivationOwnedTags.AddTag(Tags.StateCombatSkill);
	ActivationBlockedTags.AddTag(Tags.StateDead);
	ActivationBlockedTags.AddTag(Tags.StateCCStun);
	ActivationBlockedTags.AddTag(Tags.StateActionAttacking);
	ActivationBlockedTags.AddTag(Tags.StateCombatSkill);

	DamageTypeTag = Tags.DamageTypeBlunt;
	HitReactionTag = Tags.HitReactionKnockback;
	ElementalType = EFrontierElementalType::Fire;
}

bool UGA_BurningSlam::CanActivateAbility(
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

	const AFrontierPlayerCharacter* PlayerCharacter = ActorInfo ? Cast<AFrontierPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	return PlayerCharacter && PlayerCharacter->HasAuthority() && !PlayerCharacter->IsDead();
}

void UGA_BurningSlam::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	FRONTIER_LOG_FUNC();
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	CachedPlayerCharacter = ActorInfo ? Cast<AFrontierPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!CachedPlayerCharacter || !SlamMontage)
	{
		FRONTIER_LOG(Warning, TEXT("BurningSlam activation failed. Player=%s Montage=%s"),
			*GetNameSafe(CachedPlayerCharacter.Get()),
			*GetNameSafe(SlamMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	StartLocation = CachedPlayerCharacter->GetActorLocation();

	const int32 SkillLevel = FMath::Max(1, GetAbilityLevel(Handle, ActorInfo));
	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	TMap<FGameplayTag, float> Parameters;
	Parameters.Add(Tags.SkillParameterTravelDistance, MaxTargetDistance);
	Parameters.Add(Tags.SkillParameterRadius, ImpactRadius);
	Parameters.Add(Tags.SkillParameterDamageMultiplier, DamageMultiplier);
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (const UFrontierSkillDataSubsystem* SkillData = GameInstance->GetSubsystem<UFrontierSkillDataSubsystem>())
		{
			SkillData->ApplySkillBalance(GetClass(), SkillLevel, Parameters);
		}
	}

	ResolvedImpactRadius = FMath::Max(0.0f, Parameters.FindRef(Tags.SkillParameterRadius));
	ResolvedDamageMultiplier = FMath::Max(0.0f, Parameters.FindRef(Tags.SkillParameterDamageMultiplier));
	ResolvedMaxTargetDistance = FMath::Max(
		MinTargetDistance,
		Parameters.FindRef(Tags.SkillParameterTravelDistance));
	CurrentTargetDistance = FMath::Clamp(
		TargetDistance,
		MinTargetDistance,
		ResolvedMaxTargetDistance);
	if (!ResolveTargetLocation(TargetActorLocation, TargetGroundLocation))
	{
		FRONTIER_LOG(Warning, TEXT("BurningSlam activation failed because target location could not be resolved. Player=%s"),
			*GetNameSafe(CachedPlayerCharacter.Get()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ShowTargetWarning();
	ShowOrUpdateTrajectoryPreview();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ConfirmTimeoutTimerHandle,
			this,
			&UGA_BurningSlam::HandleMontageCancelled,
			ConfirmTimeout,
			false);

		World->GetTimerManager().SetTimer(
			TargetingUpdateTimerHandle,
			this,
			&UGA_BurningSlam::TickTargeting,
			TargetingUpdateInterval,
			true);
	}

	FRONTIER_LOG(Log, TEXT("BurningSlam targeting started. Player=%s TargetActorLocation=%s TargetGroundLocation=%s Distance=%.2f Radius=%.2f"),
		*GetNameSafe(CachedPlayerCharacter.Get()),
		*TargetActorLocation.ToCompactString(),
		*TargetGroundLocation.ToCompactString(),
		CurrentTargetDistance,
		ResolvedImpactRadius);
}

void UGA_BurningSlam::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	FRONTIER_LOG(Log, TEXT("BurningSlam ended. Player=%s Cancelled=%d Confirmed=%d DamageApplied=%d"),
		*GetNameSafe(CachedPlayerCharacter.Get()),
		bWasCancelled ? 1 : 0,
		bConfirmed ? 1 : 0,
		bImpactDamageApplied ? 1 : 0);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MovementTimerHandle);
		World->GetTimerManager().ClearTimer(ConfirmTimeoutTimerHandle);
		World->GetTimerManager().ClearTimer(TargetingUpdateTimerHandle);
	}

	HideTargetWarning();
	HideTrajectoryPreview();
	RestoreMovementMode();
	RestoreSlamCollisionOverrides();
	RestoreAnimationMode();
	if (bWasCancelled && CachedPlayerCharacter)
	{
		CachedPlayerCharacter->ClearMontageHitReactionResistance(SlamMontage);
	}

	CachedPlayerCharacter = nullptr;
	StartLocation = FVector::ZeroVector;
	TargetActorLocation = FVector::ZeroVector;
	TargetGroundLocation = FVector::ZeroVector;
	MovementStartTimeSeconds = 0.0f;
	CurrentTargetDistance = 0.0f;
	ResolvedMaxTargetDistance = 0.0f;
	ResolvedImpactRadius = 0.0f;
	ResolvedDamageMultiplier = 1.0f;
	bConfirmed = false;
	bImpactDamageApplied = false;
	ActiveWarningId.Invalidate();
	ActiveTrajectoryId.Invalidate();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_BurningSlam::OnAreaSkillConfirmed()
{
	FRONTIER_LOG_FUNC();
	if (!CachedPlayerCharacter || !CachedPlayerCharacter->HasAuthority())
	{
		return;
	}

	bConfirmed = true;
	CachedPlayerCharacter->SetCurrentAttackAnimationModeInternal(EFrontierAttackAnimationMode::FullBody);
	bAnimationModeOverridden = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ConfirmTimeoutTimerHandle);
		World->GetTimerManager().ClearTimer(TargetingUpdateTimerHandle);
	}

	ShowOrUpdateTrajectoryPreview();
	HideTargetWarning();
	HideTrajectoryPreview();

	const FVector Direction = (TargetActorLocation - CachedPlayerCharacter->GetActorLocation()).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		CachedPlayerCharacter->SetActorRotation(Direction.Rotation());
	}

	CachedPlayerCharacter->SetMontageHitReactionResistance(SlamMontage, HitReactionResistance);
	CachedPlayerCharacter->MulticastPlaySkillMontage(SlamMontage, EFrontierAttackAnimationMode::FullBody);

	StartSlamMovement();
}

void UGA_BurningSlam::OnAreaSkillCancelled()
{
}

void UGA_BurningSlam::AdjustAreaTargetDistanceInput(const float InputAxis)
{
	if (bConfirmed || !CachedPlayerCharacter || !CachedPlayerCharacter->HasAuthority())
	{
		return;
	}

	if (FMath::IsNearlyZero(InputAxis))
	{
		return;
	}

	const float SafeMaxDistance = FMath::Max(
		MinTargetDistance,
		ResolvedMaxTargetDistance > 0.0f ? ResolvedMaxTargetDistance : MaxTargetDistance);
	CurrentTargetDistance = FMath::Clamp(
		CurrentTargetDistance - (InputAxis * TargetDistanceInputScale),
		MinTargetDistance,
		SafeMaxDistance);

	if (RefreshTargetLocation())
	{
		UpdateTargetWarning();
		ShowOrUpdateTrajectoryPreview();
	}
}

void UGA_BurningSlam::HandleMontageCompleted()
{
	FRONTIER_LOG_FUNC();
}

void UGA_BurningSlam::HandleMontageCancelled()
{
	FRONTIER_LOG_FUNC();
	K2_CancelAbility();
}

bool UGA_BurningSlam::CanConfirmAreaSkill()
{
	if (!Super::CanConfirmAreaSkill())
	{
		return false;
	}

	if (!CachedPlayerCharacter || !CachedPlayerCharacter->HasAuthority())
	{
		return false;
	}

	const UCharacterMovementComponent* MovementComponent = CachedPlayerCharacter->GetCharacterMovement();
	if (MovementComponent && MovementComponent->IsFalling())
	{
		FRONTIER_LOG(Log, TEXT("BurningSlam confirm rejected because player is falling. Player=%s"),
			*GetNameSafe(CachedPlayerCharacter.Get()));
		return false;
	}

	if (!ResolveTargetLocation(TargetActorLocation, TargetGroundLocation))
	{
		FRONTIER_LOG(Log, TEXT("BurningSlam confirm rejected because target location is invalid. Player=%s"),
			*GetNameSafe(CachedPlayerCharacter.Get()));
		return false;
	}

	return true;
}

bool UGA_BurningSlam::ResolveTargetLocation(
	FVector& OutActorLocation,
	FVector& OutGroundLocation) const
{
	if (!CachedPlayerCharacter || !CachedPlayerCharacter->GetWorld())
	{
		return false;
	}

	const float SafeMaxDistance = FMath::Max(
		MinTargetDistance,
		ResolvedMaxTargetDistance > 0.0f ? ResolvedMaxTargetDistance : MaxTargetDistance);

	const float ResolvedTargetDistance = FMath::Clamp(
		CurrentTargetDistance > 0.0f
			? CurrentTargetDistance
			: TargetDistance,
		MinTargetDistance,
		SafeMaxDistance
	);

	FFrontierAreaSkillTargetLocationParams TargetLocationParams;
	TargetLocationParams.SourceCharacter = CachedPlayerCharacter;
	TargetLocationParams.TargetDistance = ResolvedTargetDistance;
	TargetLocationParams.MinTargetDistance = MinTargetDistance;
	TargetLocationParams.MaxTargetDistance = SafeMaxDistance;
	TargetLocationParams.AreaRadius = ResolvedImpactRadius;
	TargetLocationParams.bCanOver = bCanOver;
	TargetLocationParams.bUseNavigationProjection = false;
	TargetLocationParams.MaxAllowedHeightDifference = 150.0f;
	TargetLocationParams.NavigationProjectionExtent = FVector(200.0f, 200.0f, 500.0f);
	TargetLocationParams.FloorTraceUpDistance = 1000.0f;
	TargetLocationParams.FloorTraceDownDistance = 3000.0f;
	TargetLocationParams.MinFloorNormalZ = 0.5f;
	TargetLocationParams.ObstacleFrontClearance = 10.0f;
	TargetLocationParams.ObstacleTraceObjectType = ECC_WorldStatic;
	TargetLocationParams.FloorTraceObjectType = ECC_WorldStatic;
	TargetLocationParams.TraceTag = TEXT("BurningSlamTargetLocationTrace");

	return ResolveAreaSkillTargetLocation(TargetLocationParams, OutActorLocation, OutGroundLocation);
}

void UGA_BurningSlam::ShowTargetWarning()
{
	FRONTIER_LOG_FUNC();
	if (!CachedPlayerCharacter || !CachedPlayerCharacter->HasAuthority() || ActiveWarningId.IsValid())
	{
		return;
	}

	AFrontierPlayerController* PlayerController = Cast<AFrontierPlayerController>(CachedPlayerCharacter->GetController());
	if (!PlayerController)
	{
		return;
	}

	ActiveWarningId = FGuid::NewGuid();
	FAttackWarningData WarningData;
	WarningData.WarningLocation = TargetGroundLocation + FVector(0.0f, 0.0f, 5.0f);
	WarningData.WarningRotation = FRotator::ZeroRotator;
	WarningData.WarningRadius = ResolvedImpactRadius;
	WarningData.WarningNiagaraReferenceRadius = ImpactRadius;
	WarningData.WarningDuration = FMath::Max(ConfirmTimeout + SlamMoveDuration + 1.0f, 1.0f);
	WarningData.WarningShapeType = EAttackWarningShapeType::Circle;
	WarningData.WarningDecalMaterial = WarningDecalMaterial;
	WarningData.WarningNiagaraSystem = WarningNiagaraSystem;
	WarningData.bAttachToGround = true;
	WarningData.bDestroyOnImpact = true;
	PlayerController->ClientShowLocalAttackWarning(ActiveWarningId, WarningData);
}

void UGA_BurningSlam::UpdateTargetWarning()
{
	if (!CachedPlayerCharacter || !CachedPlayerCharacter->HasAuthority() || !ActiveWarningId.IsValid())
	{
		return;
	}

	AFrontierPlayerController* PlayerController = Cast<AFrontierPlayerController>(CachedPlayerCharacter->GetController());
	if (!PlayerController)
	{
		return;
	}

	FAttackWarningData WarningData;
	WarningData.WarningLocation = TargetGroundLocation + FVector(0.0f, 0.0f, 5.0f);
	WarningData.WarningRotation = FRotator::ZeroRotator;
	WarningData.WarningRadius = ResolvedImpactRadius;
	WarningData.WarningNiagaraReferenceRadius = ImpactRadius;
	WarningData.WarningDuration = FMath::Max(ConfirmTimeout + SlamMoveDuration + 1.0f, 1.0f);
	WarningData.WarningShapeType = EAttackWarningShapeType::Circle;
	WarningData.WarningDecalMaterial = WarningDecalMaterial;
	WarningData.WarningNiagaraSystem = WarningNiagaraSystem;
	WarningData.bAttachToGround = true;
	WarningData.bDestroyOnImpact = true;
	PlayerController->ClientShowLocalAttackWarning(ActiveWarningId, WarningData);
}

void UGA_BurningSlam::HideTargetWarning()
{
	if (!CachedPlayerCharacter || !CachedPlayerCharacter->HasAuthority() || !ActiveWarningId.IsValid())
	{
		return;
	}

	AFrontierPlayerController* PlayerController = Cast<AFrontierPlayerController>(CachedPlayerCharacter->GetController());
	if (PlayerController)
	{
		PlayerController->ClientHideLocalAttackWarning(ActiveWarningId);
	}
	ActiveWarningId.Invalidate();
}

void UGA_BurningSlam::ShowOrUpdateTrajectoryPreview()
{
	if (!CachedPlayerCharacter || !CachedPlayerCharacter->HasAuthority() || !TrajectoryNiagaraSystem)
	{
		return;
	}

	AFrontierPlayerController* PlayerController = Cast<AFrontierPlayerController>(CachedPlayerCharacter->GetController());
	if (!PlayerController)
	{
		return;
	}

	if (!ActiveTrajectoryId.IsValid())
	{
		ActiveTrajectoryId = FGuid::NewGuid();
	}

	TArray<FVector> TrajectoryPoints;
	BuildSlamTrajectoryPoints(TrajectoryPoints);
	if (TrajectoryPoints.Num() < 2)
	{
		return;
	}

	const float PreviewDuration = FMath::Max(ConfirmTimeout + SlamMoveDuration + 1.0f, 1.0f);
	PlayerController->ClientShowLocalSkillTrajectory(ActiveTrajectoryId, TrajectoryNiagaraSystem, TrajectoryPoints, PreviewDuration);
}

void UGA_BurningSlam::HideTrajectoryPreview()
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

void UGA_BurningSlam::BuildSlamTrajectoryPoints(TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();

	if (!CachedPlayerCharacter)
	{
		return;
	}

	const FVector PreviewForward = CachedPlayerCharacter->GetActorForwardVector().GetSafeNormal2D();
	const FVector PreviewStartLocation = CachedPlayerCharacter->GetActorLocation()
		+ PreviewForward * TrajectoryStartForwardOffset;
	const int32 SafePointCount = FMath::Max(2, TrajectoryPointCount);
	OutPoints.Reserve(SafePointCount);

	for (int32 PointIndex = 0; PointIndex < SafePointCount; ++PointIndex)
	{
		const float Alpha = SafePointCount > 1
			? static_cast<float>(PointIndex) / static_cast<float>(SafePointCount - 1)
			: 1.0f;
		OutPoints.Add(EvaluateSlamMovementLocation(PreviewStartLocation, TargetActorLocation, Alpha));
	}
}

FVector UGA_BurningSlam::EvaluateSlamMovementLocation(
	const FVector& PathStart,
	const FVector& PathTarget,
	const float Alpha) const
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	const float SafeHorizontalEnd = FMath::Clamp(HorizontalTravelEndFraction, 0.1f, 1.0f);
	const float SafeApexFraction = FMath::Clamp(SlamApexFraction, 0.1f, 0.9f);

	// Reach the target quickly, then keep the final part of the movement for the slam.
	const float TravelAlpha = FMath::Clamp(ClampedAlpha / SafeHorizontalEnd, 0.0f, 1.0f);
	const float HorizontalAlpha = 1.0f - (1.0f - TravelAlpha) * (1.0f - TravelAlpha);
	const FVector BaseLocation = FMath::Lerp(PathStart, PathTarget, HorizontalAlpha);

	float ArcZ = 0.0f;
	if (ClampedAlpha <= SafeApexFraction)
	{
		const float AscentAlpha = ClampedAlpha / SafeApexFraction;
		ArcZ = FMath::Sin(AscentAlpha * HALF_PI) * JumpHeight;
	}
	else
	{
		const float DescentAlpha = FMath::Clamp(
			(ClampedAlpha - SafeApexFraction) / (1.0f - SafeApexFraction),
			0.0f,
			1.0f);

		// Quadratic descent reaches its highest downward speed at impact.
		ArcZ = JumpHeight * (1.0f - DescentAlpha * DescentAlpha);
	}

	return BaseLocation + FVector(0.0f, 0.0f, ArcZ);
}

void UGA_BurningSlam::TickTargeting()
{
	if (bConfirmed)
	{
		return;
	}

	if (RefreshTargetLocation())
	{
		UpdateTargetWarning();
		ShowOrUpdateTrajectoryPreview();
	}
}

bool UGA_BurningSlam::RefreshTargetLocation()
{
	return ResolveTargetLocation(TargetActorLocation, TargetGroundLocation);
}

void UGA_BurningSlam::StartSlamMovement()
{
	FRONTIER_LOG_FUNC();
	if (!CachedPlayerCharacter || !CachedPlayerCharacter->HasAuthority())
	{
		return;
	}

	StartLocation = CachedPlayerCharacter->GetActorLocation();
	MovementStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	if (UCharacterMovementComponent* MovementComponent = CachedPlayerCharacter->GetCharacterMovement())
	{
		CachedMovementMode = MovementComponent->MovementMode;
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	ApplySlamCollisionOverrides();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(MovementTimerHandle, this, &UGA_BurningSlam::TickSlamMovement, MovementTickInterval, true);
	}
}

void UGA_BurningSlam::TickSlamMovement()
{
	if (!CachedPlayerCharacter || !CachedPlayerCharacter->HasAuthority() || !GetWorld())
	{
		FinishSlamMovement(true);
		return;
	}

	const float ElapsedTime = GetWorld()->GetTimeSeconds() - MovementStartTimeSeconds;
	const float Alpha = FMath::Clamp(ElapsedTime / FMath::Max(SlamMoveDuration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	const FVector NewLocation = EvaluateSlamMovementLocation(StartLocation, TargetActorLocation, Alpha);

	FHitResult SweepHit;
	CachedPlayerCharacter->SetActorLocation(NewLocation, true, &SweepHit);
	if (SweepHit.bBlockingHit)
	{
		FRONTIER_LOG(Log, TEXT("BurningSlam movement blocked by world collision. Player=%s HitActor=%s HitLocation=%s"),
			*GetNameSafe(CachedPlayerCharacter.Get()),
			*GetNameSafe(SweepHit.GetActor()),
			*SweepHit.ImpactPoint.ToCompactString());
		TargetActorLocation = CachedPlayerCharacter->GetActorLocation();
		TargetGroundLocation = SweepHit.ImpactPoint;
		FinishSlamMovement(false);
		return;
	}

	if (Alpha >= 1.0f)
	{
		FinishSlamMovement(false);
	}
}

void UGA_BurningSlam::FinishSlamMovement(const bool bInterrupted)
{
	FRONTIER_LOG(Log, TEXT("BurningSlam movement finished. Player=%s Interrupted=%d"),
		*GetNameSafe(CachedPlayerCharacter.Get()),
		bInterrupted ? 1 : 0);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MovementTimerHandle);
	}

	RestoreMovementMode();
	RestoreSlamCollisionOverrides();

	if (bInterrupted)
	{
		K2_CancelAbility();
		return;
	}

	if (CachedPlayerCharacter)
	{
		CachedPlayerCharacter->SetActorLocation(TargetActorLocation, true);
	}

	ApplyImpactDamage();
	K2_EndAbility();
}

void UGA_BurningSlam::ApplyImpactDamage()
{
	FRONTIER_LOG_FUNC();
	if (bImpactDamageApplied || !CachedPlayerCharacter || !CachedPlayerCharacter->HasAuthority())
	{
		return;
	}
	bImpactDamageApplied = true;
	HideTargetWarning();

	UFrontierCombatComponent* CombatComponent = CachedPlayerCharacter->GetCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	const FVector TraceStart = TargetGroundLocation + FVector(0.0f, 0.0f, 80.0f);
	const FVector TraceEnd = TargetGroundLocation - FVector(0.0f, 0.0f, 80.0f);
	CombatComponent->ResetHitActorsThisAttack();
	CombatComponent->ApplySphereTraceDamageBetweenPoints(
		TraceStart,
		TraceEnd,
		ResolvedImpactRadius,
		BaseDamage,
		DamageTypeTag.IsValid() ? DamageTypeTag : FFrontierGameplayTags::Get().DamageTypeBlunt,
		DamageEffectClass,
		nullptr,
		ResolvedDamageMultiplier,
		bDrawDebugImpact,
		ElementalType,
		HitReactionTag,
		HitReactionLevel,
		StaggerDuration,
		KnockbackHorizontalStrength,
		KnockbackVerticalStrength);

	if (bDrawDebugImpact && CachedPlayerCharacter->GetWorld())
	{
		DrawDebugSphere(CachedPlayerCharacter->GetWorld(), TargetGroundLocation, ResolvedImpactRadius, 24, FColor::Orange, false, 2.0f);
	}
}

void UGA_BurningSlam::ApplySlamCollisionOverrides()
{
	if (bSlamCollisionResponsesOverridden || !CachedPlayerCharacter)
	{
		return;
	}

	UCapsuleComponent* CapsuleComponent = CachedPlayerCharacter->GetCapsuleComponent();
	if (!CapsuleComponent)
	{
		return;
	}

	CachedEnemyCollisionResponse = CapsuleComponent->GetCollisionResponseToChannel(ECC_GameTraceChannel2);
	CachedPlayerCollisionResponse = CapsuleComponent->GetCollisionResponseToChannel(ECC_GameTraceChannel3);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Ignore);
	bSlamCollisionResponsesOverridden = true;

	FRONTIER_LOG(Log, TEXT("BurningSlam collision override applied. Player=%s EnemyResponse=%d PlayerResponse=%d"),
		*GetNameSafe(CachedPlayerCharacter.Get()),
		static_cast<int32>(CachedEnemyCollisionResponse.GetValue()),
		static_cast<int32>(CachedPlayerCollisionResponse.GetValue()));
}

void UGA_BurningSlam::RestoreSlamCollisionOverrides()
{
	if (!bSlamCollisionResponsesOverridden)
	{
		return;
	}

	if (CachedPlayerCharacter)
	{
		if (UCapsuleComponent* CapsuleComponent = CachedPlayerCharacter->GetCapsuleComponent())
		{
			CapsuleComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, CachedEnemyCollisionResponse.GetValue());
			CapsuleComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel3, CachedPlayerCollisionResponse.GetValue());
		}
	}

	bSlamCollisionResponsesOverridden = false;
}

void UGA_BurningSlam::RestoreMovementMode()
{
	if (CachedPlayerCharacter)
	{
		if (UCharacterMovementComponent* MovementComponent = CachedPlayerCharacter->GetCharacterMovement())
		{
			if (MovementComponent->MovementMode == MOVE_None)
			{
				MovementComponent->SetMovementMode(CachedMovementMode);
			}
		}
	}
}

void UGA_BurningSlam::RestoreAnimationMode()
{
	if (!bAnimationModeOverridden)
	{
		return;
	}

	if (CachedPlayerCharacter)
	{
		CachedPlayerCharacter->SetCurrentAttackAnimationModeInternal(EFrontierAttackAnimationMode::UpperBodyOnly);
	}

	bAnimationModeOverridden = false;
}
