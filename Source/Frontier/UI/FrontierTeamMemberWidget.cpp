#include "UI/FrontierTeamMemberWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Frontier.h"
#include "Game/FrontierPlayerState.h"

void UFrontierTeamMemberWidget::NativeConstruct()
{
	
	Super::NativeConstruct();
	BuildWidgetTreeIfNeeded();
	RefreshFromPlayerState();
}

void UFrontierTeamMemberWidget::NativeDestruct()
{
	
	UnbindObservedPlayerState();
	Super::NativeDestruct();
}

void UFrontierTeamMemberWidget::SetObservedPlayerState(AFrontierPlayerState* PlayerState)
{
	if (ObservedPlayerState == PlayerState)
	{
		RefreshFromPlayerState();
		return;
	}

	UnbindObservedPlayerState();
	ObservedPlayerState = PlayerState;

	if (ObservedPlayerState)
	{
		ObservedPlayerState->OnVitalsChanged.AddUObject(this, &UFrontierTeamMemberWidget::HandleObservedVitalsChanged);
		ObservedPlayerState->OnTeamIdChanged.AddUObject(this, &UFrontierTeamMemberWidget::HandleObservedTeamChanged);
		ObservedPlayerState->OnPlayerNameChanged.AddUObject(this, &UFrontierTeamMemberWidget::HandleObservedPlayerNameChanged);
	}

	RefreshFromPlayerState();
}

AFrontierPlayerState* UFrontierTeamMemberWidget::GetObservedPlayerState() const
{
	return ObservedPlayerState;
}

void UFrontierTeamMemberWidget::RefreshFromPlayerState()
{
	BuildWidgetTreeIfNeeded();

	if (!ObservedPlayerState)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::Visible);

	const float Health = ObservedPlayerState->GetDisplayHealth();
	const float MaxHealth = FMath::Max(ObservedPlayerState->GetDisplayMaxHealth(), 1.0f);
	const float Stamina = ObservedPlayerState->GetDisplayStamina();
	const float MaxStamina = FMath::Max(ObservedPlayerState->GetDisplayMaxStamina(), 1.0f);

	if (NameText)
	{
		NameText->SetText(FText::FromString(ObservedPlayerState->GetPlayerName()));
	}

	if (HealthBar)
	{
		HealthBar->SetPercent(FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f));
	}

	if (HealthText)
	{
		const FString HealthString = ObservedPlayerState->IsDisplayDead()
			? TEXT("DEAD")
			: FString::Printf(TEXT("HP %.0f / %.0f"), Health, MaxHealth);
		HealthText->SetText(FText::FromString(HealthString));
	}

	if (StaminaBar)
	{
		StaminaBar->SetPercent(FMath::Clamp(Stamina / MaxStamina, 0.0f, 1.0f));
	}

	if (StaminaText)
	{
		StaminaText->SetText(FText::FromString(FString::Printf(TEXT("SP %.0f / %.0f"), Stamina, MaxStamina)));
	}

}

void UFrontierTeamMemberWidget::BuildWidgetTreeIfNeeded()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UVerticalBox* RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootBox"));
	WidgetTree->RootWidget = RootBox;

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	RootBox->AddChildToVerticalBox(NameText);

	HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
	RootBox->AddChildToVerticalBox(HealthText);

	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	RootBox->AddChildToVerticalBox(HealthBar);

	StaminaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StaminaText"));
	RootBox->AddChildToVerticalBox(StaminaText);

	StaminaBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("StaminaBar"));
	RootBox->AddChildToVerticalBox(StaminaBar);
}

void UFrontierTeamMemberWidget::UnbindObservedPlayerState()
{
	if (ObservedPlayerState)
	{
		ObservedPlayerState->OnVitalsChanged.RemoveAll(this);
		ObservedPlayerState->OnTeamIdChanged.RemoveAll(this);
		ObservedPlayerState->OnPlayerNameChanged.RemoveAll(this);
	}

	ObservedPlayerState = nullptr;
}

void UFrontierTeamMemberWidget::HandleObservedVitalsChanged(AFrontierPlayerState* PlayerState)
{
	RefreshFromPlayerState();
}

void UFrontierTeamMemberWidget::HandleObservedTeamChanged(AFrontierPlayerState* PlayerState, int32 TeamId)
{
	RefreshFromPlayerState();
}

void UFrontierTeamMemberWidget::HandleObservedPlayerNameChanged(AFrontierPlayerState* PlayerState)
{
	RefreshFromPlayerState();
}
