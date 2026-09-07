#include "Character/FrontierEnemyCharacter.h"
#include "Character/FrontierChaosDeathActor.h"

#include "AI/FrontierEnemyAIController.h"
#include "AIController.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/FrontierGameplayAbility_EnemyAttack.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "AbilitySystem/FrontierAttributeSet.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "BrainComponent.h"
#include "Components/FrontierCombatComponent.h"
#include "Components/FrontierLootComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/WidgetComponent.h"
#include "Frontier.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "Tags/FrontierGameplayTags.h"
#include "UI/FrontierEnemyHealthWidget.h"

AFrontierEnemyCharacter::AFrontierEnemyCharacter()
{
	

	AbilitySystemComponent = CreateDefaultSubobject<UFrontierAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<UFrontierAttributeSet>(TEXT("AttributeSet"));

	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;

	// HealthBarWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidgetComponent"));
	// HealthBarWidgetComponent->SetupAttachment(RootComponent);
	// HealthBarWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	// HealthBarWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	// HealthBarWidgetComponent->SetDrawSize(FVector2D(140.0f, 14.0f));
	// HealthBarWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
	// HealthBarWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// HealthBarWidgetComponent->SetWidgetClass(UFrontierEnemyHealthWidget::StaticClass());

	AIControllerClass = AFrontierEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	SetTeam(EFrontierTeam::Monster);

	StartupAbilities.Add(UFrontierGameplayAbility_EnemyAttack::StaticClass());
}

EFrontierHitReactionLevel AFrontierEnemyCharacter::GetCurrentHitReactionResistance() const
{
	const EFrontierHitReactionLevel BaseResistance = Super::GetCurrentHitReactionResistance();
	if (!AttackDefinitions.IsValidIndex(CurrentAttackDefinitionIndex))
	{
		return BaseResistance;
	}

	const FFrontierEnemyAttackDefinition& AttackDefinition = AttackDefinitions[CurrentAttackDefinitionIndex];
	const UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	const EFrontierHitReactionLevel AttackResistance = AnimInstance && AttackDefinition.Montage
		&& AnimInstance->Montage_IsPlaying(AttackDefinition.Montage)
		? AttackDefinition.HitReactionResistance
		: EFrontierHitReactionLevel::None;
	return static_cast<uint8>(AttackResistance) > static_cast<uint8>(BaseResistance)
		? AttackResistance
		: BaseResistance;
}

bool AFrontierEnemyCharacter::IsDamageReactionBlocked() const
{
	return bBlockDamageReaction;
}

void AFrontierEnemyCharacter::SetBlockDamageReaction(const bool bNewBlockDamageReaction)
{
	if (!HasAuthority())
	{
		return;
	}

	bBlockDamageReaction = bNewBlockDamageReaction;
	if (bBlockDamageReaction)
	{
		bHasLastDamageReaction = false;
	}
}

void AFrontierEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	

	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFrontierEnemyCharacter, EnemyState);
	DOREPLIFETIME(AFrontierEnemyCharacter, CombatTarget);
	DOREPLIFETIME(AFrontierEnemyCharacter, bUseChaosDeathForCurrentDeath);
}

void AFrontierEnemyCharacter::SetEnemyState(const EFrontierEnemyState NewState)
{
	if (!HasAuthority())
	{
		FRONTIER_LOG(Warning, TEXT("Ignoring state change on non-authority instance."));
		return;
	}

	if (IsDead())
	{
		return;
	}

	if (EnemyState == NewState)
	{
		return;
	}

	const EFrontierEnemyState PreviousState = EnemyState;
	EnemyState = NewState;

	HandleEnemyStateChanged(PreviousState, EnemyState);
	OnEnemyStateChanged.Broadcast(PreviousState, EnemyState);
}

EFrontierEnemyState AFrontierEnemyCharacter::GetEnemyState() const
{
	return EnemyState;
}

void AFrontierEnemyCharacter::SetCombatTarget(AActor* NewCombatTarget)
{
	if (!HasAuthority())
	{
		FRONTIER_LOG(Warning, TEXT("Ignoring combat target change on non-authority instance."));
		return;
	}

	if (IsDead())
	{
		return;
	}

	if (CombatTarget == NewCombatTarget)
	{
		return;
	}

	const bool bHadCombatTarget = IsValid(CombatTarget);
	CombatTarget = NewCombatTarget;
	const bool bHasNewCombatTarget = IsValid(CombatTarget);

	if (AAIController* EnemyController = Cast<AAIController>(GetController()))
	{
		EnemyController->StopMovement();
		EnemyController->ClearFocus(EAIFocusPriority::Gameplay);
	}

	if (bHasNewCombatTarget)
	{
		const FVector DirectionToTarget = (CombatTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (!DirectionToTarget.IsNearlyZero())
		{
			SetActorRotation(DirectionToTarget.Rotation());
		}
	}

	if (bHadCombatTarget != bHasNewCombatTarget)
	{
		if (AAIController* EnemyController = Cast<AAIController>(GetController()))
		{
			if (UBrainComponent* EnemyBrainComponent = EnemyController->GetBrainComponent())
			{
				EnemyBrainComponent->RestartLogic();
			}
		}
	}
}

AActor* AFrontierEnemyCharacter::GetCombatTarget() const
{
	return CombatTarget;
}

bool AFrontierEnemyCharacter::HasCombatTarget() const
{
	return IsValid(CombatTarget);
}

bool AFrontierEnemyCharacter::IsCombatTargetInAttackRange() const
{
	if (CombatTarget == nullptr)
	{
		return false;
	}

	return CanUseAttackOnTarget(CombatTarget);
}

FVector AFrontierEnemyCharacter::GetDesiredMoveLocation() const
{
	return DesiredMoveLocation;
}

bool AFrontierEnemyCharacter::RefreshDesiredMoveLocation(const EFrontierEnemyMoveMode MoveMode)
{
	if (IsDead())
	{
		return false;
	}

	FVector NextLocation = FVector::ZeroVector;
	bool bResolvedLocation = false;

	switch (MoveMode)
	{
	case EFrontierEnemyMoveMode::Patrol:
		bResolvedLocation = SelectPatrolLocation(NextLocation);
		break;

	case EFrontierEnemyMoveMode::Chase:
		bResolvedLocation = SelectChaseLocation(CombatTarget, NextLocation);
		break;

	default:
		break;
	}

	if (!bResolvedLocation)
	{
		FRONTIER_LOG(Warning, TEXT("Failed to resolve desired move location."));
		return false;
	}

	DesiredMoveLocation = NextLocation;
	return true;
}

bool AFrontierEnemyCharacter::TryPerformAttack()
{
	if (IsDead())
	{
		return false;
	}

	if (!CombatTarget)
	{
		return false;
	}

	if (!CanUseAttackOnTarget(CombatTarget))
	{
		return false;
	}

	return ExecuteAttackBehavior(CombatTarget);
}

float AFrontierEnemyCharacter::GetLastAttackMontageDuration() const
{
	
	return LastAttackMontageDuration;
}

bool AFrontierEnemyCharacter::PrepareRandomAttackDefinitionForAbility()
{
	

	if (!HasAuthority() || IsDead())
	{
		return false;
	}

	TArray<int32> ValidAttackDefinitionIndices;
	ValidAttackDefinitionIndices.Reserve(AttackDefinitions.Num());
	for (int32 AttackIndex = 0; AttackIndex < AttackDefinitions.Num(); ++AttackIndex)
	{
		const FFrontierEnemyAttackDefinition& AttackDefinition = AttackDefinitions[AttackIndex];
		if (IsValid(AttackDefinition.Montage)
			&& !AttackDefinition.StartSocketName.IsNone()
			&& !AttackDefinition.EndSocketName.IsNone()
			&& AttackDefinition.TraceRadius > 0.0f
			&& AttackDefinition.BaseDamage > 0.0f
			&& GetMesh()
			&& GetMesh()->DoesSocketExist(AttackDefinition.StartSocketName)
			&& GetMesh()->DoesSocketExist(AttackDefinition.EndSocketName))
		{
			ValidAttackDefinitionIndices.Add(AttackIndex);
		}
	}

	if (ValidAttackDefinitionIndices.IsEmpty())
	{
		FRONTIER_LOG(Warning, TEXT("Enemy attack ability could not find a valid attack definition."));
		CurrentAttackDefinitionIndex = INDEX_NONE;
		LastAttackMontageDuration = 0.0f;
		return false;
	}

	CurrentAttackDefinitionIndex = ValidAttackDefinitionIndices[FMath::RandRange(0, ValidAttackDefinitionIndices.Num() - 1)];
	UAnimMontage* SelectedAttackMontage = AttackDefinitions[CurrentAttackDefinitionIndex].Montage;
	LastAttackMontageDuration = SelectedAttackMontage ? SelectedAttackMontage->GetPlayLength() : 0.0f;
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	return SelectedAttackMontage != nullptr;
}

UAnimMontage* AFrontierEnemyCharacter::GetCurrentAttackMontage() const
{
	
	return AttackDefinitions.IsValidIndex(CurrentAttackDefinitionIndex)
		? AttackDefinitions[CurrentAttackDefinitionIndex].Montage
		: nullptr;
}

bool AFrontierEnemyCharacter::HasValidAttackDefinition() const
{
	

	for (const FFrontierEnemyAttackDefinition& AttackDefinition : AttackDefinitions)
	{
		if (IsValid(AttackDefinition.Montage)
			&& !AttackDefinition.StartSocketName.IsNone()
			&& !AttackDefinition.EndSocketName.IsNone()
			&& AttackDefinition.TraceRadius > 0.0f
			&& AttackDefinition.BaseDamage > 0.0f
			&& GetMesh()
			&& GetMesh()->DoesSocketExist(AttackDefinition.StartSocketName)
			&& GetMesh()->DoesSocketExist(AttackDefinition.EndSocketName))
		{
			return true;
		}
	}

	return false;
}

void AFrontierEnemyCharacter::BeginCurrentAttackTrace()
{
	

	if (!HasAuthority())
	{
		return;
	}

	if (UFrontierCombatComponent* LocalCombatComponent = GetCombatComponent())
	{
		LocalCombatComponent->ResetHitActorsThisAttack();
	}
}

void AFrontierEnemyCharacter::TickCurrentAttackTrace()
{
	if (!HasAuthority() || !AttackDefinitions.IsValidIndex(CurrentAttackDefinitionIndex))
	{
		return;
	}

	const FFrontierEnemyAttackDefinition& AttackDefinition = AttackDefinitions[CurrentAttackDefinitionIndex];
	if (!IsValid(AttackDefinition.Montage) || AttackDefinition.BaseDamage <= 0.0f || AttackDefinition.TraceRadius <= 0.0f)
	{
		return;
	}

	UFrontierCombatComponent* LocalCombatComponent = GetCombatComponent();
	if (!LocalCombatComponent)
	{
		FRONTIER_LOG(Warning, TEXT("Attack trace skipped because the combat component is missing."));
		return;
	}

	const FGameplayTag DamageTypeTag = AttackDefinition.DamageTypeTag.IsValid()
		? AttackDefinition.DamageTypeTag
		: FFrontierGameplayTags::Get().DamageTypePhysical;

	LocalCombatComponent->ApplySphereTraceDamageFromSockets(
		GetMesh(),
		AttackDefinition.StartSocketName,
		AttackDefinition.EndSocketName,
		AttackDefinition.TraceRadius,
		AttackDefinition.BaseDamage,
		DamageTypeTag,
		nullptr,
		nullptr,
		1.0f,
		AttackDefinition.bDrawDebugTrace,
		EFrontierElementalType::Normal,
		AttackDefinition.HitReactionTag,
		AttackDefinition.HitReactionLevel,
		AttackDefinition.StaggerDuration,
		AttackDefinition.KnockbackHorizontalStrength,
		AttackDefinition.KnockbackVerticalStrength);
}

void AFrontierEnemyCharacter::EndCurrentAttackTrace()
{
	

	if (HasAuthority())
	{
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
	}
}

void AFrontierEnemyCharacter::BeginPlay()
{
	

	Super::BeginPlay();
	if (HasAuthority())
	{
		GrantStartupAbilities();
	}

	HomeLocation = GetActorLocation();
	PatrolCenterLocation = HomeLocation;
	DesiredMoveLocation = HomeLocation;
	SelectPatrolLocation(DesiredMoveLocation);

	// if (GetNetMode() != NM_DedicatedServer && HealthBarWidgetComponent)
	// {
	// 	HealthBarWidgetComponent->InitWidget();
	// 	if (UFrontierEnemyHealthWidget* HealthWidget = Cast<UFrontierEnemyHealthWidget>(HealthBarWidgetComponent->GetUserWidgetObject()))
	// 	{
	// 		HealthWidget->BindToEnemy(this);
	// 	}
	// 	else
	// 	{
	// 		FRONTIER_LOG(Warning, TEXT("Enemy health widget could not be initialized. Enemy=%s WidgetClass=%s"),
	// 			*GetNameSafe(this),
	// 			*GetNameSafe(HealthBarWidgetComponent->GetWidgetClass()));
	// 	}
	// }
}

void AFrontierEnemyCharacter::HandleDeathStateChanged(const bool bWasDead)
{
	if (HasAuthority() && !bWasDead && IsDead())
	{
		const bool bWasKnockbackKillingBlow =
			!IsDamageReactionBlocked()
			&& (LastDamageHitReactionLevel == EFrontierHitReactionLevel::KnockBack
				|| LastDamageHitReactionTag.MatchesTagExact(FFrontierGameplayTags::Get().HitReactionKnockback));
		bUseChaosDeathForCurrentDeath = bUseChaosDeath
			&& IsValid(ChaosDeathActorClass)
			&& bLastDamageWasFromPlayer
			&& bWasKnockbackKillingBlow;
	}

	Super::HandleDeathStateChanged(bWasDead);
	if (HasAuthority() && !bWasDead && IsDead())
	{
		CombatTarget = nullptr;
		CurrentAttackDefinitionIndex = INDEX_NONE;
		LastAttackMontageDuration = 0.0f;
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;

		if (AAIController* EnemyController = Cast<AAIController>(GetController()))
		{
			EnemyController->StopMovement();
			EnemyController->ClearFocus(EAIFocusPriority::Gameplay);
			if (UBrainComponent* EnemyBrainComponent = EnemyController->GetBrainComponent())
			{
				FRONTIER_LOG(Log, TEXT("Stopping enemy AI brain because enemy died. Enemy=%s Controller=%s Brain=%s"),
					*GetNameSafe(this),
					*GetNameSafe(EnemyController),
					*GetNameSafe(EnemyBrainComponent));
				EnemyBrainComponent->StopLogic(TEXT("Enemy died"));
			}
		}

		OnEnemyDied.Broadcast(this);
	}
}

bool AFrontierEnemyCharacter::UsesAlternativeDeathPresentation() const
{
	return bUseChaosDeathForCurrentDeath && IsValid(ChaosDeathActorClass);
}

void AFrontierEnemyCharacter::ActivateAlternativeDeathPresentation()
{
	if (!HasAuthority() || !UsesAlternativeDeathPresentation() || !GetMesh())
	{
		return;
	}

	const FTransform SpawnTransform = ChaosDeathRelativeTransform * GetMesh()->GetComponentTransform();
	MulticastSpawnChaosDeathActor(SpawnTransform, LastDamageImpactPoint, bHasLastDamageImpactPoint);
}

void AFrontierEnemyCharacter::MulticastSpawnChaosDeathActor_Implementation(
	const FTransform SpawnTransform,
	const FVector ImpactPoint,
	const bool bHasImpactPoint)
{
	if (GetNetMode() == NM_DedicatedServer || !ChaosDeathActorClass || !GetWorld())
	{
		return;
	}
	HideCharacterForDeathReplacement();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (AFrontierChaosDeathActor* ChaosDeathActor = GetWorld()->SpawnActor<AFrontierChaosDeathActor>(
		ChaosDeathActorClass,
		SpawnTransform,
		SpawnParameters))
	{
		ChaosDeathActor->SetDestructionOrigin(ImpactPoint, bHasImpactPoint);
	}
}

void AFrontierEnemyCharacter::CreateDeathLootSlots(TArray<FFrontierInventorySlot>& OutLootSlots) const
{
	

	OutLootSlots = LootComponent ? LootComponent->GenerateLootSlots() : TArray<FFrontierInventorySlot>();
}

EFrontierLootContainerSourceType AFrontierEnemyCharacter::GetDeathLootContainerSourceType() const
{
	return EFrontierLootContainerSourceType::EnemyDeath;
}

void AFrontierEnemyCharacter::OnRep_EnemyState(const EFrontierEnemyState PreviousState)
{
	HandleEnemyStateChanged(PreviousState, EnemyState);
	OnEnemyStateChanged.Broadcast(PreviousState, EnemyState);
}

void AFrontierEnemyCharacter::OnRep_CombatTarget(AActor* PreviousCombatTarget)
{
}

void AFrontierEnemyCharacter::MulticastPlayAttackMontage_Implementation(UAnimMontage* AttackMontage)
{
	if (!AttackMontage)
	{
		FRONTIER_LOG(Warning, TEXT("Cannot play a null enemy attack montage."));
		return;
	}

	PlayAnimMontage(AttackMontage);
}

bool AFrontierEnemyCharacter::SelectPatrolLocation(FVector& OutLocation)
{
	

	if (PatrolRouteMode == EFrontierPatrolRouteMode::Spline && PatrolSplineComponent)
	{
		const int32 SplinePointCount = PatrolSplineComponent->GetNumberOfSplinePoints();
		if (SplinePointCount > 0)
		{
			PatrolPointIndex = (PatrolPointIndex + 1) % SplinePointCount;
			OutLocation = PatrolSplineComponent->GetLocationAtSplinePoint(PatrolPointIndex, ESplineCoordinateSpace::World);
			return true;
		}

		FRONTIER_LOG(Warning, TEXT("Patrol spline exists but has no points. Falling back to random patrol."));
	}

	if (!PatrolPoints.IsEmpty())
	{
		for (int32 AttemptIndex = 0; AttemptIndex < PatrolPoints.Num(); ++AttemptIndex)
		{
			PatrolPointIndex = (PatrolPointIndex + 1) % PatrolPoints.Num();

			if (const AActor* PatrolPoint = PatrolPoints[PatrolPointIndex])
			{
				OutLocation = PatrolPoint->GetActorLocation();
				return true;
			}
		}

		FRONTIER_LOG(Warning, TEXT("Patrol points exist but none are valid. Falling back to random patrol."));
	}

	const FVector PatrolCenter = PatrolCenterMode == EFrontierPatrolCenterMode::CurrentLocation
		? GetActorLocation()
		: PatrolCenterMode == EFrontierPatrolCenterMode::ExplicitLocation
			? PatrolCenterLocation
			: HomeLocation;

	if (PatrolRadius <= KINDA_SMALL_NUMBER)
	{
		OutLocation = PatrolCenter;
		return true;
	}

	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavigationSystem)
	{
		FRONTIER_LOG(Warning, TEXT("Navigation system is missing. Falling back to patrol center."));
		OutLocation = PatrolCenter;
		return true;
	}

	const float RequiredMinDistanceSq = FMath::Square(FMath::Min(MinPatrolDistance, PatrolRadius));
	for (int32 AttemptIndex = 0; AttemptIndex < 8; ++AttemptIndex)
	{
		FNavLocation CandidateLocation;
		if (!NavigationSystem->GetRandomReachablePointInRadius(PatrolCenter, PatrolRadius, CandidateLocation))
		{
			continue;
		}

		if (FVector::DistSquared(PatrolCenter, CandidateLocation.Location) < RequiredMinDistanceSq)
		{
			continue;
		}

		OutLocation = CandidateLocation.Location;
		return true;
	}

	FRONTIER_LOG(Warning, TEXT("Failed to find random patrol location in radius %.2f. Falling back to patrol center."), PatrolRadius);
	OutLocation = PatrolCenter;
	return true;
}

void AFrontierEnemyCharacter::SetRandomPatrolArea(
	const FVector InHomeLocation,
	const FVector InPatrolCenter,
	const float InPatrolRadius)
{
	FRONTIER_LOG(Log, TEXT("Setting enemy random patrol area. Enemy=%s Home=%s Center=%s Radius=%.2f"),
		*GetNameSafe(this),
		*InHomeLocation.ToCompactString(),
		*InPatrolCenter.ToCompactString(),
		InPatrolRadius);

	HomeLocation = InHomeLocation;
	PatrolCenterLocation = InPatrolCenter;
	PatrolRadius = FMath::Max(0.0f, InPatrolRadius);
	PatrolCenterMode = EFrontierPatrolCenterMode::ExplicitLocation;
	PatrolRouteMode = EFrontierPatrolRouteMode::RandomAroundCenter;
	PatrolSplineComponent = nullptr;
	PatrolPointIndex = INDEX_NONE;
	DesiredMoveLocation = HomeLocation;
	SelectPatrolLocation(DesiredMoveLocation);
}

void AFrontierEnemyCharacter::SetSplinePatrolRoute(
	const FVector InHomeLocation,
	USplineComponent* InPatrolSpline)
{
	FRONTIER_LOG(Log, TEXT("Setting enemy spline patrol route. Enemy=%s Home=%s Spline=%s"),
		*GetNameSafe(this),
		*InHomeLocation.ToCompactString(),
		*GetNameSafe(InPatrolSpline));

	HomeLocation = InHomeLocation;
	PatrolCenterLocation = InHomeLocation;
	PatrolSplineComponent = InPatrolSpline;
	PatrolRouteMode = InPatrolSpline ? EFrontierPatrolRouteMode::Spline : EFrontierPatrolRouteMode::RandomAroundCenter;
	PatrolCenterMode = EFrontierPatrolCenterMode::SpawnLocation;
	PatrolPointIndex = INDEX_NONE;
	DesiredMoveLocation = HomeLocation;
	SelectPatrolLocation(DesiredMoveLocation);
}

bool AFrontierEnemyCharacter::SelectChaseLocation(AActor* TargetActor, FVector& OutLocation)
{
	if (!TargetActor)
	{
		FRONTIER_LOG(Warning, TEXT("Cannot select chase location without a target."));
		return false;
	}

	OutLocation = TargetActor->GetActorLocation();
	return true;
}

bool AFrontierEnemyCharacter::CanUseAttackOnTarget(AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return false;
	}

	const FVector MyLocation = GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();

	const float DistanceSquared2D = FVector::DistSquared2D(MyLocation, TargetLocation);
	const float AttackRangeSquared = FMath::Square(AttackRange);
	const bool bInAttackRange = DistanceSquared2D <= AttackRangeSquared;

	return bInAttackRange;
}

bool AFrontierEnemyCharacter::ExecuteAttackBehavior(AActor* TargetActor)
{
	if (!TargetActor || !HasAuthority() || IsDead())
	{
		FRONTIER_LOG(Warning, TEXT("Enemy attack ability activation rejected by basic validation."));
		return false;
	}

	UFrontierAbilitySystemComponent* LocalAbilitySystemComponent = GetFrontierAbilitySystemComponent();
	if (!LocalAbilitySystemComponent)
	{
		FRONTIER_LOG(Warning, TEXT("Enemy attack ability activation failed because ASC is missing."));
		return false;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(FFrontierGameplayTags::Get().AbilityAttackPrimary);
	const bool bActivated = LocalAbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTags);
	return bActivated;
}

void AFrontierEnemyCharacter::GrantStartupAbilities()
{
	

	UFrontierAbilitySystemComponent* LocalAbilitySystemComponent = GetFrontierAbilitySystemComponent();
	if (!LocalAbilitySystemComponent)
	{
		return;
	}

	for (TSubclassOf<UGameplayAbility> AbilityClass : StartupAbilities)
	{
		if (!AbilityClass || LocalAbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass))
		{
			continue;
		}

		LocalAbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
		FRONTIER_LOG(Log, TEXT("Granted enemy startup ability %s to %s"), *GetNameSafe(AbilityClass), *GetNameSafe(this));
	}
}

void AFrontierEnemyCharacter::HandleEnemyStateChanged(const EFrontierEnemyState PreviousState, const EFrontierEnemyState NewState)
{
	if (HasAuthority() && PreviousState == EFrontierEnemyState::Attacking && NewState != EFrontierEnemyState::Attacking)
	{
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
	}

	AAIController* EnemyController = Cast<AAIController>(GetController());
	if (!EnemyController)
	{
		return;
	}

	if (NewState == EFrontierEnemyState::Attacking)
	{
		EnemyController->StopMovement();
		if (IsValid(CombatTarget))
		{
			EnemyController->SetFocus(CombatTarget);
		}
		return;
	}

	if (NewState == EFrontierEnemyState::Chasing && IsValid(CombatTarget))
	{
		EnemyController->SetFocus(CombatTarget);
		return;
	}

	EnemyController->ClearFocus(EAIFocusPriority::Gameplay);
}
