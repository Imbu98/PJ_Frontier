#pragma once

#include "CoreMinimal.h"
#include "Conditions/StateTreeAIConditionBase.h"
#include "FrontierEnemyStateTreeConditions.generated.h"

class AActor;

USTRUCT()
struct FFrontierStateTreeConditionEnemyActorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category=Parameter)
	bool bExpectedResult = true;
};

USTRUCT(meta=(DisplayName="Frontier Has Combat Target", Category="Frontier|AI"))
struct FRONTIER_API FFrontierStateTreeCondition_HasCombatTarget : public FStateTreeAIConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierStateTreeConditionEnemyActorInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

USTRUCT(meta=(DisplayName="Frontier Can Attack Target", Category="Frontier|AI"))
struct FRONTIER_API FFrontierStateTreeCondition_CanAttackTarget : public FStateTreeAIConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FFrontierStateTreeConditionEnemyActorInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
