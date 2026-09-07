#include "Interaction/FrontierInteractInterface.h"

#include "Components/PrimitiveComponent.h"

bool IFrontierInteractInterface::CanInteract(const AFrontierPlayerController* InteractingController) const
{
	return InteractingController != nullptr;
}

bool IFrontierInteractInterface::IsInstantInteraction(const AFrontierPlayerController* InteractingController) const
{
	return false;
}

float IFrontierInteractInterface::GetInteractionDuration(const AFrontierPlayerController* InteractingController) const
{
	return -1.0f;
}

void IFrontierInteractInterface::Interacted(AFrontierPlayerController* InteractingController)
{
}

FText IFrontierInteractInterface::GetInteractionDisplayName(const AFrontierPlayerController* InteractingController) const
{
	return FText::FromString(TEXT("Interact"));
}

FText IFrontierInteractInterface::GetInteractionActionText(const AFrontierPlayerController* InteractingController) const
{
	return NSLOCTEXT("FrontierInteraction", "DefaultAction", "상호작용");
}

FText IFrontierInteractInterface::GetInteractionPromptText(const AFrontierPlayerController* InteractingController) const
{
	return FText::FromString(TEXT("Press F to interact"));
}

FVector IFrontierInteractInterface::GetInteractionWorldLocation() const
{
	return FVector::ZeroVector;
}

void IFrontierInteractInterface::GetInteractionHighlightComponents(TArray<UPrimitiveComponent*>& OutComponents) const
{
	OutComponents.Reset();
}
