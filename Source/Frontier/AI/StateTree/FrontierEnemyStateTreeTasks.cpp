#include "AI/StateTree/FrontierEnemyStateTreeTasks.h"

#include "AIController.h"
#include "Character/FrontierBossEnemyCharacter.h"
#include "Frontier.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "StateTreeExecutionContext.h"

namespace
{
	AFrontierEnemyCharacter* ResolveEnemyCharacter(AActor* Actor)
	{
		if (AFrontierEnemyCharacter* EnemyCharacter = Cast<AFrontierEnemyCharacter>(Actor))
		{
			return EnemyCharacter;
		}

		if (const AAIController* AIController = Cast<AAIController>(Actor))
		{
			return Cast<AFrontierEnemyCharacter>(AIController->GetPawn());
		}

		return nullptr;
	}
}

EStateTreeRunStatus FFrontierStateTreeTask_SetEnemyState::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AFrontierEnemyCharacter* EnemyCharacter = ResolveEnemyCharacter(InstanceData.Actor);

	if (!EnemyCharacter)
	{
		FRONTIER_LOG(Warning, TEXT("Failed to resolve Frontier enemy character for state task."));
		return EStateTreeRunStatus::Failed;
	}

	if (EnemyCharacter->IsDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	EnemyCharacter->SetEnemyState(InstanceData.DesiredState);

	// This task prepares the state. The action task in the same state decides when the state completes.
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FFrontierStateTreeTask_RefreshMoveLocation::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AFrontierEnemyCharacter* EnemyCharacter = ResolveEnemyCharacter(InstanceData.Actor);

	if (!EnemyCharacter)
	{
		FRONTIER_LOG(Warning, TEXT("Failed to resolve Frontier enemy character for move refresh task."));
		return EStateTreeRunStatus::Failed;
	}

	if (EnemyCharacter->IsDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!EnemyCharacter->RefreshDesiredMoveLocation(InstanceData.MoveMode))
	{
		return EStateTreeRunStatus::Failed;
	}

	// Keep the state active until MoveTo reaches the selected destination.
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FFrontierStateTreeTask_SetMoveSpeed::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AFrontierEnemyCharacter* EnemyCharacter = ResolveEnemyCharacter(InstanceData.Actor);

	if (!EnemyCharacter)
	{
		FRONTIER_LOG(Warning, TEXT("Failed to resolve Frontier enemy character for move speed task."));
		return EStateTreeRunStatus::Failed;
	}

	if (EnemyCharacter->IsDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!EnemyCharacter->GetCharacterMovement())
	{
		FRONTIER_LOG(Warning, TEXT("Move speed task failed because CharacterMovementComponent is missing."));
		return EStateTreeRunStatus::Failed;
	}

	EnemyCharacter->SetDesiredMaxWalkSpeed(InstanceData.MaxWalkSpeed);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FFrontierStateTreeTask_PerformAttack::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AFrontierEnemyCharacter* EnemyCharacter = ResolveEnemyCharacter(InstanceData.Actor);

	if (!EnemyCharacter)
	{
		FRONTIER_LOG(Warning, TEXT("Failed to resolve Frontier enemy character for attack task."));
		return EStateTreeRunStatus::Failed;
	}

	if (EnemyCharacter->IsDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!EnemyCharacter->HasCombatTarget() || !EnemyCharacter->IsCombatTargetInAttackRange())
	{
		if (AFrontierBossEnemyCharacter* BossCharacter = Cast<AFrontierBossEnemyCharacter>(EnemyCharacter))
		{
			BossCharacter->ClearSelectedCombatAction();
		}
		return EStateTreeRunStatus::Failed;
	}

	if (!EnemyCharacter->TryPerformAttack())
	{
		FRONTIER_LOG(Warning, TEXT("Attack task failed to start the attack behavior."));
		if (AFrontierBossEnemyCharacter* BossCharacter = Cast<AFrontierBossEnemyCharacter>(EnemyCharacter))
		{
			BossCharacter->ClearSelectedCombatAction();
		}
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.RemainingAttackTime = FMath::Max3(
		InstanceData.AttackInterval,
		EnemyCharacter->GetLastAttackMontageDuration(),
		0.05f);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FFrontierStateTreeTask_PerformAttack::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AFrontierEnemyCharacter* EnemyCharacter = ResolveEnemyCharacter(InstanceData.Actor);

	if (!EnemyCharacter)
	{
		FRONTIER_LOG(Warning, TEXT("Attack task stopped because the enemy character is unavailable."));
		return EStateTreeRunStatus::Failed;
	}

	if (EnemyCharacter->IsDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.RemainingAttackTime -= DeltaTime;
	if (InstanceData.RemainingAttackTime > 0.0f)
	{
		return EStateTreeRunStatus::Running;
	}

	if (!EnemyCharacter->HasCombatTarget() || !EnemyCharacter->IsCombatTargetInAttackRange())
	{
		if (AFrontierBossEnemyCharacter* BossCharacter = Cast<AFrontierBossEnemyCharacter>(EnemyCharacter))
		{
			BossCharacter->ClearSelectedCombatAction();
		}
		return EStateTreeRunStatus::Failed;
	}

	if (AFrontierBossEnemyCharacter* BossCharacter = Cast<AFrontierBossEnemyCharacter>(EnemyCharacter))
	{
		BossCharacter->ClearSelectedCombatAction();
		return EStateTreeRunStatus::Succeeded;
	}

	if (!EnemyCharacter->TryPerformAttack())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.RemainingAttackTime = FMath::Max3(
		InstanceData.AttackInterval,
		EnemyCharacter->GetLastAttackMontageDuration(),
		0.05f);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FFrontierStateTreeTask_MoveToDesiredLocation::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AFrontierEnemyCharacter* EnemyCharacter = ResolveEnemyCharacter(InstanceData.AIController.Get());

	if (!InstanceData.AIController)
	{
		FRONTIER_LOG(Warning, TEXT("MoveToDesiredLocation failed because AIController context is missing."));
		return EStateTreeRunStatus::Failed;
	}

	if (!EnemyCharacter)
	{
		FRONTIER_LOG(Warning, TEXT("MoveToDesiredLocation failed because Frontier enemy character could not be resolved."));
		return EStateTreeRunStatus::Failed;
	}

	if (EnemyCharacter->IsDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (EnemyCharacter->GetEnemyState() == EFrontierEnemyState::Chasing && EnemyCharacter->HasCombatTarget())
	{
		InstanceData.TargetActor = EnemyCharacter->GetCombatTarget();
	}
	else
	{
		InstanceData.TargetActor = nullptr;
		InstanceData.Destination = EnemyCharacter->GetDesiredMoveLocation();
	}

	return FStateTreeMoveToTask::EnterState(Context, Transition);
}
