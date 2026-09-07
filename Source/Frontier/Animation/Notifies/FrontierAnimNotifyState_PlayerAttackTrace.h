#pragma once

#include "AbilitySystem/Abilities/FrontierMeleeTraceAbilityInterface.h"
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "FrontierAnimNotifyState_PlayerAttackTrace.generated.h"

UCLASS(meta=(DisplayName="Frontier Player Attack Trace"))
class FRONTIER_API UFrontierAnimNotifyState_PlayerAttackTrace : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
	const FFrontierMeleeTraceOverrides& GetTraceOverrides() const { return TraceOverrides; }
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Attack Trace")
	FFrontierMeleeTraceOverrides TraceOverrides;
};
