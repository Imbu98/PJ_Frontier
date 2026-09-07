#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "FrontierAnimNotify_IceFloorSpawn.generated.h"

UCLASS(meta=(DisplayName="Frontier Spawn Ice Floor"))
class FRONTIER_API UFrontierAnimNotify_IceFloorSpawn : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
