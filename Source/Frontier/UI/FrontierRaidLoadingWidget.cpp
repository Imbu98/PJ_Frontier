#include "UI/FrontierRaidLoadingWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UFrontierRaidLoadingWidget::SetStatusMessage(const FText& InStatusMessage)
{
	CachedStatusMessage = InStatusMessage;

	if (StatusText)
	{
		StatusText->SetText(CachedStatusMessage);
	}
}

void UFrontierRaidLoadingWidget::SetProgress(const float InProgress)
{
	CachedProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	if (ProgressBar)
	{
		ProgressBar->SetPercent(CachedProgress);
	}
}

void UFrontierRaidLoadingWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!WidgetTree || WidgetTree->RootWidget)
	{
		if (!ProgressBar && WidgetTree && WidgetTree->RootWidget)
		{
			if (UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget))
			{
				ProgressBar = WidgetTree->ConstructWidget<UProgressBar>(
					UProgressBar::StaticClass(),
					TEXT("ProgressBar"));
				if (ProgressBar)
				{
					if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(RootPanel->AddChild(ProgressBar)))
					{
						CanvasSlot->SetAnchors(FAnchors(0.2f, 0.88f, 0.8f, 0.94f));
						CanvasSlot->SetOffsets(FMargin(0.0f));
					}
				}
			}
		}
		SetProgress(CachedProgress);
		SetStatusMessage(CachedStatusMessage);
		return;
	}

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	if (StatusText)
	{
		StatusText->SetJustification(ETextJustify::Center);
		WidgetTree->RootWidget = StatusText;
	}

	SetProgress(CachedProgress);
	SetStatusMessage(CachedStatusMessage);
}
