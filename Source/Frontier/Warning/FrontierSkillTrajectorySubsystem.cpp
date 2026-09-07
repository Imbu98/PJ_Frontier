#include "Warning/FrontierSkillTrajectorySubsystem.h"

#include "Engine/World.h"
#include "Frontier.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"

namespace
{
const FName FrontierSkillTrajectoryPositionsParameterName(TEXT("User.Positions"));
}

void UFrontierSkillTrajectorySubsystem::ShowTrajectory(
	const FGuid TrajectoryId,
	UNiagaraSystem* TrajectoryNiagaraSystem,
	const TArray<FVector>& TrajectoryPoints,
	const float Duration)
{
	if (!TrajectoryId.IsValid() || !TrajectoryNiagaraSystem || TrajectoryPoints.Num() < 2)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UNiagaraComponent* TrajectoryComponent = nullptr;
	if (TObjectPtr<UNiagaraComponent>* ExistingComponent = ActiveTrajectoryComponents.Find(TrajectoryId))
	{
		TrajectoryComponent = ExistingComponent->Get();
	}

	if (!TrajectoryComponent)
	{
		TrajectoryComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			TrajectoryNiagaraSystem,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			FVector::OneVector,
			false,
			false);
		if (!TrajectoryComponent)
		{
			return;
		}

		ActiveTrajectoryComponents.Add(TrajectoryId, TrajectoryComponent);
	}
	else if (TrajectoryComponent->GetAsset() != TrajectoryNiagaraSystem)
	{
		TrajectoryComponent->SetAsset(TrajectoryNiagaraSystem);
	}

	if (FTimerHandle* ExistingTimerHandle = ActiveTrajectoryTimers.Find(TrajectoryId))
	{
		World->GetTimerManager().ClearTimer(*ExistingTimerHandle);
		ActiveTrajectoryTimers.Remove(TrajectoryId);
	}

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		TrajectoryComponent,
		FrontierSkillTrajectoryPositionsParameterName,
		TrajectoryPoints);

	// Reinitializing guarantees systems that consume the vector array on startup redraw with the latest path.
	TrajectoryComponent->ReinitializeSystem();

	if (Duration > 0.0f)
	{
		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimer(
			TimerHandle,
			FTimerDelegate::CreateUObject(this, &UFrontierSkillTrajectorySubsystem::HideTrajectory, TrajectoryId),
			Duration,
			false);
		ActiveTrajectoryTimers.Add(TrajectoryId, TimerHandle);
	}
}

void UFrontierSkillTrajectorySubsystem::HideTrajectory(const FGuid TrajectoryId)
{
	if (!TrajectoryId.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (FTimerHandle* TimerHandle = ActiveTrajectoryTimers.Find(TrajectoryId))
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(*TimerHandle);
		}
		ActiveTrajectoryTimers.Remove(TrajectoryId);
	}

	TObjectPtr<UNiagaraComponent> TrajectoryComponent = nullptr;
	if (ActiveTrajectoryComponents.RemoveAndCopyValue(TrajectoryId, TrajectoryComponent) && TrajectoryComponent)
	{
		FRONTIER_LOG(Log, TEXT("Hiding local skill trajectory. TrajectoryId=%s Component=%s"),
			*TrajectoryId.ToString(),
			*GetNameSafe(TrajectoryComponent.Get()));
		TrajectoryComponent->Deactivate();
		TrajectoryComponent->DestroyComponent();
	}
}

void UFrontierSkillTrajectorySubsystem::HideAllTrajectories()
{
	TArray<FGuid> TrajectoryIds;
	ActiveTrajectoryComponents.GetKeys(TrajectoryIds);
	for (const FGuid& TrajectoryId : TrajectoryIds)
	{
		HideTrajectory(TrajectoryId);
	}
}

void UFrontierSkillTrajectorySubsystem::Deinitialize()
{
	HideAllTrajectories();
	Super::Deinitialize();
}
