#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Warning/AttackWarningTypes.h"
#include "AttackWarningActor.generated.h"

class UDecalComponent;
class UNiagaraComponent;
class USceneComponent;

UCLASS()
class FRONTIER_API AAttackWarningActor : public AActor
{
	GENERATED_BODY()

public:
	AAttackWarningActor();

	void ActivateWarning(const FAttackWarningData& WarningData);
	void DeactivateWarning();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Warning")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Warning")
	TObjectPtr<UDecalComponent> DecalComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Warning")
	TObjectPtr<UNiagaraComponent> NiagaraComponent;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Warning", meta=(ClampMin="1.0"))
	float DecalProjectionDepth = 256.0f;
};
