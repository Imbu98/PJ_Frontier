#include "Animation/Notifies/FrontierAnimNotify_GolemThrowStoneProjectile.h"

#include "AbilitySystem/Abilities/GolemAbilities/Ability/GolemRangedAttackAbility.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "Character/FrontierBaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Frontier.h"

namespace
{
	UGolemRangedAttackAbility* ResolveGolemRangedAttackAbilityForThrowNotify(USkeletalMeshComponent* MeshComp)
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

			if (UGolemRangedAttackAbility* RangedAttackAbility = Cast<UGolemRangedAttackAbility>(AbilitySpec.GetPrimaryInstance()))
			{
				return RangedAttackAbility;
			}
		}

		return nullptr;
	}
}

FString UFrontierAnimNotify_GolemThrowStoneProjectile::GetNotifyName_Implementation() const
{
	return TEXT("Frontier Golem Throw Stone Projectile");
}

void UFrontierAnimNotify_GolemThrowStoneProjectile::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	

	if (UGolemRangedAttackAbility* RangedAttackAbility = ResolveGolemRangedAttackAbilityForThrowNotify(MeshComp))
	{
		RangedAttackAbility->ThrowHeldProjectileFromNotify();
	}
}
