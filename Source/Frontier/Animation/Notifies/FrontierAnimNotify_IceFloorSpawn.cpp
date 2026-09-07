#include "Animation/Notifies/FrontierAnimNotify_IceFloorSpawn.h"

#include "AbilitySystem/Abilities/FrontierIceFloorSpawnAbilityInterface.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "Character/FrontierBaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"

FString UFrontierAnimNotify_IceFloorSpawn::GetNotifyName_Implementation() const
{
	return TEXT("Spawn Ice Floor");
}

void UFrontierAnimNotify_IceFloorSpawn::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	const AFrontierBaseCharacter* OwnerCharacter = MeshComp ? Cast<AFrontierBaseCharacter>(MeshComp->GetOwner()) : nullptr;
	UFrontierAbilitySystemComponent* ASC = OwnerCharacter ? OwnerCharacter->GetFrontierAbilitySystemComponent() : nullptr;
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority() || !ASC)
	{
		return;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : ASC->GetActivatableAbilities())
	{
		if (!AbilitySpec.IsActive())
		{
			continue;
		}

		UGameplayAbility* AbilityInstance = AbilitySpec.GetPrimaryInstance();
		if (AbilityInstance && AbilityInstance->GetClass()->ImplementsInterface(UFrontierIceFloorSpawnAbilityInterface::StaticClass()))
		{
			Cast<IFrontierIceFloorSpawnAbilityInterface>(AbilityInstance)->SpawnIceFloorFromNotify();
			return;
		}
	}
}
