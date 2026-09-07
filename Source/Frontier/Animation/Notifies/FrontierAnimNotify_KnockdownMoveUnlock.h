#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "FrontierAnimNotify_KnockdownMoveUnlock.generated.h"

UCLASS(meta=(DisplayName="Frontier Knockdown Move Unlock"))
class FRONTIER_API UFrontierAnimNotify_KnockdownMoveUnlock : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
