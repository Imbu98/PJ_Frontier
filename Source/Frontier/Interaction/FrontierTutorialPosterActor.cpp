#include "Interaction/FrontierTutorialPosterActor.h"

#include "Components/StaticMeshComponent.h"
#include "Frontier.h"
#include "FrontierPlayerController.h"
#include "UI/FrontierTutorialPosterWidget.h"

AFrontierTutorialPosterActor::AFrontierTutorialPosterActor()
{
	InteractionPromptText = FText::FromString(TEXT("Press F to read"));
	InteractionDisplayName = FText::FromString(TEXT("Tutorial Poster"));
	InteractionActionText = FText::FromString(TEXT("열기"));
}

void AFrontierTutorialPosterActor::Interacted(AFrontierPlayerController* InteractingController)
{
	if (!HasAuthority() || !CanInteract(InteractingController) || !InteractingController)
	{
		return;
	}

	FRONTIER_LOG(Log, TEXT("Tutorial poster interacted. Poster=%s Controller=%s"),
		*GetNameSafe(this),
		*GetNameSafe(InteractingController));

	InteractingController->ClientOpenTutorialPoster(this);
}

TSubclassOf<UFrontierTutorialPosterWidget> AFrontierTutorialPosterActor::GetTutorialPosterWidgetClass() const
{
	return TutorialPosterWidgetClass;
}

UMaterialInterface* AFrontierTutorialPosterActor::GetPosterMaterial() const
{
	if (!MeshComponent)
	{
		return nullptr;
	}

	return MeshComponent->GetMaterial(PosterMaterialIndex);
}
