#include "Animation/Notifies/FrontierAnimNotify_ConsumePotion.h"

#include "Character/FrontierPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"

FString UFrontierAnimNotify_ConsumePotion::GetNotifyName_Implementation() const
{
	return TEXT("Frontier Consume Potion");
}

void UFrontierAnimNotify_ConsumePotion::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (AFrontierPlayerCharacter* PlayerCharacter = MeshComp ? Cast<AFrontierPlayerCharacter>(MeshComp->GetOwner()) : nullptr)
	{
		PlayerCharacter->HandleConsumableUseNotify();
	}
}
