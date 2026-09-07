#pragma once

#include "CoreMinimal.h"
#include "Combat/FrontierHitReactionTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "FrontierMeleeTraceAbilityInterface.generated.h"

USTRUCT(BlueprintType)
struct FFrontierMeleeTraceOverrides
{
	GENERATED_BODY()

	/** Starts a new hit group, allowing actors hit by an earlier notify window to be hit again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trace Override")
	bool bResetHitActorsOnBegin = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trace Override")
	bool bOverrideDamageType = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trace Override", meta=(EditCondition="bOverrideDamageType"))
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trace Override")
	bool bOverrideDamageMultiplier = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trace Override", meta=(EditCondition="bOverrideDamageMultiplier", ClampMin="0.0"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trace Override")
	bool bOverrideHitReaction = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trace Override", meta=(EditCondition="bOverrideHitReaction"))
	FGameplayTag HitReactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trace Override", meta=(EditCondition="bOverrideHitReaction"))
	EFrontierHitReactionLevel HitReactionLevel = EFrontierHitReactionLevel::Light;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trace Override", meta=(EditCondition="bOverrideHitReaction", ClampMin="0.0"))
	float StaggerDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trace Override", meta=(EditCondition="bOverrideHitReaction", ClampMin="0.0"))
	float KnockbackHorizontalStrength = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trace Override", meta=(EditCondition="bOverrideHitReaction", ClampMin="0.0"))
	float KnockbackVerticalStrength = 150.0f;
};

UINTERFACE(MinimalAPI)
class UFrontierMeleeTraceAbilityInterface : public UInterface
{
	GENERATED_BODY()
};

class FRONTIER_API IFrontierMeleeTraceAbilityInterface
{
	GENERATED_BODY()

public:
	virtual void BeginAttackTraceWindow(const FFrontierMeleeTraceOverrides& TraceOverrides) = 0;
	virtual void TickAttackTraceWindow(const FFrontierMeleeTraceOverrides& TraceOverrides) = 0;
	virtual void EndAttackTraceWindow() = 0;
};
