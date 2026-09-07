#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Warning/AttackWarningTypes.h"
#include "AttackWarningSubsystem.generated.h"

class AAttackWarningActor;

UCLASS()
class FRONTIER_API UAttackWarningSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	void ShowWarning(FGuid WarningId, const FAttackWarningData& WarningData);
	void HideWarning(FGuid WarningId);
	void HideAllWarnings();

	virtual void Deinitialize() override;

protected:
	AAttackWarningActor* AcquireWarningActor();
	void ReleaseWarningActor(AAttackWarningActor* WarningActor);

	UPROPERTY(Transient)
	TArray<TObjectPtr<AAttackWarningActor>> AvailableWarnings;

	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<AAttackWarningActor>> ActiveWarnings;

	TMap<FGuid, FTimerHandle> ActiveWarningTimers;
};
