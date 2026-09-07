#include "Animation/Notifies/FrontierAnimNotifyState_PlayerAttackTrace.h"

#include "AbilitySystem/Abilities/FrontierMeleeTraceAbilityInterface.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "Character/FrontierBaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	IFrontierMeleeTraceAbilityInterface* ResolveMeleeTraceAbility(USkeletalMeshComponent* MeshComp)
	{
		const AFrontierBaseCharacter* OwnerCharacter = MeshComp ? Cast<AFrontierBaseCharacter>(MeshComp->GetOwner()) : nullptr;
		UFrontierAbilitySystemComponent* ASC = OwnerCharacter ? OwnerCharacter->GetFrontierAbilitySystemComponent() : nullptr;
		if (!ASC)
		{
			return nullptr;
		}

		for (const FGameplayAbilitySpec& AbilitySpec : ASC->GetActivatableAbilities())
		{
			if (!AbilitySpec.IsActive())
			{
				continue;
			}

			UGameplayAbility* AbilityInstance = AbilitySpec.GetPrimaryInstance();
			if (AbilityInstance && AbilityInstance->GetClass()->ImplementsInterface(UFrontierMeleeTraceAbilityInterface::StaticClass()))
			{
				return Cast<IFrontierMeleeTraceAbilityInterface>(AbilityInstance);
			}
		}

		return nullptr;
	}
}

FString UFrontierAnimNotifyState_PlayerAttackTrace::GetNotifyName_Implementation() const
{
	return TEXT("Frontier Player Attack Trace");
}

void UFrontierAnimNotifyState_PlayerAttackTrace::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (IFrontierMeleeTraceAbilityInterface* AttackAbility = ResolveMeleeTraceAbility(MeshComp))
	{
		AttackAbility->BeginAttackTraceWindow(TraceOverrides);
	}
}

void UFrontierAnimNotifyState_PlayerAttackTrace::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (IFrontierMeleeTraceAbilityInterface* AttackAbility = ResolveMeleeTraceAbility(MeshComp))
	{
		AttackAbility->TickAttackTraceWindow(TraceOverrides);
	}
}

void UFrontierAnimNotifyState_PlayerAttackTrace::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (IFrontierMeleeTraceAbilityInterface* AttackAbility = ResolveMeleeTraceAbility(MeshComp))
	{
		AttackAbility->EndAttackTraceWindow();
	}
}
