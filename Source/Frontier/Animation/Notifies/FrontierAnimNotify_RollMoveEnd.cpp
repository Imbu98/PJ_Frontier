#include "Animation/Notifies/FrontierAnimNotify_RollMoveEnd.h"

#include "Character/FrontierPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"

FString UFrontierAnimNotify_RollMoveEnd::GetNotifyName_Implementation() const
{
	return TEXT("Frontier Roll Move End");
}

void UFrontierAnimNotify_RollMoveEnd::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (AFrontierPlayerCharacter* PlayerCharacter = MeshComp ? Cast<AFrontierPlayerCharacter>(MeshComp->GetOwner()) : nullptr)
	{
		PlayerCharacter->HandleRollMoveEndNotify();
	}
}
