#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FrontierGameplayAbility.h"
#include "Engine/EngineTypes.h"
#include "FrontierGameplayAbility_AreaSkill.generated.h"

class ACharacter;

struct FRONTIER_API FFrontierAreaSkillTargetLocationParams
{
	const ACharacter* SourceCharacter = nullptr;
	float TargetDistance = 0.0f;
	float MinTargetDistance = 0.0f;
	float MaxTargetDistance = 0.0f;
	float AreaRadius = 0.0f;
	bool bCanOver = false;
	bool bUseNavigationProjection = true;
	float MaxAllowedHeightDifference = 150.0f;
	FVector NavigationProjectionExtent = FVector(200.0f, 200.0f, 500.0f);
	float FloorTraceUpDistance = 1000.0f;
	float FloorTraceDownDistance = 3000.0f;
	float MinFloorNormalZ = 0.5f;
	float ObstacleFrontClearance = 10.0f;
	TEnumAsByte<ECollisionChannel> ObstacleTraceObjectType = ECC_WorldStatic;
	TEnumAsByte<ECollisionChannel> FloorTraceObjectType = ECC_WorldStatic;
	FName TraceTag = NAME_None;
};

UCLASS(Abstract)
class FRONTIER_API UFrontierGameplayAbility_AreaSkill : public UFrontierGameplayAbility
{
	GENERATED_BODY()

public:
	UFrontierGameplayAbility_AreaSkill();

	UFUNCTION(BlueprintCallable, Category="Frontier|Skill|Area")
	void ConfirmAreaSkill();

	UFUNCTION(BlueprintCallable, Category="Frontier|Skill|Area")
	void CancelAreaSkill();

	UFUNCTION(BlueprintCallable, Category="Frontier|Skill|Area")
	virtual void AdjustAreaTargetDistanceInput(float InputAxis);

	UFUNCTION(BlueprintPure, Category="Frontier|Skill|Area")
	bool IsAwaitingAreaConfirm() const;

protected:
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual bool CanConfirmAreaSkill();
	virtual void OnAreaSkillConfirmed();
	virtual void OnAreaSkillCancelled();

	static bool ResolveAreaSkillTargetLocation(
		const FFrontierAreaSkillTargetLocationParams& Params,
		FVector& OutActorLocation,
		FVector& OutGroundLocation);

	bool CanCommitAreaSkillCostAndCooldown() const;
	bool CommitAreaSkillCostAndCooldown();
	void ClearPendingEquipmentAreaSkill();

	UPROPERTY(Transient)
	bool bAreaSkillConfirmed = false;
};
