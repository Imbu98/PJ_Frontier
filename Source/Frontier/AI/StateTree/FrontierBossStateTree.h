#pragma once

#include "CoreMinimal.h"
#include "Character/FrontierBossEnemyCharacter.h"
#include "Conditions/StateTreeAIConditionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "Tasks/StateTreeAITask.h"
#include "FrontierBossStateTree.generated.h"

class AActor;

USTRUCT()
struct FFrontierBossCombatEvaluatorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(VisibleAnywhere, Category=Output)
	float TargetDistance = 0.0f;

	UPROPERTY(VisibleAnywhere, Category=Output)
	EFrontierBossDistanceRange DistanceRange = EFrontierBossDistanceRange::None;

	UPROPERTY(VisibleAnywhere, Category=Output)
	bool bHasTarget = false;

	UPROPERTY(VisibleAnywhere, Category=Output)
	bool bActionPlaying = false;
};

USTRUCT(meta=(DisplayName="Frontier Boss Combat Evaluator", Category="Frontier|BossAI"))
struct FRONTIER_API FFrontierBossCombatEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierBossCombatEvaluatorInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;
	virtual void Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
};

USTRUCT()
struct FFrontierBossConditionActorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category=Parameter)
	bool bExpectedResult = true;
};

USTRUCT(meta=(DisplayName="Frontier Boss Can Act", Category="Frontier|BossAI"))
struct FRONTIER_API FFrontierBossCondition_CanAct : public FStateTreeAIConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierBossConditionActorInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

USTRUCT(meta=(DisplayName="Frontier Boss Is Action Playing", Category="Frontier|BossAI"))
struct FRONTIER_API FFrontierBossCondition_IsActionPlaying : public FStateTreeAIConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierBossConditionActorInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

USTRUCT()
struct FFrontierBossConditionDistanceRangeInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category=Parameter)
	EFrontierBossDistanceRange ExpectedRange = EFrontierBossDistanceRange::Close;

	UPROPERTY(EditAnywhere, Category=Parameter)
	bool bExpectedResult = true;
};

USTRUCT(meta=(DisplayName="Frontier Boss Is Distance Range", Category="Frontier|BossAI"))
struct FRONTIER_API FFrontierBossCondition_IsDistanceRange : public FStateTreeAIConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierBossConditionDistanceRangeInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

USTRUCT(meta=(DisplayName="Frontier Boss Is Target In Attack Range", Category="Frontier|BossAI"))
struct FRONTIER_API FFrontierBossCondition_IsTargetInAttackRange : public FStateTreeAIConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierBossConditionActorInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

USTRUCT()
struct FFrontierBossConditionCanUseSkillInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category=Parameter)
	FGameplayTag AbilityTag;

	UPROPERTY(EditAnywhere, Category=Parameter)
	bool bExpectedResult = true;
};

USTRUCT(meta=(DisplayName="Frontier Boss Can Use Skill", Category="Frontier|BossAI"))
struct FRONTIER_API FFrontierBossCondition_CanUseSkill : public FStateTreeAIConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierBossConditionCanUseSkillInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

USTRUCT()
struct FFrontierBossConditionSelectedActionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category=Parameter)
	EFrontierBossCombatAction ExpectedAction = EFrontierBossCombatAction::None;

	UPROPERTY(EditAnywhere, Category=Parameter)
	bool bExpectedResult = true;
};

USTRUCT(meta=(DisplayName="Frontier Boss Is Selected Action", Category="Frontier|BossAI"))
struct FRONTIER_API FFrontierBossCondition_IsSelectedAction : public FStateTreeAIConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierBossConditionSelectedActionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

UENUM()
enum class EFrontierBossMoveRequest : uint8
{
	ChaseTarget,
	SkillRange,
	Reposition,
	ReturnHome
};

USTRUCT()
struct FFrontierBossTaskMoveInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category=Parameter)
	EFrontierBossMoveRequest MoveRequest = EFrontierBossMoveRequest::ChaseTarget;

	UPROPERTY(EditAnywhere, Category=Parameter)
	FGameplayTag AbilityTag;
};

USTRUCT(meta=(DisplayName="Frontier Boss Refresh Move Location", Category="Frontier|BossAI"))
struct FRONTIER_API FFrontierBossTask_RefreshMoveLocation : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierBossTaskMoveInstanceData;

	FFrontierBossTask_RefreshMoveLocation()
	{
		bShouldCallTick = false;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FFrontierBossTaskSelectActionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AActor> Actor = nullptr;
};

USTRUCT(meta=(DisplayName="Frontier Boss Select Weighted Combat Action", Category="Frontier|BossAI"))
struct FRONTIER_API FFrontierBossTask_SelectWeightedCombatAction : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierBossTaskSelectActionInstanceData;

	FFrontierBossTask_SelectWeightedCombatAction()
	{
		bShouldCallTick = false;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FFrontierBossTaskFindRandomTargetInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category=Parameter)
	bool bKeepExistingTargetIfNoCandidate = true;
};

USTRUCT(meta=(DisplayName="Frontier Boss Find Random Target In Range", Category="Frontier|BossAI"))
struct FRONTIER_API FFrontierBossTask_FindRandomTargetInRange : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierBossTaskFindRandomTargetInstanceData;

	FFrontierBossTask_FindRandomTargetInRange()
	{
		bShouldCallTick = false;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FFrontierBossTaskAbilityInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category=Parameter)
	FGameplayTag AbilityTag;

	UPROPERTY(EditAnywhere, Category=Parameter)
	bool bUseWeightedSkillForCurrentRange = false;
};

USTRUCT(meta=(DisplayName="Frontier Boss Activate Ability", Category="Frontier|BossAI"))
struct FRONTIER_API FFrontierBossTask_ActivateAbility : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierBossTaskAbilityInstanceData;

	FFrontierBossTask_ActivateAbility()
	{
		bShouldCallTick = false;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT(meta=(DisplayName="Frontier Boss Activate Selected Ability", Category="Frontier|BossAI"))
struct FRONTIER_API FFrontierBossTask_ActivateSelectedAbility : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierBossTaskSelectActionInstanceData;

	FFrontierBossTask_ActivateSelectedAbility()
	{
		bShouldCallTick = false;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FFrontierBossTaskActorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AActor> Actor = nullptr;
};

USTRUCT()
struct FFrontierBossTaskWaitActionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category=Parameter, meta=(ClampMin="0.1"))
	float MaxWaitTime = 8.0f;

	UPROPERTY(Transient)
	float ElapsedTime = 0.0f;
};

USTRUCT(meta=(DisplayName="Frontier Boss Wait Action Finished", Category="Frontier|BossAI"))
struct FRONTIER_API FFrontierBossTask_WaitActionFinished : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierBossTaskWaitActionInstanceData;

	FFrontierBossTask_WaitActionFinished()
	{
		bShouldCallTick = true;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
};

USTRUCT()
struct FFrontierBossTaskChaseMeleeInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category=Parameter, meta=(ClampMin="0.1"))
	float MaxChaseTime = 3.0f;

	UPROPERTY(EditAnywhere, Category=Parameter, meta=(ClampMin="0.0"))
	float AcceptanceRadius = 150.0f;

	UPROPERTY(EditAnywhere, Category=Parameter, meta=(ClampMin="0.05"))
	float RepathInterval = 0.25f;

	UPROPERTY(Transient)
	float ElapsedTime = 0.0f;

	UPROPERTY(Transient)
	float RepathRemainingTime = 0.0f;
};

USTRUCT(meta=(DisplayName="Frontier Boss Chase Melee Target", Category="Frontier|BossAI"))
struct FRONTIER_API FFrontierBossTask_ChaseMeleeTarget : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierBossTaskChaseMeleeInstanceData;

	FFrontierBossTask_ChaseMeleeTarget()
	{
		bShouldCallTick = true;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT(meta=(DisplayName="Frontier Boss Face Target", Category="Frontier|BossAI"))
struct FRONTIER_API FFrontierBossTask_FaceTarget : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierBossTaskActorInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT(meta=(DisplayName="Frontier Boss Abandon Combat", Category="Frontier|BossAI"))
struct FRONTIER_API FFrontierBossTask_AbandonCombat : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierBossTaskActorInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
