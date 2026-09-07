#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "FrontierSkillTrajectorySubsystem.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

UCLASS()
class FRONTIER_API UFrontierSkillTrajectorySubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	void ShowTrajectory(FGuid TrajectoryId, UNiagaraSystem* TrajectoryNiagaraSystem, const TArray<FVector>& TrajectoryPoints, float Duration);
	void HideTrajectory(FGuid TrajectoryId);
	void HideAllTrajectories();

	virtual void Deinitialize() override;

private:
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UNiagaraComponent>> ActiveTrajectoryComponents;

	TMap<FGuid, FTimerHandle> ActiveTrajectoryTimers;
};
