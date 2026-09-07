#pragma once

#include "CoreMinimal.h"
#include "Interaction/FrontierInteractableActor.h"
#include "FrontierExtractionZoneActor.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UPrimitiveComponent;
class USceneComponent;
class AFrontierPlayerCharacter;
class AFrontierPlayerController;
struct FHitResult;

USTRUCT()
struct FFrontierExtractionCandidate
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AFrontierPlayerCharacter> PlayerCharacter = nullptr;
};

UCLASS()
class FRONTIER_API AFrontierExtractionZoneActor : public AFrontierInteractableActor
{
	GENERATED_BODY()

public:
	AFrontierExtractionZoneActor();
	virtual void Tick(float DeltaSeconds) override;
	virtual bool CanInteract(const AFrontierPlayerController* InteractingController) const override;
	virtual void Interacted(AFrontierPlayerController* InteractingController) override;
	virtual FText GetInteractionPromptText(const AFrontierPlayerController* InteractingController) const override;
	virtual FVector GetInteractionWorldLocation() const override;
	virtual void GetInteractionHighlightComponents(TArray<UPrimitiveComponent*>& OutComponents) const override;

	/** Extraction zones are not F-interactable; this only reports whether an actor is inside the trigger. */
	UFUNCTION(BlueprintPure, Category="Frontier|Extraction")
	bool IsActorInsideExtractionZone(const AActor* Actor) const;

	UFUNCTION(BlueprintPure, Category="Frontier|Extraction")
	float GetLocalProgressForCharacter(const AFrontierPlayerCharacter* PlayerCharacter) const;

	/** Called by the BP child to bind its Niagara component directly. */
	UFUNCTION(BlueprintCallable, Category="Frontier|Extraction|Visual")
	void SetExtractionNiagaraComponent(UNiagaraComponent* InNiagaraComponent);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void HandleExtractionBoxBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleExtractionBoxEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void AddOrRefreshCandidate(AFrontierPlayerCharacter* PlayerCharacter);
	void RemoveCandidate(AFrontierPlayerCharacter* PlayerCharacter);
	void EvaluateCandidates();
	float GetCurrentWorldTimeSeconds() const;
	void SetExtractionStartTime(float NewStartTime);
	void SetHasOccupants(bool bNewHasOccupants);
	void UpdateNiagaraEffect();

	UFUNCTION()
	void OnRep_HasOccupants();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Extraction")
	TObjectPtr<UBoxComponent> ExtractionBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Extraction", meta=(ClampMin="0.1"))
	float RequiredSafeDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Extraction", meta=(ClampMin="0.01"))
	float EvaluationInterval = 0.1f;

	UPROPERTY(Transient)
	TArray<FFrontierExtractionCandidate> ExtractionCandidates;

	/** Server-authoritative shared timer. A value below zero means the zone is idle. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Frontier|Extraction")
	float ExtractionStartTime = -1.0f;

	/** Replicated so every client shows the BP Niagara component for the same occupants. */
	UPROPERTY(ReplicatedUsing=OnRep_HasOccupants, VisibleInstanceOnly, BlueprintReadOnly, Category="Frontier|Extraction")
	bool bHasOccupants = false;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> NiagaraComponent = nullptr;

	/** Local presentation state used to restart Niagara from age zero for each extraction cycle. */
	bool bNiagaraEffectRunning = false;

	/** Runtime Niagara user-parameter name. NS_Spline7 expects User.Speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Extraction|Visual")
	FName NiagaraSpeedParameterName = TEXT("User.Speed");

	/** B value: value sent to the Niagara parameter when extraction starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Extraction|Visual", meta=(ClampMin="0.0", FormerlySerializedAs="NiagaraStartSpeed"))
	float NiagaraParameterStartValue = 0.25f;

	/** A value: value approached by the Niagara parameter as extraction completes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Extraction|Visual", meta=(ClampMin="0.0", FormerlySerializedAs="NiagaraMaxSpeed"))
	float NiagaraParameterEndValue = 4.0f;

	float LastEvaluationTime = -1000.0f;
};
