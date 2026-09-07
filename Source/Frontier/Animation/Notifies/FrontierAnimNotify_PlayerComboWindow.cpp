#include "Animation/Notifies/FrontierAnimNotify_PlayerComboWindow.h"

#include "AbilitySystem/Abilities/FrontierGameplayAbility_PlayerAttack.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "Character/FrontierBaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	UFrontierGameplayAbility_PlayerAttack* ResolvePlayerAttackAbilityForComboNotify(USkeletalMeshComponent* MeshComp)
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

FString UFrontierAnimNotify_PlayerComboWindow::GetNotifyName_Implementation() const
{
	return TEXT("Frontier Player Combo Window");
}

void UFrontierAnimNotify_PlayerComboWindow::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (UFrontierGameplayAbility_PlayerAttack* AttackAbility = ResolvePlayerAttackAbilityForComboNotify(MeshComp))
	{
		AttackAbility->OpenComboAttackWindowFromNotify();
	}
}
