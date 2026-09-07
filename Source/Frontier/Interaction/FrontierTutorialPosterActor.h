#pragma once

#include "CoreMinimal.h"
#include "Interaction/FrontierInteractableActor.h"
#include "FrontierTutorialPosterActor.generated.h"

class UFrontierTutorialPosterWidget;
class UMaterialInterface;

UCLASS(Blueprintable)
class FRONTIER_API AFrontierTutorialPosterActor : public AFrontierInteractableActor
{
	GENERATED_BODY()

public:
	AFrontierTutorialPosterActor();

	virtual void Interacted(AFrontierPlayerController* InteractingController) override;

	UFUNCTION(BlueprintPure, Category="Frontier|Tutorial")
	TSubclassOf<UFrontierTutorialPosterWidget> GetTutorialPosterWidgetClass() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Tutorial")
	UMaterialInterface* GetPosterMaterial() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Tutorial")
	TSubclassOf<UFrontierTutorialPosterWidget> TutorialPosterWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Tutorial", meta=(ClampMin="0"))
	int32 PosterMaterialIndex = 0;
};
