#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FrontierInteractInterface.generated.h"

class AFrontierPlayerController;
class UPrimitiveComponent;

UINTERFACE(MinimalAPI, BlueprintType)
class UFrontierInteractInterface : public UInterface
{
	GENERATED_BODY()
};

class FRONTIER_API IFrontierInteractInterface
{
	GENERATED_BODY()

public:
	virtual bool CanInteract(const AFrontierPlayerController* InteractingController) const;
	/** Instant interactions execute authoritatively without opening the timed interaction channel. */
	virtual bool IsInstantInteraction(const AFrontierPlayerController* InteractingController) const;
	/** Return <= 0 to use the player controller's default interaction duration. */
	virtual float GetInteractionDuration(const AFrontierPlayerController* InteractingController) const;
	virtual void Interacted(AFrontierPlayerController* InteractingController);
	/** Short target name displayed next to the interaction key in the player HUD. */
	virtual FText GetInteractionDisplayName(const AFrontierPlayerController* InteractingController) const;
	/** Action appended to the centered prompt, such as acquire, search, or open. */
	virtual FText GetInteractionActionText(const AFrontierPlayerController* InteractingController) const;
	virtual FText GetInteractionPromptText(const AFrontierPlayerController* InteractingController) const;
	virtual FVector GetInteractionWorldLocation() const;
	virtual void GetInteractionHighlightComponents(TArray<UPrimitiveComponent*>& OutComponents) const;
};
