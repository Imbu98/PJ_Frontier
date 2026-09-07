#include "FrontierUpgradeResultWidget.h"

#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

namespace
{
FSlateFontInfo MakeResultFont(UTextBlock* TextBlock, const int32 Size, const FName Typeface = TEXT("Regular"))
{
	FSlateFontInfo Font = TextBlock ? TextBlock->GetFont() : FSlateFontInfo();
	Font.Size = Size;
	Font.TypefaceFontName = Typeface;
	return Font;
}

float SmoothStep01(const float Value)
{
	const float Clamped = FMath::Clamp(Value, 0.0f, 1.0f);
	return Clamped * Clamped * (3.0f - 2.0f * Clamped);
}
}

void UFrontierUpgradeResultWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bPresentationPlaying)
	{
		UpdatePresentationAnimation(InDeltaTime);
	}
}

void UFrontierUpgradeResultWidget::PresentResult(
	const bool bInUpgradeSucceeded,
	const int32 InPreviousEnhancementLevel,
	const int32 InCurrentEnhancementLevel,
	const FText& InItemName,
	UTexture2D* InItemIcon)
{
	bUpgradeSucceeded = bInUpgradeSucceeded;
	PreviousEnhancementLevel = FMath::Max(0, InPreviousEnhancementLevel);
	CurrentEnhancementLevel = FMath::Max(0, InCurrentEnhancementLevel);
	PresentedItemName = InItemName;
	PresentedItemIcon = InItemIcon;
	AnimationElapsed = 0.0f;
	bPresentationPlaying = true;

	ApplyResultStyle();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(0.0f);
	if (Border_ResultCard)
	{
		Border_ResultCard->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Border_ResultCard->SetRenderScale(FVector2D(0.78f));
		Border_ResultCard->SetRenderTranslation(FVector2D(0.0f, 28.0f));
	}
	BP_OnResultPresented(bUpgradeSucceeded);
}





void UFrontierUpgradeResultWidget::ApplyResultStyle()
{
	const FLinearColor Accent = bUpgradeSucceeded ? SuccessAccentColor : FailureAccentColor;
	if (Border_ResultCard)
	{
		Border_ResultCard->SetBrushColor(FLinearColor(Accent.R * 0.35f, Accent.G * 0.35f, Accent.B * 0.35f, 0.92f));
	}
	if (Border_ResultAccent)
	{
		Border_ResultAccent->SetBrushColor(FLinearColor(Accent.R, Accent.G, Accent.B, 0.23f));
	}
	if (Text_ResultSymbol)
	{
		Text_ResultSymbol->SetText(bUpgradeSucceeded ? FText::FromString(TEXT("+")) : FText::FromString(TEXT("!")));
		Text_ResultSymbol->SetColorAndOpacity(FSlateColor(Accent));
	}
	if (Text_ResultTitle)
	{
		Text_ResultTitle->SetText(bUpgradeSucceeded
			? NSLOCTEXT("FrontierUpgradeResult", "SuccessTitle", "강화 성공")
			: NSLOCTEXT("FrontierUpgradeResult", "FailureTitle", "강화 실패"));
		Text_ResultTitle->SetColorAndOpacity(FSlateColor(Accent));
	}
	if (Text_ItemName)
	{
		Text_ItemName->SetText(PresentedItemName);
	}
	if (Text_LevelTransition)
	{
		Text_LevelTransition->SetText(FText::Format(
			NSLOCTEXT("FrontierUpgradeResult", "LevelTransition", "+{0}  >  +{1}"),
			FText::AsNumber(PreviousEnhancementLevel),
			FText::AsNumber(CurrentEnhancementLevel)));
		Text_LevelTransition->SetColorAndOpacity(FSlateColor(Accent));
	}
	if (Text_ResultMessage)
	{
		Text_ResultMessage->SetText(bUpgradeSucceeded
			? NSLOCTEXT("FrontierUpgradeResult", "SuccessMessage", "장비의 강화 단계와 능력치가 상승했습니다.")
			: NSLOCTEXT("FrontierUpgradeResult", "FailureMessage", "강화에 실패했습니다. 사용한 재료와 재화는 소모되었습니다."));
	}
	if (Image_ItemIcon)
	{
		Image_ItemIcon->SetVisibility(PresentedItemIcon
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
		if (PresentedItemIcon)
		{
			Image_ItemIcon->SetBrushFromTexture(PresentedItemIcon, true);
		}
	}
}

void UFrontierUpgradeResultWidget::UpdatePresentationAnimation(const float DeltaTime)
{
	AnimationElapsed += FMath::Max(0.0f, DeltaTime);
	const float SafeIntroDuration = FMath::Max(IntroDuration, KINDA_SMALL_NUMBER);
	const float SafeOutroDuration = FMath::Max(OutroDuration, KINDA_SMALL_NUMBER);
	const float OutroStart = SafeIntroDuration + FMath::Max(0.0f, HoldDuration);
	const float TotalDuration = OutroStart + SafeOutroDuration;

	float Opacity = 1.0f;
	float Scale = 1.0f;
	FVector2D Translation = FVector2D::ZeroVector;

	if (AnimationElapsed < SafeIntroDuration)
	{
		const float Progress = SmoothStep01(AnimationElapsed / SafeIntroDuration);
		Opacity = Progress;
		if (Progress < 0.72f)
		{
			Scale = FMath::Lerp(0.78f, 1.055f, Progress / 0.72f);
		}
		else
		{
			Scale = FMath::Lerp(1.055f, 1.0f, (Progress - 0.72f) / 0.28f);
		}
		Translation.Y = FMath::Lerp(28.0f, 0.0f, Progress);
	}
	else if (AnimationElapsed < OutroStart)
	{
		const float HoldProgress = (AnimationElapsed - SafeIntroDuration) / FMath::Max(HoldDuration, KINDA_SMALL_NUMBER);
		if (bUpgradeSucceeded)
		{
			Scale = 1.0f + FMath::Sin(HoldProgress * 2.0f * PI) * 0.012f;
		}
		else if (HoldProgress < 0.38f)
		{
			const float ShakeFade = 1.0f - HoldProgress / 0.38f;
			Translation.X = FMath::Sin(HoldProgress * 10.0f * PI) * 11.0f * ShakeFade;
		}
	}
	else
	{
		const float Progress = SmoothStep01((AnimationElapsed - OutroStart) / SafeOutroDuration);
		Opacity = 1.0f - Progress;
		Scale = FMath::Lerp(1.0f, 0.96f, Progress);
		Translation.Y = FMath::Lerp(0.0f, -22.0f, Progress);
	}

	SetRenderOpacity(Opacity);
	if (Border_ResultCard)
	{
		Border_ResultCard->SetRenderScale(FVector2D(Scale));
		Border_ResultCard->SetRenderTranslation(Translation);
	}
	if (Border_ResultAccent)
	{
		const FLinearColor Accent = bUpgradeSucceeded ? SuccessAccentColor : FailureAccentColor;
		const float Pulse = bUpgradeSucceeded
			? 0.18f + 0.09f * (0.5f + 0.5f * FMath::Sin(AnimationElapsed * 9.0f))
			: 0.2f;
		Border_ResultAccent->SetBrushColor(FLinearColor(Accent.R, Accent.G, Accent.B, Pulse * Opacity));
	}

	if (AnimationElapsed >= TotalDuration)
	{
		bPresentationPlaying = false;
		RemoveFromParent();
	}
}
