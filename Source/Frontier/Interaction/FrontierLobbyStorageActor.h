#pragma once

#include "CoreMinimal.h"
#include "Interaction/FrontierInteractableActor.h"
#include "FrontierLobbyStorageActor.generated.h"

class AFrontierPlayerController;
class UPrimitiveComponent;
class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class FRONTIER_API AFrontierLobbyStorageActor : public AFrontierInteractableActor
{
	GENERATED_BODY()

public:
	AFrontierLobbyStorageActor();

	virtual bool CanInteract(const AFrontierPlayerController* InteractingController) const override;
	virtual void Interacted(AFrontierPlayerController* InteractingController) override;
	virtual FText GetInteractionDisplayName(const AFrontierPlayerController* InteractingController) const override;
	virtual FText GetInteractionActionText(const AFrontierPlayerController* InteractingController) const override;
	virtual FText GetInteractionPromptText(const AFrontierPlayerController* InteractingController) const override;
	virtual FVector GetInteractionWorldLocation() const override;
	virtual void GetInteractionHighlightComponents(TArray<UPrimitiveComponent*>& OutComponents) const override;

};
