#include "AbilitySystem/Abilities/GA_AxeWhirlWind.h"

#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Animation/AnimMontage.h"
#include "Character/FrontierBaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/FrontierCombatComponent.h"
#include "Skill/FrontierSkillDataSubsystem.h"
#include "Tags/FrontierGameplayTags.h"

UGA_AxeWhirlWind::UGA_AxeWhirlWind()
{
	RequiredWeaponTypeTag = FFrontierGameplayTags::Get().WeaponTypeAxe;
	AttackData.bLockMovementDuringAttack = false;
}

void UGA_AxeWhirlWind::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive() || !CachedSourceCharacter || !LoadedAttackMontage)
	{
		return;
	}

	// Montage root motion requires an active movement mode even if old asset defaults locked attacks.
	UnlockCharacterMovementIfNeeded();

	const int32 SkillLevel = FMath::Max(1, GetAbilityLevel(Handle, ActorInfo));
	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	TMap<FGameplayTag, float> Parameters;
	Parameters.Add(Tags.SkillParameterAttackCount, static_cast<float>(BaseAttackCount));
	Parameters.Add(Tags.SkillParameterRadius, WhirlwindRadius);
	Parameters.Add(Tags.SkillParameterDamageMultiplier, AttackData.DamageMultiplier);
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (const UFrontierSkillDataSubsystem* SkillData = GameInstance->GetSubsystem<UFrontierSkillDataSubsystem>())
		{
			SkillData->ApplySkillBalance(GetClass(), SkillLevel, Parameters);
		}
	}

	ResolvedAttackCount = FMath::Max(1, FMath::RoundToInt(Parameters.FindRef(Tags.SkillParameterAttackCount)));
	ResolvedWhirlwindRadius = FMath::Max(0.0f, Parameters.FindRef(Tags.SkillParameterRadius));
	ResolvedDamageMultiplier = FMath::Max(0.0f, Parameters.FindRef(Tags.SkillParameterDamageMultiplier));
	DamagePulsesApplied = 0;
	bDamageWindowStarted = false;
	ApplyWhirlwindCollisionOverrides();
}

void UGA_AxeWhirlWind::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (DamageRepeatTask)
	{
		DamageRepeatTask->EndTask();
		DamageRepeatTask = nullptr;
	}
	RestoreWhirlwindCollisionOverrides();

	ResolvedAttackCount = 0;
	ResolvedWhirlwindRadius = 0.0f;
	ResolvedDamageMultiplier = 1.0f;
	DamagePulsesApplied = 0;
	bDamageWindowStarted = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_AxeWhirlWind::ApplyWhirlwindCollisionOverrides()
{
	if (bWhirlwindCollisionResponsesOverridden || !CachedSourceCharacter)
	{
		return;
	}

	UCapsuleComponent* CapsuleComponent = CachedSourceCharacter->GetCapsuleComponent();
	if (!CapsuleComponent)
	{
		return;
	}

	CachedEnemyCollisionResponse = CapsuleComponent->GetCollisionResponseToChannel(ECC_GameTraceChannel2);
	CachedPlayerCollisionResponse = CapsuleComponent->GetCollisionResponseToChannel(ECC_GameTraceChannel3);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Ignore);
	bWhirlwindCollisionResponsesOverridden = true;
}

void UGA_AxeWhirlWind::RestoreWhirlwindCollisionOverrides()
{
	if (!bWhirlwindCollisionResponsesOverridden)
	{
		return;
	}

	if (CachedSourceCharacter)
	{
		if (UCapsuleComponent* CapsuleComponent = CachedSourceCharacter->GetCapsuleComponent())
		{
			CapsuleComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, CachedEnemyCollisionResponse.GetValue());
			CapsuleComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel3, CachedPlayerCollisionResponse.GetValue());
		}
	}

	bWhirlwindCollisionResponsesOverridden = false;
}

void UGA_AxeWhirlWind::BeginAttackTraceWindow(const FFrontierMeleeTraceOverrides& TraceOverrides)
{
}

void UGA_AxeWhirlWind::TickAttackTraceWindow(const FFrontierMeleeTraceOverrides& TraceOverrides)
{
}

void UGA_AxeWhirlWind::EndAttackTraceWindow()
{
}

void UGA_AxeWhirlWind::BeginWhirlwindDamageWindow(const float WindowDuration)
{
	if (!IsActive() || !CachedSourceCharacter || ResolvedAttackCount <= 0 || bDamageWindowStarted)
	{
		return;
	}
	bDamageWindowStarted = true;
	DamagePulsesApplied = 0;

	// Apply the first pulse immediately, then leave one interval of margin before NotifyEnd.
	ApplyWhirlwindDamagePulse(0);
	const int32 RemainingPulseCount = ResolvedAttackCount - 1;
	if (RemainingPulseCount <= 0)
	{
		return;
	}

	const float PulseInterval = FMath::Max(WindowDuration / static_cast<float>(ResolvedAttackCount), 0.01f);
	DamageRepeatTask = UAbilityTask_Repeat::RepeatAction(
		this,
		PulseInterval,
		RemainingPulseCount);
	if (DamageRepeatTask)
	{
		DamageRepeatTask->OnPerformAction.AddDynamic(this, &UGA_AxeWhirlWind::ApplyWhirlwindDamagePulse);
		DamageRepeatTask->ReadyForActivation();
	}
}

void UGA_AxeWhirlWind::EndWhirlwindDamageWindow()
{
	if (DamageRepeatTask)
	{
		DamageRepeatTask->EndTask();
		DamageRepeatTask = nullptr;
	}
}

float UGA_AxeWhirlWind::ResolveWhirlwindVisualRadius(const UObject* WorldContextObject, const int32 AbilityLevel) const
{
	if (IsActive() && ResolvedWhirlwindRadius > 0.0f)
	{
		return ResolvedWhirlwindRadius;
	}

	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	TMap<FGameplayTag, float> Parameters;
	Parameters.Add(Tags.SkillParameterRadius, WhirlwindRadius);
	if (const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr)
	{
		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (const UFrontierSkillDataSubsystem* SkillData = GameInstance->GetSubsystem<UFrontierSkillDataSubsystem>())
			{
				SkillData->ApplySkillBalance(GetClass(), FMath::Max(1, AbilityLevel), Parameters);
			}
		}
	}

	return FMath::Max(0.0f, Parameters.FindRef(Tags.SkillParameterRadius));
}

void UGA_AxeWhirlWind::ApplyWhirlwindDamagePulse(const int32 ActionNumber)
{
	if (!IsActive()
		|| !CachedSourceCharacter
		|| !CachedSourceCharacter->HasAuthority()
		|| DamagePulsesApplied >= ResolvedAttackCount)
	{
		return;
	}
	++DamagePulsesApplied;

	UFrontierCombatComponent* CombatComponent = CachedSourceCharacter->GetCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	CombatComponent->ResetHitActorsThisAttack();
	const FVector Center = CachedSourceCharacter->GetActorLocation();
	const FGameplayTag DamageTypeTag = AttackData.DamageTypeTag.IsValid()
		? AttackData.DamageTypeTag
		: FFrontierGameplayTags::Get().DamageTypeNormal;
	CombatComponent->ApplySphereTraceDamageBetweenPoints(
		Center,
		Center,
		ResolvedWhirlwindRadius,
		0.0f,
		DamageTypeTag,
		LoadedDamageEffectClass,
		&LoadedAdditionalHitEffectClasses,
		ResolvedDamageMultiplier,
		AttackData.bDrawDebugTrace,
		CachedWeaponElementalType,
		AttackData.HitReactionTag,
		AttackData.HitReactionLevel,
		AttackData.StaggerDuration,
		AttackData.KnockbackHorizontalStrength,
		AttackData.KnockbackVerticalStrength);
}
