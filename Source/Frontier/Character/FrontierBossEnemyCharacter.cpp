#include "Character/FrontierBossEnemyCharacter.h"

#include "AIController.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/FrontierGameplayAbility_EnemyAttack.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "Character/FrontierPlayerCharacter.h"
#include "DrawDebugHelpers.h"
#include "Frontier.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "Tags/FrontierGameplayTags.h"

AFrontierBossEnemyCharacter::AFrontierBossEnemyCharacter()
{
	

	const FFrontierGameplayTags& FrontierTags = FFrontierGameplayTags::Get();

	BossSkills.Add({ EFrontierBossCombatAction::DashSkill, FrontierTags.AbilityBossDashSkill, 300.0f, 800.0f, 5.0f, 1.0f, true });
	BossSkills.Add({ EFrontierBossCombatAction::RangedSkill, FrontierTags.AbilityBossRangedSkill, 800.0f, 1500.0f, 5.0f, 1.0f, true });
}

float AFrontierBossEnemyCharacter::GetDistanceToCombatTarget() const
{
	const AActor* TargetActor = GetCombatTarget();
	return IsValid(TargetActor)
		? FVector::Dist(GetActorLocation(), TargetActor->GetActorLocation())
		: TNumericLimits<float>::Max();
}

EFrontierBossDistanceRange AFrontierBossEnemyCharacter::GetDistanceRangeToCombatTarget() const
{
	

	if (!HasCombatTarget())
	{
		return EFrontierBossDistanceRange::None;
	}

	const float Distance = GetDistanceToCombatTarget();
	if (Distance <= AttackRange)
	{
		return EFrontierBossDistanceRange::Close;
	}

	if (Distance <= MidRange)
	{
		return EFrontierBossDistanceRange::Mid;
	}

	if (Distance <= FarRange)
	{
		return EFrontierBossDistanceRange::Far;
	}

	return EFrontierBossDistanceRange::OutOfCombat;
}

bool AFrontierBossEnemyCharacter::IsBossActionPlaying() const
{
	const UFrontierAbilitySystemComponent* LocalAbilitySystemComponent = GetFrontierAbilitySystemComponent();
	if (!LocalAbilitySystemComponent)
	{
		return false;
	}
	
	const FFrontierGameplayTags& FrontierTags = FFrontierGameplayTags::Get();
	const bool bAttacking = LocalAbilitySystemComponent->HasMatchingGameplayTag(FrontierTags.StateActionAttacking);
	const bool bSkill = LocalAbilitySystemComponent->HasMatchingGameplayTag(FrontierTags.StateCombatSkill);
	const bool bDashSkill = LocalAbilitySystemComponent->HasMatchingGameplayTag(FrontierTags.StateCombatSkillDash);
	const bool bStun = LocalAbilitySystemComponent->HasMatchingGameplayTag(FrontierTags.StateCCStun);
	const bool bDead = LocalAbilitySystemComponent->HasMatchingGameplayTag(FrontierTags.StateDead);
	
	FRONTIER_LOG(VeryVerbose, TEXT("BossActionPlaying Check: Attacking=%d Skill=%d DashSkill=%d Stun=%d Dead=%d"),
	bAttacking ? 1 : 0,
	bSkill ? 1 : 0,
	bDashSkill ? 1 : 0,
	bStun ? 1 : 0,
	bDead ? 1 : 0);

	return LocalAbilitySystemComponent->HasMatchingGameplayTag(FrontierTags.StateActionAttacking)
		|| LocalAbilitySystemComponent->HasMatchingGameplayTag(FrontierTags.StateCombatSkill)
		|| LocalAbilitySystemComponent->HasMatchingGameplayTag(FrontierTags.StateCombatSkillDash)
		|| LocalAbilitySystemComponent->HasMatchingGameplayTag(FrontierTags.StateCCStun)
		|| LocalAbilitySystemComponent->HasMatchingGameplayTag(FrontierTags.StateDead);
	
	
}

void AFrontierBossEnemyCharacter::CancelBossActions()
{
	

	if (!HasAuthority())
	{
		return;
	}

	UFrontierAbilitySystemComponent* LocalAbilitySystemComponent = GetFrontierAbilitySystemComponent();
	if (!LocalAbilitySystemComponent)
	{
		return;
	}

	const FFrontierGameplayTags& FrontierTags = FFrontierGameplayTags::Get();
	FGameplayTagContainer CancelTags;
	CancelTags.AddTag(FrontierTags.AbilityAttackPrimary);
	CancelTags.AddTag(FrontierTags.AbilityBossDashSkill);
	CancelTags.AddTag(FrontierTags.AbilityBossRangedSkill);
	LocalAbilitySystemComponent->CancelAbilities(&CancelTags);
	LocalAbilitySystemComponent->RemoveLooseGameplayTag(FrontierTags.StateActionAttacking);
	LocalAbilitySystemComponent->RemoveLooseGameplayTag(FrontierTags.StateCombatSkill);
	LocalAbilitySystemComponent->RemoveLooseGameplayTag(FrontierTags.StateCombatSkillDash);
	StopArcMove(true);
}

bool AFrontierBossEnemyCharacter::IsSkillReady(const FGameplayTag AbilityTag) const
{
	const FFrontierBossSkillDefinition* SkillDefinition = FindSkillDefinition(AbilityTag);
	if (!SkillDefinition)
	{
		return false;
	}

	const float* LastUseTime = LastSkillUseTimes.Find(AbilityTag);
	if (!LastUseTime)
	{
		return true;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	return Now - *LastUseTime >= SkillDefinition->Cooldown;
}

bool AFrontierBossEnemyCharacter::CanUseSkill(const FGameplayTag AbilityTag) const
{
	FRONTIER_LOG(VeryVerbose, TEXT("Checking boss skill availability. Boss=%s AbilityTag=%s"), *GetNameSafe(this), *AbilityTag.ToString());

	const FFrontierBossSkillDefinition* SkillDefinition = FindSkillDefinition(AbilityTag);
	if (!SkillDefinition)
	{
		FRONTIER_LOG(Warning, TEXT("Boss skill rejected because definition is missing. AbilityTag=%s"), *AbilityTag.ToString());
		return false;
	}

	const bool bHasTarget = HasCombatTarget();
	const bool bBossDead = IsDead();
	const bool bIsActionPlaying = IsBossActionPlaying();
	const bool bIsReady = IsSkillReady(AbilityTag);
	if (!bHasTarget || bBossDead || bIsActionPlaying || !bIsReady)
	{
		FRONTIER_LOG(VeryVerbose, TEXT("Boss skill rejected. HasTarget=%d IsDead=%d IsActionPlaying=%d IsReady=%d"),
			bHasTarget ? 1 : 0,
			bBossDead ? 1 : 0,
			bIsActionPlaying ? 1 : 0,
			bIsReady ? 1 : 0);
		return false;
	}

	const float Distance = GetDistanceToCombatTarget();
	if (!IsSkillInDistanceRange(*SkillDefinition, Distance))
	{
		FRONTIER_LOG(VeryVerbose, TEXT("Boss skill rejected by distance. Distance=%.2f Min=%.2f Max=%.2f"),
			Distance,
			SkillDefinition->MinRange,
			SkillDefinition->MaxRange);
		return false;
	}

	const bool bHasLineOfSight = !SkillDefinition->bRequiresLineOfSight || HasLineOfSightToCombatTarget();
	if (!bHasLineOfSight)
	{
		FRONTIER_LOG(VeryVerbose, TEXT("Boss skill rejected because line of sight failed. AbilityTag=%s Distance=%.2f"),
			*AbilityTag.ToString(),
			Distance);
	}

	return bHasLineOfSight;
}

bool AFrontierBossEnemyCharacter::TryActivateBossAbility(const FGameplayTag AbilityTag)
{
	FRONTIER_LOG(VeryVerbose, TEXT("Trying to activate boss ability. Boss=%s AbilityTag=%s"), *GetNameSafe(this), *AbilityTag.ToString());

	if (!HasAuthority() || !CanUseSkill(AbilityTag))
	{
		return false;
	}

	UFrontierAbilitySystemComponent* LocalAbilitySystemComponent = GetFrontierAbilitySystemComponent();
	if (!LocalAbilitySystemComponent)
	{
		return false;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(AbilityTag);
	const bool bActivated = LocalAbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTags);
	if (bActivated)
	{
		LastSkillUseTimes.FindOrAdd(AbilityTag) = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	}
	else
	{
		FRONTIER_LOG(Warning, TEXT("Boss ability activation failed after CanUseSkill passed. Boss=%s AbilityTag=%s AbilityCount=%d"),
			*GetNameSafe(this),
			*AbilityTag.ToString(),
			LocalAbilitySystemComponent->GetActivatableAbilities().Num());

		for (const FGameplayAbilitySpec& AbilitySpec : LocalAbilitySystemComponent->GetActivatableAbilities())
		{
			const UGameplayAbility* AbilityCDO = AbilitySpec.Ability;
			const FGameplayTagContainer AssetTags = AbilityCDO
				? AbilityCDO->GetAssetTags()
				: FGameplayTagContainer();

			FRONTIER_LOG(Warning, TEXT("Boss ASC ability spec. Class=%s AssetTags=%s Active=%d"),
				*GetNameSafe(AbilityCDO ? AbilityCDO->GetClass() : nullptr),
				*AssetTags.ToStringSimple(),
				AbilitySpec.IsActive() ? 1 : 0);
		}
	}

	FRONTIER_LOG(VeryVerbose, TEXT("Boss ability activation result. AbilityTag=%s Activated=%d"), *AbilityTag.ToString(), bActivated ? 1 : 0);
	return bActivated;
}

bool AFrontierBossEnemyCharacter::TryActivateWeightedSkillForRange(const EFrontierBossDistanceRange DistanceRange)
{
	FRONTIER_LOG(VeryVerbose, TEXT("Trying weighted boss skill for range %d."), static_cast<uint8>(DistanceRange));

	FGameplayTag AbilityTag;
	return GetWeightedSkillForRange(DistanceRange, AbilityTag)
		&& TryActivateBossAbility(AbilityTag);
}

bool AFrontierBossEnemyCharacter::SelectWeightedCombatAction()
{
	

	SelectedCombatAction = EFrontierBossCombatAction::None;
	SelectedCombatActionAbilityTag = FGameplayTag();

	if (!HasAuthority() || !HasCombatTarget() || IsDead() || IsBossActionPlaying())
	{
		FRONTIER_LOG(VeryVerbose, TEXT("Boss combat action selection rejected. HasAuthority=%d HasTarget=%d IsDead=%d IsActionPlaying=%d"),
			HasAuthority() ? 1 : 0,
			HasCombatTarget() ? 1 : 0,
			IsDead() ? 1 : 0,
			IsBossActionPlaying() ? 1 : 0);
		return false;
	}

	float TotalWeight = 0.0f;
	const bool bCanSelectPrimaryMelee = HasCombatTarget()
		&& !IsDead()
		&& !IsBossActionPlaying()
		&& HasValidAttackDefinition();
	if (bCanSelectPrimaryMelee)
	{
		TotalWeight += 1.0f;
	}

	for (const FFrontierBossSkillDefinition& SkillDefinition : BossSkills)
	{
		if (CanSelectCombatAction(SkillDefinition))
		{
			TotalWeight += SkillDefinition.Weight;
		}
	}

	if (TotalWeight <= 0.0f)
	{
		FRONTIER_LOG(Warning, TEXT("Boss combat action selection failed because no skill candidates were valid. Distance=%.2f CandidateCount=%d"),
			GetDistanceToCombatTarget(),
			BossSkills.Num());
		return false;
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	if (bCanSelectPrimaryMelee)
	{
		Roll -= 1.0f;
		if (Roll <= 0.0f)
		{
			SelectedCombatAction = EFrontierBossCombatAction::MeleeSkill;
			SelectedCombatActionAbilityTag = FGameplayTag();
			FRONTIER_LOG(VeryVerbose, TEXT("Selected boss primary melee action. Distance=%.2f AttackRange=%.2f TotalWeight=%.2f"),
				GetDistanceToCombatTarget(),
				AttackRange,
				TotalWeight);
			return true;
		}
	}

	for (const FFrontierBossSkillDefinition& SkillDefinition : BossSkills)
	{
		if (!CanSelectCombatAction(SkillDefinition))
		{
			continue;
		}

		Roll -= SkillDefinition.Weight;
		if (Roll <= 0.0f)
		{
			SelectedCombatAction = SkillDefinition.Action;
			SelectedCombatActionAbilityTag = SkillDefinition.AbilityTag;
			FRONTIER_LOG(VeryVerbose, TEXT("Selected weighted boss combat action. Action=%d AbilityTag=%s Distance=%.2f Weight=%.2f TotalWeight=%.2f"),
				static_cast<uint8>(SelectedCombatAction),
				*SelectedCombatActionAbilityTag.ToString(),
				GetDistanceToCombatTarget(),
				SkillDefinition.Weight,
				TotalWeight);
			return true;
		}
	}

	return false;
}

EFrontierBossCombatAction AFrontierBossEnemyCharacter::GetSelectedCombatAction() const
{
	return SelectedCombatAction;
}

FGameplayTag AFrontierBossEnemyCharacter::GetSelectedCombatActionAbilityTag() const
{
	return SelectedCombatActionAbilityTag;
}

void AFrontierBossEnemyCharacter::ClearSelectedCombatAction()
{
	FRONTIER_LOG(VeryVerbose, TEXT("Clearing selected boss combat action. Boss=%s PreviousAction=%d PreviousAbilityTag=%s"),
		*GetNameSafe(this),
		static_cast<uint8>(SelectedCombatAction),
		*SelectedCombatActionAbilityTag.ToString());

	SelectedCombatAction = EFrontierBossCombatAction::None;
	SelectedCombatActionAbilityTag = FGameplayTag();
}

bool AFrontierBossEnemyCharacter::TryActivateSelectedCombatActionAbility()
{
	FRONTIER_LOG(VeryVerbose, TEXT("Trying selected boss action ability. Action=%d AbilityTag=%s"),
		static_cast<uint8>(SelectedCombatAction),
		*SelectedCombatActionAbilityTag.ToString());

	if (!SelectedCombatActionAbilityTag.IsValid())
	{
		if (SelectedCombatAction == EFrontierBossCombatAction::MeleeSkill)
		{
			FRONTIER_LOG(VeryVerbose, TEXT("Selected boss melee action uses primary attack flow. Boss=%s"), *GetNameSafe(this));
			return !IsBossActionPlaying() && TryPerformAttack();
		}

		return false;
	}

	return TryActivateBossAbility(SelectedCombatActionAbilityTag);
}

bool AFrontierBossEnemyCharacter::CanReselectCombatTarget() const
{
	if (!HasAuthority() || IsDead() || IsBossActionPlaying())
	{
		return false;
	}

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	return CurrentTime - LastTargetReselectTime >= TargetReselectCooldown;
}

bool AFrontierBossEnemyCharacter::TryFindRandomCombatTargetInRange(const bool bKeepExistingTargetIfNoCandidate)
{
	

	if (!HasAuthority())
	{
		return HasCombatTarget();
	}

	if (IsDead())
	{
		return false;
	}

	if (IsBossActionPlaying())
	{
		FRONTIER_LOG(VeryVerbose, TEXT("Skipping boss target reselect because an action is playing. Boss=%s Target=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetCombatTarget()));
		return HasCombatTarget();
	}

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (CurrentTime - LastTargetReselectTime < TargetReselectCooldown)
	{
		FRONTIER_LOG(VeryVerbose, TEXT("Skipping boss target reselect because cooldown is active. Boss=%s Remaining=%.2f Target=%s"),
			*GetNameSafe(this),
			TargetReselectCooldown - (CurrentTime - LastTargetReselectTime),
			*GetNameSafe(GetCombatTarget()));
		return HasCombatTarget();
	}

	if (!GetWorld() || TargetSearchRadius <= KINDA_SMALL_NUMBER)
	{
		return bKeepExistingTargetIfNoCandidate && HasCombatTarget();
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel3); // Player

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossTargetReselect), false, this);
	const bool bHasAnyOverlap = GetWorld()->OverlapMultiByObjectType(
		OverlapResults,
		GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(TargetSearchRadius),
		QueryParams);

	TArray<AFrontierPlayerCharacter*> Candidates;
	Candidates.Reserve(FMath::Max(1, MaxTargetCandidates));

	if (bHasAnyOverlap)
	{
		const FFrontierGameplayTags& FrontierTags = FFrontierGameplayTags::Get();
		for (const FOverlapResult& OverlapResult : OverlapResults)
		{
			if (Candidates.Num() >= MaxTargetCandidates)
			{
				break;
			}

			AFrontierPlayerCharacter* Candidate = Cast<AFrontierPlayerCharacter>(OverlapResult.GetActor());
			if (!IsValid(Candidate) || Candidate->IsDead())
			{
				continue;
			}

			if (bExcludeStunnedTargetsFromReselect)
			{
				const UFrontierAbilitySystemComponent* CandidateASC = Candidate->GetFrontierAbilitySystemComponent();
				if (CandidateASC && CandidateASC->HasMatchingGameplayTag(FrontierTags.StateCCStun))
				{
					continue;
				}
			}

			if (bRequireLineOfSightForTargetReselect)
			{
				FHitResult HitResult;
				FCollisionQueryParams LineOfSightParams(SCENE_QUERY_STAT(BossTargetReselectLineOfSight), false, this);
				const FVector TraceStart = GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
				const FVector TraceEnd = Candidate->GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
				const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, LineOfSightParams);
				if (bHit && HitResult.GetActor() != Candidate)
				{
					continue;
				}
			}

			Candidates.Add(Candidate);
		}
	}

	if (!Candidates.IsEmpty())
	{
		AFrontierPlayerCharacter* SelectedTarget = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
		SetCombatTarget(SelectedTarget);
		LastTargetReselectTime = CurrentTime;
		FRONTIER_LOG(VeryVerbose, TEXT("Boss selected random combat target. Boss=%s Target=%s CandidateCount=%d Radius=%.2f"),
			*GetNameSafe(this),
			*GetNameSafe(SelectedTarget),
			Candidates.Num(),
			TargetSearchRadius);
		return true;
	}

	FRONTIER_LOG(VeryVerbose, TEXT("Boss target reselect found no candidates. Boss=%s KeepExisting=%d CurrentTarget=%s"),
		*GetNameSafe(this),
		bKeepExistingTargetIfNoCandidate ? 1 : 0,
		*GetNameSafe(GetCombatTarget()));

	if (bKeepExistingTargetIfNoCandidate && HasCombatTarget())
	{
		LastTargetReselectTime = CurrentTime;
		return true;
	}

	SetCombatTarget(nullptr);
	LastTargetReselectTime = CurrentTime;
	return false;
}

bool AFrontierBossEnemyCharacter::RefreshMoveLocationForSkillRange(const FGameplayTag AbilityTag)
{
	FRONTIER_LOG(VeryVerbose, TEXT("Refreshing boss move location for skill range. AbilityTag=%s"), *AbilityTag.ToString());

	const FFrontierBossSkillDefinition* SkillDefinition = FindSkillDefinition(AbilityTag);
	AActor* TargetActor = GetCombatTarget();
	if (!SkillDefinition || !IsValid(TargetActor))
	{
		return false;
	}

	const FVector DirectionFromTarget = (GetActorLocation() - TargetActor->GetActorLocation()).GetSafeNormal();
	const float DesiredDistance = FMath::Clamp((SkillDefinition->MinRange + SkillDefinition->MaxRange) * 0.5f, 0.0f, FarRange);
	const FVector DesiredLocation = TargetActor->GetActorLocation() + DirectionFromTarget * DesiredDistance;

	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavigationSystem)
	{
		SetDesiredBossMoveLocation(DesiredLocation);
		return true;
	}

	FNavLocation ProjectedLocation;
	if (NavigationSystem->ProjectPointToNavigation(DesiredLocation, ProjectedLocation, FVector(300.0f, 300.0f, 800.0f)))
	{
		if (FVector::DistSquared2D(ProjectedLocation.Location, GetActorLocation()) <= FMath::Square(50.0f))
		{
			FRONTIER_LOG(VeryVerbose, TEXT("Projected skill-range location is already near the boss. Location=%s"), *ProjectedLocation.Location.ToCompactString());
			return false;
		}

		SetDesiredBossMoveLocation(ProjectedLocation.Location);
		return true;
	}

	const float SearchRadius = FMath::Max(SkillDefinition->MaxRange, 300.0f);
	for (int32 AttemptIndex = 0; AttemptIndex < 8; ++AttemptIndex)
	{
		FNavLocation CandidateLocation;
		if (!NavigationSystem->GetRandomReachablePointInRadius(TargetActor->GetActorLocation(), SearchRadius, CandidateLocation))
		{
			continue;
		}

		const float CandidateDistance = FVector::Dist2D(CandidateLocation.Location, TargetActor->GetActorLocation());
		if (CandidateDistance < SkillDefinition->MinRange || CandidateDistance > SkillDefinition->MaxRange)
		{
			continue;
		}

		if (FVector::DistSquared2D(CandidateLocation.Location, GetActorLocation()) <= FMath::Square(50.0f))
		{
			continue;
		}

		SetDesiredBossMoveLocation(CandidateLocation.Location);
		return true;
	}

	FRONTIER_LOG(Warning, TEXT("Failed to resolve navigable skill-range location. RawDesiredLocation=%s"), *DesiredLocation.ToCompactString());
	return false;
}

bool AFrontierBossEnemyCharacter::RefreshRepositionLocation()
{
	

	AActor* TargetActor = GetCombatTarget();
	if (!IsValid(TargetActor))
	{
		return false;
	}

	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavigationSystem)
	{
		return false;
	}

	FNavLocation CandidateLocation;
	if (!NavigationSystem->GetRandomReachablePointInRadius(GetActorLocation(), RepositionRadius, CandidateLocation))
	{
		return false;
	}

	SetDesiredBossMoveLocation(CandidateLocation.Location);
	return true;
}

bool AFrontierBossEnemyCharacter::RefreshReturnHomeLocation()
{
	
	SetDesiredBossMoveLocation(HomeLocation);
	return true;
}

void AFrontierBossEnemyCharacter::FaceCombatTarget()
{
	

	AActor* TargetActor = GetCombatTarget();
	if (!HasAuthority() || !IsValid(TargetActor))
	{
		return;
	}

	const FVector Direction = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		SetActorRotation(Direction.Rotation());
	}
}

void AFrontierBossEnemyCharacter::AbandonCombat()
{
	

	if (!HasAuthority())
	{
		return;
	}

	SetCombatTarget(nullptr);
	SetEnemyState(EFrontierEnemyState::Returning);
	RefreshReturnHomeLocation();
}

bool AFrontierBossEnemyCharacter::StartArcMoveToLocation(
	const FVector StartLocation,
	const FVector ImpactLocation,
	const float JumpHeight,
	const float Duration)
{
	FRONTIER_LOG(Log, TEXT("Starting boss arc move. Boss=%s Start=%s Impact=%s JumpHeight=%.2f Duration=%.2f"),
		*GetNameSafe(this),
		*StartLocation.ToCompactString(),
		*ImpactLocation.ToCompactString(),
		JumpHeight,
		Duration);

	if (!HasAuthority() || IsDead() || Duration <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	StopArcMove(true);

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		SavedArcMoveMovementMode = MovementComponent->MovementMode;
		SavedArcMoveCustomMovementMode = MovementComponent->CustomMovementMode;
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	if (UCapsuleComponent* BossCapsuleComponent = GetCapsuleComponent())
	{
		// Pass through characters during the airborne portion of the dash.
		BossCapsuleComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
		BossCapsuleComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Ignore);
	}

	ArcMoveStartLocation = StartLocation;
	ArcMoveImpactLocation = ImpactLocation;
	ArcMoveJumpHeight = FMath::Max(0.0f, JumpHeight);
	ArcMoveDuration = FMath::Max(0.01f, Duration);
	ArcMoveElapsedTime = 0.0f;
	bArcMoveActive = true;
	bArcMoveAwaitingLandingNotify = false;

	SetActorLocation(ArcMoveStartLocation, false);

	GetWorldTimerManager().SetTimer(
		ArcMoveTimerHandle,
		this,
		&AFrontierBossEnemyCharacter::UpdateArcMove,
		0.016f,
		true);

	return true;
}

void AFrontierBossEnemyCharacter::StopArcMove(const bool bInterrupted)
{
	if (!bArcMoveActive && !bArcMoveAwaitingLandingNotify)
	{
		return;
	}

	FinishArcMove(bInterrupted);
}

void AFrontierBossEnemyCharacter::UpdateArcMove()
{
	if (!HasAuthority() || !bArcMoveActive)
	{
		return;
	}

	const float StepDeltaSeconds = GetWorldTimerManager().GetTimerRate(ArcMoveTimerHandle) > 0.0f
		? GetWorldTimerManager().GetTimerRate(ArcMoveTimerHandle)
		: 0.016f;
	ArcMoveElapsedTime += StepDeltaSeconds;
	const float Alpha = FMath::Clamp(ArcMoveElapsedTime / ArcMoveDuration, 0.0f, 1.0f);
	const float EasedAlpha = SmoothArcAlpha(Alpha);
	const FVector HorizontalLocation = FMath::Lerp(ArcMoveStartLocation, ArcMoveImpactLocation, EasedAlpha);
	const float ArcZ = FMath::Sin(EasedAlpha * PI) * ArcMoveJumpHeight;
	const FVector NewLocation = HorizontalLocation + FVector(0.0f, 0.0f, ArcZ);

	SetActorLocation(NewLocation, false);

	if (Alpha >= 1.0f)
	{
		SetActorLocation(ArcMoveImpactLocation, false);
		GetWorldTimerManager().ClearTimer(ArcMoveTimerHandle);
		bArcMoveActive = false;
		bArcMoveAwaitingLandingNotify = true;
	}
}

bool AFrontierBossEnemyCharacter::CompleteArcMoveFromNotify()
{
	if (!IsArcMoveReadyForLandingNotify())
	{
		return false;
	}

	FinishArcMove(false);
	return true;
}

bool AFrontierBossEnemyCharacter::PrepareArcMoveLandingFromNotify()
{
	if (!IsArcMoveReadyForLandingNotify())
	{
		return false;
	}

	SetActorLocation(ArcMoveImpactLocation, false);
	return true;
}

bool AFrontierBossEnemyCharacter::IsArcMoveReadyForLandingNotify() const
{
	return bArcMoveActive || bArcMoveAwaitingLandingNotify;
}

void AFrontierBossEnemyCharacter::FinishArcMove(const bool bInterrupted)
{
	FRONTIER_LOG(Log, TEXT("Finishing boss arc move. Boss=%s Interrupted=%d Location=%s"),
		*GetNameSafe(this),
		bInterrupted ? 1 : 0,
		*GetActorLocation().ToCompactString());

	GetWorldTimerManager().ClearTimer(ArcMoveTimerHandle);
	bArcMoveActive = false;
	bArcMoveAwaitingLandingNotify = false;
	ArcMoveElapsedTime = 0.0f;

	if (!bInterrupted)
	{
		SetActorLocation(ArcMoveImpactLocation, false);
	}

	if (UCapsuleComponent* BossCapsuleComponent = GetCapsuleComponent())
	{
		BossCapsuleComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
		BossCapsuleComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Block);
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->SetMovementMode(SavedArcMoveMovementMode, SavedArcMoveCustomMovementMode);
		if (MovementComponent->MovementMode == MOVE_None)
		{
			MovementComponent->SetMovementMode(MOVE_Walking);
		}
		MovementComponent->StopMovementImmediately();
	}

	OnArcMoveFinished.Broadcast(bInterrupted);
}

float AFrontierBossEnemyCharacter::SmoothArcAlpha(const float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return ClampedAlpha * ClampedAlpha * (3.0f - 2.0f * ClampedAlpha);
}

void AFrontierBossEnemyCharacter::BeginPlay()
{
	

	Super::BeginPlay();
	EnsurePrimaryAttackAbility();
}

void AFrontierBossEnemyCharacter::EnsurePrimaryAttackAbility()
{
	

	if (!HasAuthority())
	{
		return;
	}

	UFrontierAbilitySystemComponent* LocalAbilitySystemComponent = GetFrontierAbilitySystemComponent();
	if (!LocalAbilitySystemComponent)
	{
		FRONTIER_LOG(Warning, TEXT("Boss primary attack ability check failed because ASC is missing. Boss=%s"), *GetNameSafe(this));
		return;
	}

	if (!LocalAbilitySystemComponent->FindAbilitySpecFromClass(UFrontierGameplayAbility_EnemyAttack::StaticClass()))
	{
		LocalAbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UFrontierGameplayAbility_EnemyAttack::StaticClass(), 1, INDEX_NONE, this));
		FRONTIER_LOG(Log, TEXT("Granted missing boss primary attack ability to %s."), *GetNameSafe(this));
	}
}

EFrontierLootContainerSourceType AFrontierBossEnemyCharacter::GetDeathLootContainerSourceType() const
{
	return EFrontierLootContainerSourceType::BossDeath;
}

bool AFrontierBossEnemyCharacter::CanUseAttackOnTarget(AActor* TargetActor) const
{
	const bool bHasValidTarget = IsValid(TargetActor);
	const float DistanceSq2D = bHasValidTarget
		? FVector::DistSquared2D(GetActorLocation(), TargetActor->GetActorLocation())
		: TNumericLimits<float>::Max();
	const bool bInRange = DistanceSq2D <= FMath::Square(AttackRange);

	FRONTIER_LOG(VeryVerbose, TEXT("Boss attack target check. Boss=%s Target=%s Distance2D=%.2f AttackRange=%.2f InRange=%d"),
		*GetNameSafe(this),
		*GetNameSafe(TargetActor),
		FMath::Sqrt(DistanceSq2D),
		AttackRange,
		bInRange ? 1 : 0);

	return bHasValidTarget && bInRange;
}

const FFrontierBossSkillDefinition* AFrontierBossEnemyCharacter::FindSkillDefinition(const FGameplayTag AbilityTag) const
{
	return BossSkills.FindByPredicate([AbilityTag](const FFrontierBossSkillDefinition& SkillDefinition)
	{
		return SkillDefinition.AbilityTag == AbilityTag;
	});
}

bool AFrontierBossEnemyCharacter::HasLineOfSightToCombatTarget() const
{
	const AActor* TargetActor = GetCombatTarget();
	if (!IsValid(TargetActor) || !GetWorld())
	{
		return false;
	}

	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossSkillLineOfSight), false, this);
	const FVector TraceStart = GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
	const FVector TraceEnd = TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
	return !bHit || HitResult.GetActor() == TargetActor;
}

bool AFrontierBossEnemyCharacter::GetWeightedSkillForRange(const EFrontierBossDistanceRange DistanceRange, FGameplayTag& OutAbilityTag) const
{
	const float Distance = GetDistanceToCombatTarget();
	float TotalWeight = 0.0f;
	for (const FFrontierBossSkillDefinition& SkillDefinition : BossSkills)
	{
		if (SkillDefinition.Weight > 0.0f
			&& SkillDefinition.Action != EFrontierBossCombatAction::MeleeSkill
			&& SkillDefinition.AbilityTag != FFrontierGameplayTags::Get().AbilityAttackPrimary
			&& SkillDefinition.AbilityTag.IsValid()
			&& IsSkillInDistanceRange(SkillDefinition, Distance)
			&& IsSkillReady(SkillDefinition.AbilityTag)
			&& (!SkillDefinition.bRequiresLineOfSight || HasLineOfSightToCombatTarget()))
		{
			const EFrontierBossDistanceRange SkillRange = Distance <= AttackRange
				? EFrontierBossDistanceRange::Close
				: Distance <= MidRange
					? EFrontierBossDistanceRange::Mid
					: Distance <= FarRange
						? EFrontierBossDistanceRange::Far
						: EFrontierBossDistanceRange::OutOfCombat;
			if (SkillRange == DistanceRange)
			{
				TotalWeight += SkillDefinition.Weight;
			}
		}
	}

	if (TotalWeight <= 0.0f)
	{
		return false;
	}

	float Roll = FMath::FRand() * TotalWeight;
	for (const FFrontierBossSkillDefinition& SkillDefinition : BossSkills)
	{
		if (SkillDefinition.Weight <= 0.0f
			|| SkillDefinition.Action == EFrontierBossCombatAction::MeleeSkill
			|| SkillDefinition.AbilityTag == FFrontierGameplayTags::Get().AbilityAttackPrimary
			|| !SkillDefinition.AbilityTag.IsValid()
			|| !IsSkillInDistanceRange(SkillDefinition, Distance)
			|| !IsSkillReady(SkillDefinition.AbilityTag)
			|| (SkillDefinition.bRequiresLineOfSight && !HasLineOfSightToCombatTarget()))
		{
			continue;
		}

		Roll -= SkillDefinition.Weight;
		if (Roll <= 0.0f)
		{
			OutAbilityTag = SkillDefinition.AbilityTag;
			return true;
		}
	}

	return false;
}

bool AFrontierBossEnemyCharacter::IsSkillInDistanceRange(const FFrontierBossSkillDefinition& SkillDefinition, const float Distance) const
{
	return Distance >= SkillDefinition.MinRange && Distance <= SkillDefinition.MaxRange;
}

bool AFrontierBossEnemyCharacter::CanSelectCombatAction(const FFrontierBossSkillDefinition& SkillDefinition) const
{
	if (SkillDefinition.Weight <= 0.0f
		|| SkillDefinition.Action == EFrontierBossCombatAction::None
		|| !SkillDefinition.AbilityTag.IsValid())
	{
		FRONTIER_LOG(VeryVerbose, TEXT("Boss combat action candidate rejected by action, tag, or weight. Action=%d AbilityTag=%s Weight=%.2f"),
			static_cast<uint8>(SkillDefinition.Action),
			*SkillDefinition.AbilityTag.ToString(),
			SkillDefinition.Weight);
		return false;
	}

	if (!HasCombatTarget() || IsDead() || IsBossActionPlaying())
	{
		FRONTIER_LOG(VeryVerbose, TEXT("Boss combat action candidate rejected by actor state. Action=%d HasTarget=%d IsDead=%d IsActionPlaying=%d"),
			static_cast<uint8>(SkillDefinition.Action),
			HasCombatTarget() ? 1 : 0,
			IsDead() ? 1 : 0,
			IsBossActionPlaying() ? 1 : 0);
		return false;
	}

	switch (SkillDefinition.Action)
	{
	case EFrontierBossCombatAction::DashSkill:
	case EFrontierBossCombatAction::RangedSkill:
	{
		const bool bCanUseSkill = CanUseSkill(SkillDefinition.AbilityTag);
		if (!bCanUseSkill)
		{
			FRONTIER_LOG(VeryVerbose, TEXT("Boss combat action candidate rejected by skill availability. Action=%d AbilityTag=%s Distance=%.2f"),
				static_cast<uint8>(SkillDefinition.Action),
				*SkillDefinition.AbilityTag.ToString(),
				GetDistanceToCombatTarget());
		}
		return bCanUseSkill;
	}

	case EFrontierBossCombatAction::MeleeAttack:
	case EFrontierBossCombatAction::MeleeSkill:
	case EFrontierBossCombatAction::Reposition:
	case EFrontierBossCombatAction::ReturnHome:
		FRONTIER_LOG(VeryVerbose, TEXT("Boss combat action candidate rejected because it is not a boss skill action. Action=%d"),
			static_cast<uint8>(SkillDefinition.Action));
		return false;

	default:
		return false;
	}
}

void AFrontierBossEnemyCharacter::SetDesiredBossMoveLocation(const FVector& NewLocation)
{
	DesiredMoveLocation = NewLocation;
	FRONTIER_LOG(VeryVerbose, TEXT("Boss desired move location updated to %s"), *DesiredMoveLocation.ToCompactString());
}
