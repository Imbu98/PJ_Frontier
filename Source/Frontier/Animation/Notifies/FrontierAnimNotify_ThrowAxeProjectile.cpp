#include "Animation/Notifies/FrontierAnimNotify_ThrowAxeProjectile.h"

#include "AbilitySystem/Abilities/GA_ThrowAxe.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "Character/FrontierBaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Frontier.h"

namespace
{
UGA_ThrowAxe* ResolveThrowAxeAbility(USkeletalMeshComponent* MeshComp)
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

		if (UGA_ThrowAxe* ThrowAxeAbility = Cast<UGA_ThrowAxe>(AbilitySpec.GetPrimaryInstance()))
		{
			return ThrowAxeAbility;
		}
	}

	return nullptr;
}
}

FString UFrontierAnimNotify_ThrowAxeProjectile::GetNotifyName_Implementation() const
{
	return TEXT("Frontier Throw Axe Projectile");
}

void UFrontierAnimNotify_ThrowAxeProjectile::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (UGA_ThrowAxe* ThrowAxeAbility = ResolveThrowAxeAbility(MeshComp))
	{
		ThrowAxeAbility->ThrowAxeFromNotify();
		return;
	}

	FRONTIER_LOG(Warning, TEXT("Throw axe projectile notify could not find active throw axe ability. Owner=%s Animation=%s"),
		*GetNameSafe(MeshComp ? MeshComp->GetOwner() : nullptr),
		*GetNameSafe(Animation));
}
