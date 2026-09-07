#include "Animation/Notifies/FrontierAnimNotify_BossDashStart.h"

#include "AbilitySystem/Abilities/GolemAbilities/Ability/GA_BossDashAttack.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "Character/FrontierBaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Frontier.h"

namespace
{
	UGA_BossDashAttack* ResolveBossDashAttackAbility(USkeletalMeshComponent* MeshComp)
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

			if (UGA_BossDashAttack* DashAttackAbility = Cast<UGA_BossDashAttack>(AbilitySpec.GetPrimaryInstance()))
			{
				return DashAttackAbility;
			}
		}

		return nullptr;
	}
}

FString UFrontierAnimNotify_BossDashStart::GetNotifyName_Implementation() const
{
	return TEXT("Frontier Boss Dash Start");
}

void UFrontierAnimNotify_BossDashStart::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	

	if (UGA_BossDashAttack* DashAttackAbility = ResolveBossDashAttackAbility(MeshComp))
	{
		DashAttackAbility->StartDashMoveFromNotify();
	}
}
