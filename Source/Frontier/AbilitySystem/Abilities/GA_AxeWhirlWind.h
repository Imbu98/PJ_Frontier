#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FrontierGameplayAbility_MeleeMontageSkill.h"
#include "AbilitySystem/Abilities/FrontierWhirlwindDamageWindowInterface.h"
#include "GA_AxeWhirlWind.generated.h"

class UAbilityTask_Repeat;

/** Axe whirlwind skill with notify-window damage pulses and montage-authored root motion. */
UCLASS()
class FRONTIER_API UGA_AxeWhirlWind : public UFrontierGameplayAbility_MeleeMontageSkill, public IFrontierWhirlwindDamageWindowInterface
{
	GENERATED_BODY()

public:
	UGA_AxeWhirlWind();

	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// Whirlwind owns its hit cadence instead of using montage notify trace windows.
	virtual void BeginAttackTraceWindow(const FFrontierMeleeTraceOverrides& TraceOverrides) override;
	virtual void TickAttackTraceWindow(const FFrontierMeleeTraceOverrides& TraceOverrides) override;
	virtual void EndAttackTraceWindow() override;
	virtual void BeginWhirlwindDamageWindow(float WindowDuration) override;
	virtual void EndWhirlwindDamageWindow() override;
	virtual float ResolveWhirlwindVisualRadius(const UObject* WorldContextObject, int32 AbilityLevel) const override;

protected:
	UFUNCTION()
	void ApplyWhirlwindDamagePulse(int32 ActionNumber);
	void ApplyWhirlwindCollisionOverrides();
	void RestoreWhirlwindCollisionOverrides();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Skill|Whirlwind|Damage", meta=(ClampMin="1"))
	int32 BaseAttackCount = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Skill|Whirlwind|Damage", meta=(ClampMin="0.0"))
	float WhirlwindRadius = 140.0f;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_Repeat> DamageRepeatTask;

	int32 ResolvedAttackCount = 0;
	float ResolvedWhirlwindRadius = 0.0f;
	float ResolvedDamageMultiplier = 1.0f;
	int32 DamagePulsesApplied = 0;
	bool bDamageWindowStarted = false;
	TEnumAsByte<ECollisionResponse> CachedEnemyCollisionResponse = ECR_Block;
	TEnumAsByte<ECollisionResponse> CachedPlayerCollisionResponse = ECR_Block;
	bool bWhirlwindCollisionResponsesOverridden = false;
};
