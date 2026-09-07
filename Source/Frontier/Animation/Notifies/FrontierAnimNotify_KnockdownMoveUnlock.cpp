#include "Animation/Notifies/FrontierAnimNotify_KnockdownMoveUnlock.h"

FString UFrontierAnimNotify_KnockdownMoveUnlock::GetNotifyName_Implementation() const
{
	return TEXT("Frontier Knockdown Move Unlock");
}

void UFrontierAnimNotify_KnockdownMoveUnlock::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
}
