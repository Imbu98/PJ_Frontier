#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrontierChaosDeathActor.generated.h"

class UGeometryCollectionComponent;
class USoundBase;

/**
 * Cosmetic, locally simulated Geometry Collection spawned when an enemy dies.
 * Create a Blueprint child, assign its Rest Collection, then select that class on the enemy.
 */
UCLASS(Blueprintable)
class FRONTIER_API AFrontierChaosDeathActor : public AActor
{
	GENERATED_BODY()

public:
	AFrontierChaosDeathActor();

	UFUNCTION(BlueprintPure, Category="Frontier|Death|Chaos")
	UGeometryCollectionComponent* GetGeometryCollectionComponent() const { return GeometryCollectionComponent; }

	/** Sets the world-space point from which strain and outward velocity spread. */
	UFUNCTION(BlueprintCallable, Category="Frontier|Death|Chaos")
	void SetDestructionOrigin(FVector ImpactPoint, bool bIsValidImpactPoint = true);

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category="Frontier|Death|Chaos")
	void TriggerDestruction();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Death|Chaos")
	TObjectPtr<UGeometryCollectionComponent> GeometryCollectionComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Death|Chaos", meta=(ClampMin="1.0"))
	float BreakRadius = 250.0f;

	/** Must be higher than the Damage Threshold configured in the Geometry Collection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Death|Chaos", meta=(ClampMin="0.0"))
	float BreakStrain = 1000000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Death|Chaos", meta=(ClampMin="0.0"))
	float OutwardVelocity = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Death|Chaos", meta=(ClampMin="0.0"))
	float DebrisLifetime = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Death|Chaos|Audio")
	TObjectPtr<USoundBase> BreakSound = nullptr;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Frontier|Death|Chaos")
	FVector DestructionOrigin = FVector::ZeroVector;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Frontier|Death|Chaos")
	bool bHasDestructionOrigin = false;
};
