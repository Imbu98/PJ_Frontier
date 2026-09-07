#pragma once

#include "CoreMinimal.h"
#include "Combat/FrontierDamageStatics.h"
#include "GameFramework/Actor.h"
#include "FrontierIceFloorActor.generated.h"

class AFrontierBaseCharacter;
class UNiagaraComponent;
class USphereComponent;

UCLASS()
class FRONTIER_API AFrontierIceFloorActor : public AActor
{
	GENERATED_BODY()

public:
	AFrontierIceFloorActor();

	void InitializeIceFloor(
		AFrontierBaseCharacter* InSourceCharacter,
		float InRadius,
		float InDuration,
		float InDamageInterval,
		float InDamageMultiplier,
		float InSlowMultiplier,
		const FFrontierDamageRequest& InDamageRequest);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void RefreshSlowedCharacters();
	void ApplyPeriodicDamage();
	void GatherValidTargets(TSet<AFrontierBaseCharacter*>& OutTargets) const;
	void RemoveAllSlowModifiers();
	void RefreshRadiusPresentation();

	UFUNCTION()
	void OnRep_Radius();

	UFUNCTION()
	void OnRep_Duration();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ice Floor")
	TObjectPtr<USphereComponent> EffectSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ice Floor")
	TObjectPtr<UNiagaraComponent> FloorEffect;

	UPROPERTY(ReplicatedUsing=OnRep_Radius, VisibleAnywhere, BlueprintReadOnly, Category="Ice Floor")
	float Radius = 300.0f;

	UPROPERTY(ReplicatedUsing=OnRep_Duration, VisibleAnywhere, BlueprintReadOnly, Category="Ice Floor")
	float Duration = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category="Ice Floor", meta=(ClampMin="0.05"))
	float SlowRefreshInterval = 0.2f;

	/** World-space radius used as the baseline for Niagara visual scaling. */
	UPROPERTY(EditDefaultsOnly, Category="Ice Floor|Visual", meta=(ClampMin="1.0"))
	float NiagaraReferenceRadius = 300.0f;

	/** Niagara user parameter value at NiagaraReferenceRadius. */
	UPROPERTY(EditDefaultsOnly, Category="Ice Floor|Visual", meta=(ClampMin="0.0"))
	float NiagaraScaleAtReferenceRadius = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category="Ice Floor|Visual")
	FName NiagaraScaleParameterName = TEXT("User.Scale_All");

	/** Niagara user parameter receiving the server-resolved floor duration in seconds. */
	UPROPERTY(EditDefaultsOnly, Category="Ice Floor|Visual")
	FName NiagaraDurationParameterName = TEXT("User.Duration_All");

	TWeakObjectPtr<AFrontierBaseCharacter> SourceCharacter;
	TSet<TWeakObjectPtr<AFrontierBaseCharacter>> SlowedCharacters;
	FFrontierDamageRequest DamageRequest;
	FName SlowSourceId;
	float DamageInterval = 1.0f;
	float SlowMultiplier = 0.6f;
	FTimerHandle SlowRefreshTimerHandle;
	FTimerHandle DamageTimerHandle;
};
