#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierTutorialPosterWidget.generated.h"

class UImage;
class UMaterialInterface;

UCLASS()
class FRONTIER_API UFrontierTutorialPosterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFrontierTutorialPosterWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category="Frontier|Tutorial")
	void SetPosterMaterial(UMaterialInterface* InPosterMaterial);

protected:
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> PosterImage;
};
