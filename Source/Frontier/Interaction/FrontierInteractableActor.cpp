#include "Interaction/FrontierInteractableActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "FrontierPlayerController.h"

AFrontierInteractableActor::AFrontierInteractableActor()
{
	bReplicates = true;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(SceneRoot);
	MeshComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

bool AFrontierInteractableActor::CanInteract(const AFrontierPlayerController* InteractingController) const
{
	if (!InteractingController)
	{
		return false;
	}

	const APawn* InteractingPawn = InteractingController->GetPawn();
	return InteractingPawn
		&& FVector::DistSquared(InteractingPawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(InteractionRange);
}

float AFrontierInteractableActor::GetInteractionDuration(const AFrontierPlayerController* InteractingController) const
{
	return InteractionDuration;
}

FText AFrontierInteractableActor::GetInteractionDisplayName(const AFrontierPlayerController* InteractingController) const
{
	return InteractionDisplayName;
}

FText AFrontierInteractableActor::GetInteractionActionText(const AFrontierPlayerController* InteractingController) const
{
	return InteractionActionText;
}

FText AFrontierInteractableActor::GetInteractionPromptText(const AFrontierPlayerController* InteractingController) const
{
	return InteractionPromptText;
}

FVector AFrontierInteractableActor::GetInteractionWorldLocation() const
{
	return MeshComponent ? MeshComponent->GetComponentLocation() : GetActorLocation();
}

void AFrontierInteractableActor::GetInteractionHighlightComponents(TArray<UPrimitiveComponent*>& OutComponents) const
{
	OutComponents.Reset();
	if (MeshComponent)
	{
		OutComponents.Add(MeshComponent);
	}
}

UStaticMeshComponent* AFrontierInteractableActor::GetMeshComponent() const
{
	return MeshComponent;
}
