#include "UI/FrontierInteractionPromptWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

void UFrontierInteractionPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildWidgetTreeIfNeeded();
	SetPromptText(CachedPromptText);
}

void UFrontierInteractionPromptWidget::SetPromptText(const FText& InPromptText)
{
	CachedPromptText = InPromptText;
	BuildWidgetTreeIfNeeded();

	if (PromptText)
	{
		PromptText->SetText(CachedPromptText);
	}
}

void UFrontierInteractionPromptWidget::BuildWidgetTreeIfNeeded()
{
	if (!WidgetTree)
	{
		return;
	}

	if (WidgetTree->RootWidget)
	{
		if (!PromptText)
		{
			PromptText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("PromptText")));
		}
		return;
	}

	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootBox"));
	WidgetTree->RootWidget = RootBox;

	PromptText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PromptText"));
	RootBox->AddChildToVerticalBox(PromptText);
}
