#include "UI/FrontierLoadingScreenSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Frontier.h"
#include "UI/FrontierRaidLoadingWidget.h"
#include "UI/FrontierUISettings.h"

void UFrontierLoadingScreenSubsystem::Deinitialize()
{
	StopRaidLoadingScreen();
	Super::Deinitialize();
}

void UFrontierLoadingScreenSubsystem::StartRaidLoadingScreen()
{
	if (bRaidLoadingScreenActive && IsValid(LoadingScreenUserWidget)
		&& LoadingScreenUserWidget->IsInViewport())
	{
		return;
	}

	bRaidLoadingScreenActive = true;
	CreateRaidLoadingWidget();
}

void UFrontierLoadingScreenSubsystem::RecreateRaidLoadingScreen()
{
	if (!bRaidLoadingScreenActive)
	{
		StartRaidLoadingScreen();
		return;
	}

	if (LoadingScreenUserWidget)
	{
		LoadingScreenUserWidget->RemoveFromParent();
	}
	LoadingScreenUserWidget = nullptr;
	CreateRaidLoadingWidget();
}

void UFrontierLoadingScreenSubsystem::CreateRaidLoadingWidget()
{
	if (!GetGameInstance())
	{
		bRaidLoadingScreenActive = false;
		return;
	}

	UClass* LoadingWidgetClass = nullptr;
	if (const UFrontierUISettings* Settings = GetDefault<UFrontierUISettings>())
	{
		TArray<TSoftClassPtr<UFrontierRaidLoadingWidget>> ConfiguredClasses;
		for (const TSoftClassPtr<UFrontierRaidLoadingWidget>& Candidate : Settings->RaidLoadingWidgetClasses)
		{
			if (!Candidate.IsNull())
			{
				ConfiguredClasses.Add(Candidate);
			}
		}

		if (!ConfiguredClasses.IsEmpty())
		{
			const TSoftClassPtr<UFrontierRaidLoadingWidget>& SelectedClass =
				ConfiguredClasses[FMath::RandHelper(ConfiguredClasses.Num())];
			LoadingWidgetClass = SelectedClass.LoadSynchronous();
		}
	}

	if (!LoadingWidgetClass || !LoadingWidgetClass->IsChildOf(UFrontierRaidLoadingWidget::StaticClass()))
	{
		bRaidLoadingScreenActive = false;
		FRONTIER_LOG(Warning, TEXT("[LoadingScreen] No configured raid loading WBP is available."));
		return;
	}

	LoadingScreenUserWidget = CreateWidget<UUserWidget>(GetGameInstance(), LoadingWidgetClass);
	if (!LoadingScreenUserWidget)
	{
		bRaidLoadingScreenActive = false;
		FRONTIER_LOG(Warning, TEXT("[LoadingScreen] Loading widget creation failed. Class=%s"),
			*GetNameSafe(LoadingWidgetClass));
		return;
	}

	if (UFrontierRaidLoadingWidget* RaidLoadingWidget = Cast<UFrontierRaidLoadingWidget>(LoadingScreenUserWidget))
	{
		RaidLoadingWidget->SetProgress(RaidLoadingProgress);
		RaidLoadingWidget->SetStatusMessage(RaidLoadingStatusMessage);
	}

	LoadingScreenUserWidget->AddToViewport(10000);
}

void UFrontierLoadingScreenSubsystem::StopRaidLoadingScreen()
{
	if (LoadingScreenUserWidget)
	{
		LoadingScreenUserWidget->RemoveFromParent();
	}
	LoadingScreenUserWidget = nullptr;
	bRaidLoadingScreenActive = false;
	RaidLoadingProgress = 0.0f;
	RaidLoadingStatusMessage = FText::FromString(TEXT("Loading..."));
}

void UFrontierLoadingScreenSubsystem::SetRaidLoadingProgress(
	const float InProgress,
	const FText& InStatusMessage)
{
	RaidLoadingProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	RaidLoadingStatusMessage = InStatusMessage;

	if (UFrontierRaidLoadingWidget* RaidLoadingWidget = Cast<UFrontierRaidLoadingWidget>(LoadingScreenUserWidget))
	{
		RaidLoadingWidget->SetProgress(RaidLoadingProgress);
		RaidLoadingWidget->SetStatusMessage(RaidLoadingStatusMessage);
	}
}
