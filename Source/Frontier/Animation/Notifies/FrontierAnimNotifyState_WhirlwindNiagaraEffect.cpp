#include "Animation/Notifies/FrontierAnimNotifyState_WhirlwindNiagaraEffect.h"

#include "AbilitySystem/Abilities/FrontierWhirlwindDamageWindowInterface.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "Character/FrontierBaseCharacter.h"
#include "NiagaraComponent.h"

namespace
{
	float ResolveWhirlwindRadius(USkeletalMeshComponent* MeshComp)
	{
		const AFrontierBaseCharacter* OwnerCharacter = MeshComp ? Cast<AFrontierBaseCharacter>(MeshComp->GetOwner()) : nullptr;
		const UFrontierAbilitySystemComponent* ASC = OwnerCharacter ? OwnerCharacter->GetFrontierAbilitySystemComponent() : nullptr;
		if (!ASC)
		{
			return 0.0f;
		}

		for (const FGameplayAbilitySpec& AbilitySpec : ASC->GetActivatableAbilities())
		{
			UGameplayAbility* Ability = AbilitySpec.IsActive() ? AbilitySpec.GetPrimaryInstance() : nullptr;
			if (!Ability)
			{
				Ability = AbilitySpec.Ability.Get();
			}

			if (Ability && Ability->GetClass()->ImplementsInterface(UFrontierWhirlwindDamageWindowInterface::StaticClass()))
			{
				if (const IFrontierWhirlwindDamageWindowInterface* WhirlwindAbility = Cast<IFrontierWhirlwindDamageWindowInterface>(Ability))
				{
					return WhirlwindAbility->ResolveWhirlwindVisualRadius(MeshComp, AbilitySpec.Level);
				}
			}
		}

		return 0.0f;
	}
}

FString UFrontierAnimNotifyState_WhirlwindNiagaraEffect::GetNotifyName_Implementation() const
{
	return TEXT("Whirlwind Niagara Effect");
}

void UFrontierAnimNotifyState_WhirlwindNiagaraEffect::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	UNiagaraComponent* NiagaraComponent = Cast<UNiagaraComponent>(GetSpawnedEffect(MeshComp));
	if (!NiagaraComponent || RadiusScaleParameterName.IsNone())
	{
		return;
	}

	const float ResolvedRadius = ResolveWhirlwindRadius(MeshComp);
	const float VisualScale = ResolvedRadius > 0.0f
		? ResolvedRadius / FMath::Max(ReferenceRadius, 1.0f)
		: 1.0f;
	NiagaraComponent->SetVariableFloat(RadiusScaleParameterName, VisualScale);
}
