#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "FrontierAnimNotify_PlayerComboWindow.generated.h"

UCLASS(meta=(DisplayName="Frontier Player Combo Window"))
class FRONTIER_API UFrontierAnimNotify_PlayerComboWindow : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
