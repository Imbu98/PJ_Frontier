#include "UI/FrontierTutorialPosterWidget.h"

#include "Components/Image.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInterface.h"

UFrontierTutorialPosterWidget::UFrontierTutorialPosterWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

void UFrontierTutorialPosterWidget::SetPosterMaterial(UMaterialInterface* InPosterMaterial)
{
	if (!PosterImage || !InPosterMaterial)
	{
		return;
	}

	PosterImage->SetBrushFromMaterial(InPosterMaterial);
}

FReply UFrontierTutorialPosterWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		RemoveFromParent();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
