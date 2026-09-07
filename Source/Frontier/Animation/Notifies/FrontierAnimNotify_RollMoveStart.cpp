#include "Animation/Notifies/FrontierAnimNotify_RollMoveStart.h"

#include "Character/FrontierPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"

FString UFrontierAnimNotify_RollMoveStart::GetNotifyName_Implementation() const
{
	return TEXT("Frontier Roll Move Start");
}

void UFrontierAnimNotify_RollMoveStart::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (AFrontierPlayerCharacter* PlayerCharacter = MeshComp ? Cast<AFrontierPlayerCharacter>(MeshComp->GetOwner()) : nullptr)
	{
		PlayerCharacter->HandleRollMoveStartNotify();
	}
}
