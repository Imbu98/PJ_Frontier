#include "UI/FrontierOverheadPlayerNameWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Game/FrontierPlayerState.h"

void UFrontierOverheadPlayerNameWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!WidgetTree->RootWidget)
	{
		UVerticalBox* RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootBox"));
		NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
		NameText->SetJustification(ETextJustify::Center);
		RootBox->AddChildToVerticalBox(NameText);

		HPBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HPBar"));
		HPBar->SetPercent(1.0f);
		HPBar->SetFillColorAndOpacity(FLinearColor(0.8f, 0.03f, 0.03f, 1.0f));
		RootBox->AddChildToVerticalBox(HPBar);
		WidgetTree->RootWidget = RootBox;
	}

	RefreshFromPlayerState();
	SetEnemyPresentation(bEnemyPresentation);
}

void UFrontierOverheadPlayerNameWidget::SetEnemyPresentation(const bool bInEnemyPresentation)
{
	bEnemyPresentation = bInEnemyPresentation;
	if (NameText)
	{
		NameText->SetColorAndOpacity(bEnemyPresentation
			? FSlateColor(FLinearColor(0.9f, 0.02f, 0.02f, 1.0f))
			: FSlateColor(FLinearColor::White));
	}
	if (HPBar)
	{
		HPBar->SetVisibility(bEnemyPresentation ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UFrontierOverheadPlayerNameWidget::SetHealthPercent(const float HealthPercent)
{
	if (HPBar)
	{
		HPBar->SetPercent(FMath::Clamp(HealthPercent, 0.0f, 1.0f));
	}
}

void UFrontierOverheadPlayerNameWidget::NativeDestruct()
{
	if (ObservedPlayerState)
	{
		ObservedPlayerState->OnPlayerNameChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UFrontierOverheadPlayerNameWidget::SetObservedPlayerState(AFrontierPlayerState* InPlayerState)
{
	if (ObservedPlayerState == InPlayerState)
	{
		RefreshFromPlayerState();
		return;
	}

	if (ObservedPlayerState)
	{
		ObservedPlayerState->OnPlayerNameChanged.RemoveAll(this);
	}

	ObservedPlayerState = InPlayerState;
	if (ObservedPlayerState)
	{
		ObservedPlayerState->OnPlayerNameChanged.AddUObject(this, &UFrontierOverheadPlayerNameWidget::HandlePlayerNameChanged);
	}

	RefreshFromPlayerState();
}

void UFrontierOverheadPlayerNameWidget::RefreshFromPlayerState()
{
	if (NameText)
	{
		NameText->SetText(ObservedPlayerState ? FText::FromString(ObservedPlayerState->GetPlayerName()) : FText::GetEmpty());
	}
}

void UFrontierOverheadPlayerNameWidget::HandlePlayerNameChanged(AFrontierPlayerState* PlayerState)
{
	RefreshFromPlayerState();
}
