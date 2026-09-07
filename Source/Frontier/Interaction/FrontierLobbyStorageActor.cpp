#include "Interaction/FrontierLobbyStorageActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "FrontierPlayerController.h"

AFrontierLobbyStorageActor::AFrontierLobbyStorageActor()
{
	InteractionDisplayName = FText::FromString(TEXT("Storage"));
}

bool AFrontierLobbyStorageActor::CanInteract(const AFrontierPlayerController* InteractingController) const
{
	if (!InteractingController)
	{
		return false;
	}

	const APawn* InteractingPawn = InteractingController->GetPawn();
	return InteractingPawn && FVector::DistSquared(InteractingPawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(InteractionRange);
}

void AFrontierLobbyStorageActor::Interacted(AFrontierPlayerController* InteractingController)
{
	if (!HasAuthority() || !CanInteract(InteractingController) || !InteractingController)
	{
		return;
	}

	InteractingController->ClientOpenLobbyStorageUI();
}

FText AFrontierLobbyStorageActor::GetInteractionDisplayName(const AFrontierPlayerController* InteractingController) const
{
	return InteractionDisplayName;
}

FText AFrontierLobbyStorageActor::GetInteractionActionText(const AFrontierPlayerController* InteractingController) const
{
	return NSLOCTEXT("FrontierInteraction", "OpenStorageAction", "열기");
}

FText AFrontierLobbyStorageActor::GetInteractionPromptText(const AFrontierPlayerController* InteractingController) const
{
	return FText::FromString(TEXT("Press F to open storage"));
}

FVector AFrontierLobbyStorageActor::GetInteractionWorldLocation() const
{
	return MeshComponent ? MeshComponent->GetComponentLocation() : GetActorLocation();
}

void AFrontierLobbyStorageActor::GetInteractionHighlightComponents(TArray<UPrimitiveComponent*>& OutComponents) const
{
	OutComponents.Reset();
	if (MeshComponent)
	{
		OutComponents.Add(MeshComponent);
	}
}
