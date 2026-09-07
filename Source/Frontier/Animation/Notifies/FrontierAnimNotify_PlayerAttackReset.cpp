#include "Animation/Notifies/FrontierAnimNotify_PlayerAttackReset.h"

#include "AbilitySystem/Abilities/FrontierGameplayAbility_PlayerAttack.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "Character/FrontierBaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Frontier.h"

namespace
{
	UFrontierGameplayAbility_PlayerAttack* ResolvePlayerAttackAbilityForResetNotify(USkeletalMeshComponent* MeshComp)
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

			if (UFrontierGameplayAbility_PlayerAttack* AttackAbility = Cast<UFrontierGameplayAbility_PlayerAttack>(AbilitySpec.GetPrimaryInstance()))
			{
				return AttackAbility;
			}
		}

		return nullptr;
	}
}

FString UFrontierAnimNotify_PlayerAttackReset::GetNotifyName_Implementation() const
{
	return TEXT("Frontier Player Attack Reset");
}

void UFrontierAnimNotify_PlayerAttackReset::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (UFrontierGameplayAbility_PlayerAttack* AttackAbility = ResolvePlayerAttackAbilityForResetNotify(MeshComp))
	{
		AttackAbility->ResetAttackStateFromNotify();
		return;
	}

	FRONTIER_LOG(Warning, TEXT("Player attack reset notify could not find active player attack ability. Owner=%s Animation=%s"),
		*GetNameSafe(MeshComp ? MeshComp->GetOwner() : nullptr),
		*GetNameSafe(Animation));
}
