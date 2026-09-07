#include "AI/StateTree/FrontierEnemyStateTreeConditions.h"

#include "AIController.h"
#include "Character/FrontierEnemyCharacter.h"
#include "Frontier.h"
#include "StateTreeExecutionContext.h"

namespace
{
	AFrontierEnemyCharacter* ResolveEnemyCharacterForCondition(AActor* Actor)
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

bool FFrontierStateTreeCondition_HasCombatTarget::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AFrontierEnemyCharacter* EnemyCharacter = ResolveEnemyCharacterForCondition(InstanceData.Actor);

	if (!EnemyCharacter)
	{
		FRONTIER_LOG(Warning, TEXT("Failed to resolve Frontier enemy character for has-target condition."));
		return false;
	}

	if (EnemyCharacter->IsDead())
	{
		return false == InstanceData.bExpectedResult;
	}

	const bool bHasCombatTarget = EnemyCharacter->HasCombatTarget();
	return bHasCombatTarget == InstanceData.bExpectedResult;
}

bool FFrontierStateTreeCondition_CanAttackTarget::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AFrontierEnemyCharacter* EnemyCharacter = ResolveEnemyCharacterForCondition(InstanceData.Actor);

	if (!EnemyCharacter)
	{
		FRONTIER_LOG(Warning, TEXT("Failed to resolve Frontier enemy character for can-attack condition."));
		return false;
	}

	if (EnemyCharacter->IsDead())
	{
		return false == InstanceData.bExpectedResult;
	}

	const bool bCanAttackTarget = EnemyCharacter->IsCombatTargetInAttackRange();
	return bCanAttackTarget == InstanceData.bExpectedResult;
}
