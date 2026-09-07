#include "Warning/AttackWarningSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Frontier.h"
#include "Warning/AttackWarningActor.h"

void UAttackWarningSubsystem::ShowWarning(const FGuid WarningId, const FAttackWarningData& WarningData)
{
	FRONTIER_LOG(Log, TEXT("Showing pooled attack warning. WarningId=%s Location=%s Radius=%.2f Duration=%.2f"),
		*WarningId.ToString(),
		*WarningData.WarningLocation.ToCompactString(),
		WarningData.WarningRadius,
		WarningData.WarningDuration);

	if (!WarningId.IsValid())
	{
		return;
	}

	if (TObjectPtr<AAttackWarningActor>* ActiveWarningActor = ActiveWarnings.Find(WarningId))
	{
		if (*ActiveWarningActor)
		{
			(*ActiveWarningActor)->ActivateWarning(WarningData);

			if (FTimerHandle* ExistingTimerHandle = ActiveWarningTimers.Find(WarningId))
			{
				if (UWorld* World = GetWorld())
				{
					World->GetTimerManager().ClearTimer(*ExistingTimerHandle);
				}
				ActiveWarningTimers.Remove(WarningId);
			}

			if (WarningData.WarningDuration > 0.0f)
			{
				FTimerHandle TimerHandle;
				GetWorld()->GetTimerManager().SetTimer(
					TimerHandle,
					FTimerDelegate::CreateUObject(this, &UAttackWarningSubsystem::HideWarning, WarningId),
					WarningData.WarningDuration,
					false);
				ActiveWarningTimers.Add(WarningId, TimerHandle);
			}
			return;
		}
	}

	AAttackWarningActor* WarningActor = AcquireWarningActor();
	if (!WarningActor)
	{
		return;
	}

	WarningActor->ActivateWarning(WarningData);
	ActiveWarnings.Add(WarningId, WarningActor);

	if (WarningData.WarningDuration > 0.0f)
	{
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(
			TimerHandle,
			FTimerDelegate::CreateUObject(this, &UAttackWarningSubsystem::HideWarning, WarningId),
			WarningData.WarningDuration,
			false);
		ActiveWarningTimers.Add(WarningId, TimerHandle);
	}
}

void UAttackWarningSubsystem::HideWarning(const FGuid WarningId)
{
	if (!WarningId.IsValid())
	{
		return;
	}

	if (FTimerHandle* TimerHandle = ActiveWarningTimers.Find(WarningId))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(*TimerHandle);
		}
		ActiveWarningTimers.Remove(WarningId);
	}

	TObjectPtr<AAttackWarningActor> WarningActor = nullptr;
	if (ActiveWarnings.RemoveAndCopyValue(WarningId, WarningActor) && WarningActor)
	{
		FRONTIER_LOG(Log, TEXT("Hiding pooled attack warning. WarningId=%s Actor=%s"),
			*WarningId.ToString(),
			*GetNameSafe(WarningActor.Get()));
		ReleaseWarningActor(WarningActor);
	}
}

void UAttackWarningSubsystem::HideAllWarnings()
{
	TArray<FGuid> WarningIds;
	ActiveWarnings.GetKeys(WarningIds);
	for (const FGuid& WarningId : WarningIds)
	{
		HideWarning(WarningId);
	}
}

void UAttackWarningSubsystem::Deinitialize()
{
	HideAllWarnings();

	for (AAttackWarningActor* WarningActor : AvailableWarnings)
	{
		if (WarningActor)
		{
			WarningActor->Destroy();
		}
	}
	AvailableWarnings.Reset();

	Super::Deinitialize();
}

AAttackWarningActor* UAttackWarningSubsystem::AcquireWarningActor()
{
	if (!AvailableWarnings.IsEmpty())
	{
		AAttackWarningActor* WarningActor = AvailableWarnings.Pop();
		return WarningActor;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAttackWarningActor* WarningActor = World->SpawnActor<AAttackWarningActor>(
		AAttackWarningActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);
	if (WarningActor)
	{
		WarningActor->DeactivateWarning();
	}
	return WarningActor;
}

void UAttackWarningSubsystem::ReleaseWarningActor(AAttackWarningActor* WarningActor)
{
	if (!WarningActor)
	{
		return;
	}

	WarningActor->DeactivateWarning();
	AvailableWarnings.Add(WarningActor);
}
