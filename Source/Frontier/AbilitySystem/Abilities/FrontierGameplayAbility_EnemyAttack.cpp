#include "AbilitySystem/Abilities/FrontierGameplayAbility_EnemyAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Character/FrontierEnemyCharacter.h"
#include "Frontier.h"
#include "AIController.h"
#include "Tags/FrontierGameplayTags.h"
#include "TimerManager.h"

UFrontierGameplayAbility_EnemyAttack::UFrontierGameplayAbility_EnemyAttack()
{
	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.AbilityAttackPrimary);
	SetAssetTags(AssetTags);
	
	ActivationOwnedTags.AddTag(Tags.StateActionAttacking);
	ActivationBlockedTags.AddTag(Tags.StateDead);
	ActivationBlockedTags.AddTag(Tags.StateCCStun);
	ActivationBlockedTags.AddTag(Tags.StateActionAttacking);
	ActivationBlockedTags.AddTag(Tags.StateCombatSkill);
	ActivationBlockedTags.AddTag(Tags.StateCombatSkillDash);
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

bool UFrontierGameplayAbility_EnemyAttack::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AFrontierEnemyCharacter* EnemyCharacter = ActorInfo ? Cast<AFrontierEnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	return EnemyCharacter
		&& !EnemyCharacter->IsDead()
		&& EnemyCharacter->HasCombatTarget()
		&& EnemyCharacter->IsCombatTargetInAttackRange()
		&& EnemyCharacter->HasValidAttackDefinition();
}

void UFrontierGameplayAbility_EnemyAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	FRONTIER_LOG_FUNC();
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	CachedEnemyCharacter = ActorInfo ? Cast<AFrontierEnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!CachedEnemyCharacter || !CachedEnemyCharacter->PrepareRandomAttackDefinitionForAbility())
	{
		FRONTIER_LOG(Warning, TEXT("Enemy attack ability activation failed because attack data could not be prepared."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAnimMontage* AttackMontage = CachedEnemyCharacter->GetCurrentAttackMontage();
	if (!AttackMontage)
	{
		FRONTIER_LOG(Warning, TEXT("Enemy attack ability activation failed because selected montage is null."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FRONTIER_LOG(Warning, TEXT("Enemy attack ability activation failed because CommitAbility was rejected."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (AActor* CombatTarget = CachedEnemyCharacter->GetCombatTarget())
	{
		if (AAIController* AIController = Cast<AAIController>(CachedEnemyCharacter->GetController()))
		{
			AIController->StopMovement();
			AIController->ClearFocus(EAIFocusPriority::Gameplay);
		}

		const FVector DirectionToTarget = (CombatTarget->GetActorLocation() - CachedEnemyCharacter->GetActorLocation()).GetSafeNormal2D();
		if (!DirectionToTarget.IsNearlyZero())
		{
			CachedEnemyCharacter->SetActorRotation(DirectionToTarget.Rotation());
		}
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		AttackMontage);

	if (!MontageTask)
	{
		FRONTIER_LOG(Warning, TEXT("Enemy attack ability activation failed because montage task creation failed."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UFrontierGameplayAbility_EnemyAttack::HandleMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UFrontierGameplayAbility_EnemyAttack::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UFrontierGameplayAbility_EnemyAttack::HandleMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &UFrontierGameplayAbility_EnemyAttack::HandleMontageCancelled);
	MontageTask->ReadyForActivation();

	if (UWorld* World = GetWorld())
	{
		const float TimeoutSeconds = FMath::Max(AttackMontage->GetPlayLength(), 0.1f) + 0.25f;
		World->GetTimerManager().SetTimer(
			AttackAbilityTimeoutTimerHandle,
			this,
			&UFrontierGameplayAbility_EnemyAttack::HandleMontageCompleted,
			TimeoutSeconds,
			false);
	}
}

void UFrontierGameplayAbility_EnemyAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackAbilityTimeoutTimerHandle);
	}

	if (CachedEnemyCharacter)
	{
		CachedEnemyCharacter->EndCurrentAttackTrace();
	}

	CachedEnemyCharacter = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UFrontierGameplayAbility_EnemyAttack::HandleMontageCompleted()
{
	K2_EndAbility();
}

void UFrontierGameplayAbility_EnemyAttack::HandleMontageCancelled()
{
	K2_CancelAbility();
}
