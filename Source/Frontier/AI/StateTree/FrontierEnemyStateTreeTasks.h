#pragma once

#include "CoreMinimal.h"
#include "Character/FrontierEnemyCharacter.h"
#include "Tasks/StateTreeAITask.h"
#include "Tasks/StateTreeMoveToTask.h"
#include "FrontierEnemyStateTreeTasks.generated.h"

class AActor;

USTRUCT()
struct FFrontierStateTreeTaskSetEnemyStateInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category=Parameter)
	EFrontierEnemyState DesiredState = EFrontierEnemyState::Idle;
};

USTRUCT(meta=(DisplayName="Frontier Set Enemy State", Category="Frontier|AI"))
struct FRONTIER_API FFrontierStateTreeTask_SetEnemyState : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierStateTreeTaskSetEnemyStateInstanceData;

	FFrontierStateTreeTask_SetEnemyState()
	{
		bShouldCallTick = false;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FFrontierStateTreeTaskRefreshMoveLocationInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category=Parameter)
	EFrontierEnemyMoveMode MoveMode = EFrontierEnemyMoveMode::Patrol;
};

USTRUCT(meta=(DisplayName="Frontier Refresh Move Location", Category="Frontier|AI"))
struct FRONTIER_API FFrontierStateTreeTask_RefreshMoveLocation : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierStateTreeTaskRefreshMoveLocationInstanceData;

	FFrontierStateTreeTask_RefreshMoveLocation()
	{
		bShouldCallTick = false;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FFrontierStateTreeTaskSetMoveSpeedInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category=Parameter, meta=(ClampMin="0.0"))
	float MaxWalkSpeed = 300.0f;
};

USTRUCT(meta=(DisplayName="Frontier Set Enemy Move Speed", Category="Frontier|AI"))
struct FRONTIER_API FFrontierStateTreeTask_SetMoveSpeed : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierStateTreeTaskSetMoveSpeedInstanceData;

	FFrontierStateTreeTask_SetMoveSpeed()
	{
		bShouldCallTick = false;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FFrontierStateTreeTaskPerformAttackInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category=Parameter, meta=(ClampMin="0.05"))
	float AttackInterval = 1.0f;

	UPROPERTY(Transient)
	float RemainingAttackTime = 0.0f;
};

USTRUCT(meta=(DisplayName="Frontier Perform Attack", Category="Frontier|AI"))
struct FRONTIER_API FFrontierStateTreeTask_PerformAttack : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierStateTreeTaskPerformAttackInstanceData;

	FFrontierStateTreeTask_PerformAttack()
	{
		bShouldCallTick = true;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
};

USTRUCT(meta=(DisplayName="Frontier Move To Desired Location", Category="Frontier|AI"))
struct FRONTIER_API FFrontierStateTreeTask_MoveToDesiredLocation : public FStateTreeMoveToTask
{
	GENERATED_BODY()

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
