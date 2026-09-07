#pragma once

#include "CoreMinimal.h"
#include "AnimNotifyState_TimedNiagaraEffect.h"
#include "FrontierAnimNotifyState_WhirlwindNiagaraEffect.generated.h"

/** Timed whirlwind effect whose Niagara radius follows the resolved skill radius. */
UCLASS(meta=(DisplayName="Frontier Whirlwind Niagara Effect"))
class FRONTIER_API UFrontierAnimNotifyState_WhirlwindNiagaraEffect : public UAnimNotifyState_TimedNiagaraEffect
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Whirlwind|Niagara")
	FName RadiusScaleParameterName = TEXT("User.Scale_All");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Whirlwind|Niagara", meta=(ClampMin="1.0"))
	float ReferenceRadius = 140.0f;
};
