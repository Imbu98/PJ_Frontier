#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/FrontierInteractInterface.h"
#include "FrontierInteractableActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class FRONTIER_API AFrontierInteractableActor : public AActor, public IFrontierInteractInterface
{
	GENERATED_BODY()

public:
	AFrontierInteractableActor();

	virtual bool CanInteract(const AFrontierPlayerController* InteractingController) const override;
	virtual float GetInteractionDuration(const AFrontierPlayerController* InteractingController) const override;
	virtual FText GetInteractionDisplayName(const AFrontierPlayerController* InteractingController) const override;
	virtual FText GetInteractionActionText(const AFrontierPlayerController* InteractingController) const override;
	virtual FText GetInteractionPromptText(const AFrontierPlayerController* InteractingController) const override;
	virtual FVector GetInteractionWorldLocation() const override;
	virtual void GetInteractionHighlightComponents(TArray<UPrimitiveComponent*>& OutComponents) const override;

	UFUNCTION(BlueprintPure, Category="Frontier|Interaction")
	UStaticMeshComponent* GetMeshComponent() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Interaction")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Interaction")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Interaction", meta=(ClampMin="0.0"))
	float InteractionRange = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Interaction")
	FText InteractionPromptText = FText::FromString(TEXT("Press F to interact"));

	/** Short object name shown in the centered player-status interaction prompt. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Interaction")
	FText InteractionDisplayName = FText::FromString(TEXT("Interactable"));

	/** Action appended after the display name in the centered prompt. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Interaction")
	FText InteractionActionText = FText::FromString(TEXT("상호작용"));

	/** Set to 0 to use the player controller's default duration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Interaction", meta=(ClampMin="0.0"))
	float InteractionDuration = 0.0f;
};
