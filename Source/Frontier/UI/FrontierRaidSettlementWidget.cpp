#include "UI/FrontierRaidSettlementWidget.h"

#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Frontier.h"
#include "TimerManager.h"
#include "UI/FrontierItemTooltipWidget.h"

void UFrontierRaidSettlementWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_ReturnToLobby)
	{
		Button_ReturnToLobby->OnClicked.RemoveAll(this);
		Button_ReturnToLobby->OnClicked.AddDynamic(
			this,
			&UFrontierRaidSettlementWidget::HandleReturnToLobbyClicked);
	}
}

void UFrontierRaidSettlementWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AnimationTimerHandle);
	}
	if (Button_ReturnToLobby)
	{
		Button_ReturnToLobby->OnClicked.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UFrontierRaidSettlementWidget::HandleReturnToLobbyClicked()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AnimationTimerHandle);
	}
	OnReturnToLobbyRequested.Broadcast();
}

void UFrontierRaidSettlementWidget::PlaySettlement(
	const FFrontierRaidSettlementPresentation& Presentation)
{
	if (!Presentation.bValid || Presentation.BeforeLevel.Level <= 0 || Presentation.AfterLevel.Level <= 0)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	ActivePresentation = Presentation;
	DisplayedLevel = Presentation.BeforeLevel.Level;
	SetVisibility(ESlateVisibility::Visible);
	RefreshLevelLabels();

	if (Text_AwardedExperience)
	{
		Text_AwardedExperience->SetText(FText::FromString(FString::Printf(
			TEXT("+%lld XP"),
			Presentation.AwardedExperience)));
	}
	if (Text_EscapeResult)
	{
		Text_EscapeResult->SetText(ResolveEscapeResultText(Presentation.Outcome));
	}

	const float StartPercent = Presentation.BeforeLevel.GetProgress();
	const float TargetPercent = DisplayedLevel < Presentation.AfterLevel.Level
		? 1.0f
		: Presentation.AfterLevel.GetProgress();
	StartSegment(StartPercent, TargetPercent);
}

FText UFrontierRaidSettlementWidget::ResolveEscapeResultText(
	const EFrontierRaidOutcome Outcome) const
{
	const bool bExtracted = Outcome == EFrontierRaidOutcome::Extracted;
	const FName RowName = bExtracted ? TEXT("Success") : TEXT("Fail");
	const FText FallbackText = bExtracted
		? NSLOCTEXT("FrontierRaidSettlement", "ExtractionSuccess", "탈출 성공")
		: NSLOCTEXT("FrontierRaidSettlement", "ExtractionFail", "탈출 실패");

	if (!EscapeResultTextDataTable)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Escape result text DataTable is not configured. Row=%s"),
			*RowName.ToString());
		return FallbackText;
	}

	const FFrontierItemStatDisplayNameRow* Row =
		EscapeResultTextDataTable->FindRow<FFrontierItemStatDisplayNameRow>(
			RowName,
			TEXT("RaidSettlementEscapeResult"),
			false);
	if (!Row)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Escape result text row was not found. Table=%s Row=%s"),
			*GetNameSafe(EscapeResultTextDataTable),
			*RowName.ToString());
		return FallbackText;
	}

	return Row->DisplayText.IsEmpty() ? FallbackText : Row->DisplayText;
}

void UFrontierRaidSettlementWidget::StartSegment(
	const float StartPercent,
	const float TargetPercent)
{
	SegmentStartPercent = FMath::Clamp(StartPercent, 0.0f, 1.0f);
	SegmentTargetPercent = FMath::Clamp(TargetPercent, 0.0f, 1.0f);
	SegmentStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	if (ProgressBar_Experience)
	{
		ProgressBar_Experience->SetPercent(SegmentStartPercent);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AnimationTimerHandle);
		World->GetTimerManager().SetTimer(
			AnimationTimerHandle,
			this,
			&UFrontierRaidSettlementWidget::UpdateAnimation,
			0.02f,
			true);
	}
}

void UFrontierRaidSettlementWidget::UpdateAnimation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Alpha = FMath::Clamp(
		static_cast<float>((World->GetTimeSeconds() - SegmentStartTime) / FMath::Max(0.1f, FillDurationPerLevel)),
		0.0f,
		1.0f);
	if (ProgressBar_Experience)
	{
		ProgressBar_Experience->SetPercent(FMath::Lerp(SegmentStartPercent, SegmentTargetPercent, Alpha));
	}
	if (Alpha < 1.0f)
	{
		return;
	}

	if (DisplayedLevel < ActivePresentation.AfterLevel.Level)
	{
		++DisplayedLevel;
		RefreshLevelLabels();
		const float NextTarget = DisplayedLevel < ActivePresentation.AfterLevel.Level
			? 1.0f
			: ActivePresentation.AfterLevel.GetProgress();
		StartSegment(0.0f, NextTarget);
		return;
	}

	World->GetTimerManager().ClearTimer(AnimationTimerHandle);
}

void UFrontierRaidSettlementWidget::RefreshLevelLabels()
{
	if (Text_CurrentLevel)
	{
		Text_CurrentLevel->SetText(FText::AsNumber(DisplayedLevel));
	}
	if (Text_NextLevel)
	{
		const bool bAtMaxLevel = ActivePresentation.AfterLevel.MaxLevel > 0
			&& DisplayedLevel >= ActivePresentation.AfterLevel.MaxLevel;
		Text_NextLevel->SetText(bAtMaxLevel
			? FText::FromString(TEXT("MAX"))
			: FText::AsNumber(DisplayedLevel + 1));
	}
}
