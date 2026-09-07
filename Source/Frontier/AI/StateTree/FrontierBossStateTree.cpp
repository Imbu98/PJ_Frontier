#include "AI/StateTree/FrontierBossStateTree.h"

#include "AIController.h"
#include "Frontier.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"

namespace
{
	AFrontierBossEnemyCharacter* ResolveBossEnemyCharacter(AActor* Actor)
	{
		if (AFrontierBossEnemyCharacter* BossCharacter = Cast<AFrontierBossEnemyCharacter>(Actor))
		{
			return BossCharacter;
		}

		if (const AAIController* AIController = Cast<AAIController>(Actor))
		{
			return Cast<AFrontierBossEnemyCharacter>(AIController->GetPawn());
		}

		return nullptr;
	}
}

void FFrontierBossCombatEvaluator::TreeStart(FStateTreeExecutionContext& Context) const
{
	FRONTIER_LOG_FUNC();
	Tick(Context, 0.0f);
}

void FFrontierBossCombatEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);

	if (!BossCharacter)
	{
		InstanceData.TargetDistance = 0.0f;
		InstanceData.DistanceRange = EFrontierBossDistanceRange::None;
		InstanceData.bHasTarget = false;
		InstanceData.bActionPlaying = false;
		return;
	}

	InstanceData.bHasTarget = BossCharacter->HasCombatTarget();
	InstanceData.TargetDistance = BossCharacter->GetDistanceToCombatTarget();
	InstanceData.DistanceRange = BossCharacter->GetDistanceRangeToCombatTarget();
	InstanceData.bActionPlaying = BossCharacter->IsBossActionPlaying();
}

bool FFrontierBossCondition_CanAct::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	const bool bCanAct = BossCharacter && !BossCharacter->IsDead() && !BossCharacter->IsBossActionPlaying();
	return bCanAct == InstanceData.bExpectedResult;
}

bool FFrontierBossCondition_IsActionPlaying::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	const bool bIsActionPlaying = BossCharacter && !BossCharacter->IsDead() && BossCharacter->IsBossActionPlaying();
	return bIsActionPlaying == InstanceData.bExpectedResult;
}

bool FFrontierBossCondition_IsDistanceRange::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	const bool bMatchesRange = BossCharacter && BossCharacter->GetDistanceRangeToCombatTarget() == InstanceData.ExpectedRange;
	return bMatchesRange == InstanceData.bExpectedResult;
}

bool FFrontierBossCondition_IsTargetInAttackRange::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	const bool bTargetInAttackRange = BossCharacter
		&& !BossCharacter->IsDead()
		&& BossCharacter->HasCombatTarget()
		&& BossCharacter->IsCombatTargetInAttackRange();

	return bTargetInAttackRange == InstanceData.bExpectedResult;
}

bool FFrontierBossCondition_CanUseSkill::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	const bool bCanUseSkill = BossCharacter && BossCharacter->CanUseSkill(InstanceData.AbilityTag);
	return bCanUseSkill == InstanceData.bExpectedResult;
}

bool FFrontierBossCondition_IsSelectedAction::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	const bool bMatchesAction = BossCharacter && BossCharacter->GetSelectedCombatAction() == InstanceData.ExpectedAction;
	return bMatchesAction == InstanceData.bExpectedResult;
}

EStateTreeRunStatus FFrontierBossTask_RefreshMoveLocation::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	if (!BossCharacter || BossCharacter->IsDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	bool bResolvedMoveLocation = false;
	switch (InstanceData.MoveRequest)
	{
	case EFrontierBossMoveRequest::ChaseTarget:
		bResolvedMoveLocation = BossCharacter->RefreshDesiredMoveLocation(EFrontierEnemyMoveMode::Chase);
		break;

	case EFrontierBossMoveRequest::SkillRange:
		bResolvedMoveLocation = BossCharacter->RefreshMoveLocationForSkillRange(InstanceData.AbilityTag);
		break;

	case EFrontierBossMoveRequest::Reposition:
		bResolvedMoveLocation = BossCharacter->RefreshRepositionLocation();
		break;

	case EFrontierBossMoveRequest::ReturnHome:
		bResolvedMoveLocation = BossCharacter->RefreshReturnHomeLocation();
		break;

	default:
		break;
	}

	return bResolvedMoveLocation ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FFrontierBossTask_SelectWeightedCombatAction::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	if (!BossCharacter || BossCharacter->IsDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	return BossCharacter->SelectWeightedCombatAction()
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FFrontierBossTask_FindRandomTargetInRange::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	if (!BossCharacter || BossCharacter->IsDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!BossCharacter->HasAuthority())
	{
		return BossCharacter->HasCombatTarget()
			? EStateTreeRunStatus::Succeeded
			: EStateTreeRunStatus::Failed;
	}

	const bool bHasTargetAfterSearch = BossCharacter->TryFindRandomCombatTargetInRange(InstanceData.bKeepExistingTargetIfNoCandidate);
	return bHasTargetAfterSearch
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FFrontierBossTask_ActivateAbility::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	if (!BossCharacter || BossCharacter->IsDead() || BossCharacter->IsBossActionPlaying())
	{
		return EStateTreeRunStatus::Failed;
	}

	bool bActivated = false;
	if (InstanceData.bUseWeightedSkillForCurrentRange)
	{
		bActivated = BossCharacter->TryActivateWeightedSkillForRange(BossCharacter->GetDistanceRangeToCombatTarget());
	}
	else if (InstanceData.AbilityTag.IsValid())
	{
		bActivated = BossCharacter->TryActivateBossAbility(InstanceData.AbilityTag);
	}
	else
	{
		bActivated = BossCharacter->TryActivateSelectedCombatActionAbility();
	}

	return bActivated ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FFrontierBossTask_ActivateSelectedAbility::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	if (!BossCharacter || BossCharacter->IsDead() || BossCharacter->IsBossActionPlaying())
	{
		return EStateTreeRunStatus::Failed;
	}

	return BossCharacter->TryActivateSelectedCombatActionAbility()
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FFrontierBossTask_WaitActionFinished::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.0f;

	const AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	if (!BossCharacter || BossCharacter->IsDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	return BossCharacter->IsBossActionPlaying()
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FFrontierBossTask_WaitActionFinished::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	if (!BossCharacter || BossCharacter->IsDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!BossCharacter->IsBossActionPlaying())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.ElapsedTime += DeltaTime;
	if (InstanceData.ElapsedTime >= InstanceData.MaxWaitTime)
	{
		FRONTIER_LOG(Warning, TEXT("Boss action wait timed out. Cancelling boss actions. Boss=%s Elapsed=%.2f"),
			*GetNameSafe(BossCharacter),
			InstanceData.ElapsedTime);
		BossCharacter->CancelBossActions();
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FFrontierBossTask_ChaseMeleeTarget::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.0f;
	InstanceData.RepathRemainingTime = 0.0f;

	AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	if (!BossCharacter || BossCharacter->IsDead() || BossCharacter->IsBossActionPlaying() || !BossCharacter->HasCombatTarget())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (BossCharacter->IsCombatTargetInAttackRange())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	AAIController* AIController = Cast<AAIController>(BossCharacter->GetController());
	if (!AIController)
	{
		FRONTIER_LOG(Warning, TEXT("Boss melee chase failed because AIController is missing. Boss=%s"), *GetNameSafe(BossCharacter));
		return EStateTreeRunStatus::Failed;
	}

	BossCharacter->SetEnemyState(EFrontierEnemyState::Chasing);
	AIController->MoveToActor(BossCharacter->GetCombatTarget(), InstanceData.AcceptanceRadius, true, true, true);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FFrontierBossTask_ChaseMeleeTarget::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	if (!BossCharacter || BossCharacter->IsDead() || BossCharacter->IsBossActionPlaying() || !BossCharacter->HasCombatTarget())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (BossCharacter->IsCombatTargetInAttackRange())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.ElapsedTime += DeltaTime;
	if (InstanceData.ElapsedTime >= InstanceData.MaxChaseTime)
	{
		if (AAIController* AIController = Cast<AAIController>(BossCharacter->GetController()))
		{
			AIController->StopMovement();
		}

		return EStateTreeRunStatus::Failed;
	}
	
	InstanceData.RepathRemainingTime -= DeltaTime;
	if (InstanceData.RepathRemainingTime <= 0.0f)
	{
		if (AAIController* AIController = Cast<AAIController>(BossCharacter->GetController()))
		{
			AIController->MoveToActor(BossCharacter->GetCombatTarget(), InstanceData.AcceptanceRadius, true, true, true);
		}
		InstanceData.RepathRemainingTime = InstanceData.RepathInterval;
	}
	return EStateTreeRunStatus::Running;
}

void FFrontierBossTask_ChaseMeleeTarget::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	if (!BossCharacter || BossCharacter->IsDead())
	{
		return;
	}

	if (AAIController* AIController = Cast<AAIController>(BossCharacter->GetController()))
	{
		AIController->StopMovement();
	}
}

EStateTreeRunStatus FFrontierBossTask_FaceTarget::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	if (!BossCharacter || BossCharacter->IsDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	BossCharacter->FaceCombatTarget();
	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FFrontierBossTask_AbandonCombat::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AFrontierBossEnemyCharacter* BossCharacter = ResolveBossEnemyCharacter(InstanceData.Actor);
	if (!BossCharacter)
	{
		return EStateTreeRunStatus::Failed;
	}

	BossCharacter->AbandonCombat();
	return EStateTreeRunStatus::Succeeded;
}
