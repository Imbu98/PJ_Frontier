#include "Animation/Notifies/FrontierAnimNotify_BossDashLanding.h"

#include "AbilitySystem/Abilities/GolemAbilities/Ability/GA_BossDashAttack.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "Character/FrontierBaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	UGA_BossDashAttack* ResolveBossDashLandingAbility(USkeletalMeshComponent* MeshComp)
	{
		const AFrontierBaseCharacter* OwnerCharacter = MeshComp
			? Cast<AFrontierBaseCharacter>(MeshComp->GetOwner())
			: nullptr;
		UFrontierAbilitySystemComponent* ASC = OwnerCharacter
			? OwnerCharacter->GetFrontierAbilitySystemComponent()
			: nullptr;
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

FString UFrontierAnimNotify_BossDashLanding::GetNotifyName_Implementation() const
{
	return TEXT("Frontier Boss Dash Landing");
}

void UFrontierAnimNotify_BossDashLanding::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (UGA_BossDashAttack* DashAttackAbility = ResolveBossDashLandingAbility(MeshComp))
	{
		DashAttackAbility->HandleLandingFromNotify();
	}
}
