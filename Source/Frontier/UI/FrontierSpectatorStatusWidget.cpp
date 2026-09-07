#include "UI/FrontierSpectatorStatusWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "Extraction/FrontierExtractionZoneActor.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierGameState.h"
#include "TimerManager.h"

void UFrontierSpectatorStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildWidgetTreeIfNeeded();

	if (PrevSpectatorButton)
	{
		PrevSpectatorButton->OnClicked.AddDynamic(this, &UFrontierSpectatorStatusWidget::HandlePrevSpectatorClicked);
	}

	if (NextSpectatorButton)
	{
		NextSpectatorButton->OnClicked.AddDynamic(this, &UFrontierSpectatorStatusWidget::HandleNextSpectatorClicked);
	}
	if (EndSpectatorButton)
	{
		EndSpectatorButton->OnClicked.AddDynamic(this, &UFrontierSpectatorStatusWidget::HandleEndSpectatingClicked);
	}

	RefreshSpectatorState();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			this,
			&UFrontierSpectatorStatusWidget::HandleRefreshTick,
			0.25f,
			true);
	}
}

void UFrontierSpectatorStatusWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	if (PrevSpectatorButton)
	{
		PrevSpectatorButton->OnClicked.RemoveAll(this);
	}

	if (NextSpectatorButton)
	{
		NextSpectatorButton->OnClicked.RemoveAll(this);
	}
	if (EndSpectatorButton)
	{
		EndSpectatorButton->OnClicked.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UFrontierSpectatorStatusWidget::RefreshSpectatorState()
{
	RefreshRaidTimerText();
	RefreshTargetText();
	RefreshExtractionProgress();
}

UWidget* UFrontierSpectatorStatusWidget::GetPreferredFocusTarget() const
{
	if (EndSpectatorButton && EndSpectatorButton->GetIsEnabled())
	{
		return EndSpectatorButton;
	}

	if (PrevSpectatorButton && PrevSpectatorButton->GetIsEnabled())
	{
		return PrevSpectatorButton;
	}

	return NextSpectatorButton && NextSpectatorButton->GetIsEnabled()
		? static_cast<UWidget*>(NextSpectatorButton.Get())
		: nullptr;
}

void UFrontierSpectatorStatusWidget::BuildWidgetTreeIfNeeded()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UTextBlock* RootText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpectatorTargetText"));
	RootText->SetText(FText::FromString(TEXT("Spectating : none")));
	WidgetTree->RootWidget = RootText;
	SpectatorTargetText = RootText;
}

void UFrontierSpectatorStatusWidget::RefreshRaidTimerText()
{
	if (!RaidTimerText)
	{
		return;
	}

	const AFrontierGameState* FrontierGameState = GetWorld() ? GetWorld()->GetGameState<AFrontierGameState>() : nullptr;
	if (!FrontierGameState || !FrontierGameState->IsRaidTimerActive())
	{
		RaidTimerText->SetText(FText::GetEmpty());
		RaidTimerText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const int32 RemainingTimeSeconds = FMath::Max(0, FrontierGameState->GetRaidRemainingTimeSeconds());
	const int32 Minutes = RemainingTimeSeconds / 60;
	const int32 Seconds = RemainingTimeSeconds % 60;

	RaidTimerText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RaidTimerText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds)));
	RaidTimerText->SetColorAndOpacity(FSlateColor(RemainingTimeSeconds <= 60
		? FLinearColor(1.0f, 0.2f, 0.2f, 1.0f)
		: FLinearColor::White));
}

void UFrontierSpectatorStatusWidget::RefreshTargetText()
{
	AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>();
	const bool bCanCycle = FrontierPlayerController && FrontierPlayerController->CanCycleSpectatorTargets();

	if (PrevSpectatorButton)
	{
		PrevSpectatorButton->SetIsEnabled(bCanCycle);
	}

	if (NextSpectatorButton)
	{
		NextSpectatorButton->SetIsEnabled(bCanCycle);
	}

	if (SpectatorTargetText)
	{
		SpectatorTargetText->SetText(FrontierPlayerController
			? FrontierPlayerController->GetCurrentSpectatorTargetDisplayText()
			: FText::FromString(TEXT("Spectating : none")));
	}
}

void UFrontierSpectatorStatusWidget::RefreshExtractionProgress()
{
	float ExtractionProgress = 0.0f;
	const AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>();
	const AFrontierPlayerCharacter* SpectatedCharacter = FrontierPlayerController
		? Cast<AFrontierPlayerCharacter>(FrontierPlayerController->GetViewTarget())
		: nullptr;

	if (SpectatedCharacter && GetWorld())
	{
		for (TActorIterator<AFrontierExtractionZoneActor> Iterator(GetWorld()); Iterator; ++Iterator)
		{
			ExtractionProgress = FMath::Max(ExtractionProgress, Iterator->GetLocalProgressForCharacter(SpectatedCharacter));
		}
	}

	SetExtractionProgress(ExtractionProgress);
}

void UFrontierSpectatorStatusWidget::SetExtractionProgress(const float InProgress)
{
	const float ClampedProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	const bool bShowProgress = ClampedProgress > 0.0f && ClampedProgress < 1.0f;

	if (ExtractionProgressBar)
	{
		ExtractionProgressBar->SetPercent(ClampedProgress);
	}

	if (ExtractionProgressText)
	{
		ExtractionProgressText->SetText(FText::FromString(FString::Printf(
			TEXT("Extracting %d%%"),
			FMath::RoundToInt(ClampedProgress * 100.0f))));
	}

	if (ExtractionOverlay)
	{
		ExtractionOverlay->SetVisibility(bShowProgress
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

void UFrontierSpectatorStatusWidget::HandleRefreshTick()
{
	RefreshSpectatorState();
}

void UFrontierSpectatorStatusWidget::HandlePrevSpectatorClicked()
{
	if (AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>())
	{
		FrontierPlayerController->CycleSpectatorTarget(-1);
	}

	RefreshTargetText();
}

void UFrontierSpectatorStatusWidget::HandleNextSpectatorClicked()
{
	if (AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>())
	{
		FrontierPlayerController->CycleSpectatorTarget(1);
	}

	RefreshTargetText();
}

void UFrontierSpectatorStatusWidget::HandleEndSpectatingClicked()
{
	if (EndSpectatorButton)
	{
		EndSpectatorButton->SetIsEnabled(false);
	}

	if (AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>())
	{
		FrontierPlayerController->RequestEndSpectating();
	}
}
