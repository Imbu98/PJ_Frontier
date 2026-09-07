#include "Animation/Notifies/FrontierAnimNotifyState_WhirlwindDamageWindow.h"

#include "AbilitySystem/Abilities/FrontierWhirlwindDamageWindowInterface.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "Character/FrontierBaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	IFrontierWhirlwindDamageWindowInterface* ResolveWhirlwindAbility(USkeletalMeshComponent* MeshComp)
	{
		const AFrontierBaseCharacter* OwnerCharacter = MeshComp ? Cast<AFrontierBaseCharacter>(MeshComp->GetOwner()) : nullptr;
		UFrontierAbilitySystemComponent* ASC = OwnerCharacter ? OwnerCharacter->GetFrontierAbilitySystemComponent() : nullptr;
		if (!OwnerCharacter || !OwnerCharacter->HasAuthority() || !ASC)
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
			if (AbilityInstance && AbilityInstance->GetClass()->ImplementsInterface(UFrontierWhirlwindDamageWindowInterface::StaticClass()))
			{
				return Cast<IFrontierWhirlwindDamageWindowInterface>(AbilityInstance);
			}
		}

		return nullptr;
	}
}

FString UFrontierAnimNotifyState_WhirlwindDamageWindow::GetNotifyName_Implementation() const
{
	return TEXT("Whirlwind Damage Window");
}

void UFrontierAnimNotifyState_WhirlwindDamageWindow::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (IFrontierWhirlwindDamageWindowInterface* Ability = ResolveWhirlwindAbility(MeshComp))
	{
		Ability->BeginWhirlwindDamageWindow(TotalDuration);
	}
}

void UFrontierAnimNotifyState_WhirlwindDamageWindow::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (IFrontierWhirlwindDamageWindowInterface* Ability = ResolveWhirlwindAbility(MeshComp))
	{
		Ability->EndWhirlwindDamageWindow();
	}
}
