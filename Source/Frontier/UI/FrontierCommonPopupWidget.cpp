#include "UI/FrontierCommonPopupWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "InputCoreTypes.h"

void UFrontierCommonPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CaptureAuthoredVisualStyle();
	SetIsFocusable(true);
	if (PopupConfirmButton)
	{
		PopupConfirmButton->OnClicked.AddUniqueDynamic(this, &UFrontierCommonPopupWidget::HandleConfirmClicked);
	}
	if (PopupCancelButton)
	{
		PopupCancelButton->OnClicked.AddUniqueDynamic(this, &UFrontierCommonPopupWidget::HandleCancelClicked);
	}
}

void UFrontierCommonPopupWidget::NativeDestruct()
{
	if (PopupConfirmButton)
	{
		PopupConfirmButton->OnClicked.RemoveDynamic(this, &UFrontierCommonPopupWidget::HandleConfirmClicked);
	}
	if (PopupCancelButton)
	{
		PopupCancelButton->OnClicked.RemoveDynamic(this, &UFrontierCommonPopupWidget::HandleCancelClicked);
	}
	Super::NativeDestruct();
}

FReply UFrontierCommonPopupWidget::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Enter || Key == EKeys::Virtual_Gamepad_Accept.GetVirtualKey())
	{
		if (bCurrentShowsConfirmButton)
		{
			HandleConfirmClicked();
		}
		else if (bCurrentShowsCancelButton)
		{
			HandleCancelClicked();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Escape && bCurrentAllowsEscape)
	{
		if (bCurrentShowsCancelButton)
		{
			HandleCancelClicked();
		}
		else if (bCurrentShowsConfirmButton)
		{
			HandleConfirmClicked();
		}
		else
		{
			OnPopupAction.Broadcast(EFrontierPopupResult::Dismissed);
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UFrontierCommonPopupWidget::PresentRequest(const FFrontierPopupRequest& Request)
{
	CurrentPopupType = Request.Type;
	bCurrentShowsConfirmButton = Request.bShowConfirmButton;
	bCurrentShowsCancelButton = Request.bShowCancelButton;
	bCurrentAllowsEscape = Request.bAllowEscape;

	if (PopupTitleText)
	{
		PopupTitleText->SetText(Request.Title.IsEmpty() ? GetDefaultTitle(Request.Type) : Request.Title);
	}
	if (PopupMessageText)
	{
		PopupMessageText->SetText(Request.Message);
	}
	if (PopupConfirmButtonText)
	{
		PopupConfirmButtonText->SetText(Request.ConfirmButtonText.IsEmpty()
			? GetDefaultConfirmText(Request.Type)
			: Request.ConfirmButtonText);
	}
	if (PopupCancelButtonText)
	{
		PopupCancelButtonText->SetText(Request.CancelButtonText.IsEmpty()
			? (Request.bShowConfirmButton
				? NSLOCTEXT("FrontierCommonPopup", "DefaultCancel", "아니오")
				: NSLOCTEXT("FrontierCommonPopup", "DefaultClose", "닫기"))
			: Request.CancelButtonText);
	}
	UWidget* ConfirmVisibilityTarget = PopupConfirmButtonContainer
		? PopupConfirmButtonContainer.Get()
		: PopupConfirmButton.Get();
	if (ConfirmVisibilityTarget)
	{
		ConfirmVisibilityTarget->SetVisibility(Request.bShowConfirmButton
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
	UWidget* CancelVisibilityTarget = PopupCancelButtonContainer
		? PopupCancelButtonContainer.Get()
		: PopupCancelButton.Get();
	if (CancelVisibilityTarget)
	{
		CancelVisibilityTarget->SetVisibility(Request.bShowCancelButton
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
	if (PopupButtonGap)
	{
		PopupButtonGap->SetVisibility(Request.bShowConfirmButton && Request.bShowCancelButton
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	SetVisibility(ESlateVisibility::Visible);
	ApplyVisualStyle(Request.Type);
	BP_OnPopupPresented(Request);
	if (PopupConfirmButton && Request.bShowConfirmButton)
	{
		PopupConfirmButton->SetKeyboardFocus();
	}
	else if (PopupCancelButton && Request.bShowCancelButton)
	{
		PopupCancelButton->SetKeyboardFocus();
	}
	else
	{
		SetKeyboardFocus();
	}
}

void UFrontierCommonPopupWidget::HidePopup()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UFrontierCommonPopupWidget::HandleConfirmClicked()
{
	OnPopupAction.Broadcast(EFrontierPopupResult::Confirmed);
}

void UFrontierCommonPopupWidget::HandleCancelClicked()
{
	OnPopupAction.Broadcast(EFrontierPopupResult::Cancelled);
}

FText UFrontierCommonPopupWidget::GetDefaultTitle(const EFrontierPopupType Type)
{
	switch (Type)
	{
	case EFrontierPopupType::Success:
		return NSLOCTEXT("FrontierCommonPopup", "SuccessTitle", "완료");
	case EFrontierPopupType::Warning:
		return NSLOCTEXT("FrontierCommonPopup", "WarningTitle", "경고");
	case EFrontierPopupType::Error:
		return NSLOCTEXT("FrontierCommonPopup", "ErrorTitle", "오류");
	case EFrontierPopupType::Confirmation:
		return NSLOCTEXT("FrontierCommonPopup", "ConfirmationTitle", "확인");
	default:
		return NSLOCTEXT("FrontierCommonPopup", "InformationTitle", "알림");
	}
}

FText UFrontierCommonPopupWidget::GetDefaultConfirmText(const EFrontierPopupType Type)
{
	return Type == EFrontierPopupType::Confirmation
		? NSLOCTEXT("FrontierCommonPopup", "DefaultYes", "예")
		: NSLOCTEXT("FrontierCommonPopup", "DefaultOkay", "확인");
}

void UFrontierCommonPopupWidget::CaptureAuthoredVisualStyle()
{
	if (bHasCapturedAuthoredVisualStyle)
	{
		return;
	}
	if (PopupIconImage)
	{
		AuthoredIconBrush = PopupIconImage->GetBrush();
		AuthoredIconVisibility = PopupIconImage->GetVisibility();
	}
	if (PopupBackgroundImage)
	{
		AuthoredBackgroundBrush = PopupBackgroundImage->GetBrush();
	}
	if (PopupTitleText)
	{
		AuthoredTitleColor = PopupTitleText->GetColorAndOpacity();
	}
	if (PopupMessageText)
	{
		AuthoredMessageColor = PopupMessageText->GetColorAndOpacity();
	}
	if (PopupConfirmButton)
	{
		AuthoredConfirmButtonColor = PopupConfirmButton->GetBackgroundColor();
	}
	if (PopupCancelButton)
	{
		AuthoredCancelButtonColor = PopupCancelButton->GetBackgroundColor();
	}
	bHasCapturedAuthoredVisualStyle = true;
}

void UFrontierCommonPopupWidget::ApplyVisualStyle(const EFrontierPopupType Type)
{
	CaptureAuthoredVisualStyle();
	if (PopupIconImage)
	{
		PopupIconImage->SetBrush(AuthoredIconBrush);
		PopupIconImage->SetVisibility(AuthoredIconVisibility);
	}
	if (PopupBackgroundImage)
	{
		PopupBackgroundImage->SetBrush(AuthoredBackgroundBrush);
	}
	if (PopupTitleText)
	{
		PopupTitleText->SetColorAndOpacity(AuthoredTitleColor);
	}
	if (PopupMessageText)
	{
		PopupMessageText->SetColorAndOpacity(AuthoredMessageColor);
	}
	if (PopupConfirmButton)
	{
		PopupConfirmButton->SetBackgroundColor(AuthoredConfirmButtonColor);
	}
	if (PopupCancelButton)
	{
		PopupCancelButton->SetBackgroundColor(AuthoredCancelButtonColor);
	}

	const FFrontierPopupVisualStyle* Style = PopupStyles.Find(Type);
	if (!Style)
	{
		return;
	}
	if (PopupIconImage && Style->bOverrideIconBrush)
	{
		PopupIconImage->SetBrush(Style->IconBrush);
		const bool bHasRenderableIcon = Style->IconBrush.DrawAs != ESlateBrushDrawType::NoDrawType
			&& (Style->IconBrush.GetResourceObject() || Style->IconBrush.DrawAs != ESlateBrushDrawType::Image);
		PopupIconImage->SetVisibility(!bHasRenderableIcon
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	}
	if (PopupBackgroundImage && Style->bOverrideBackgroundBrush)
	{
		PopupBackgroundImage->SetBrush(Style->BackgroundBrush);
	}
	if (PopupTitleText && Style->bOverrideTextColors)
	{
		PopupTitleText->SetColorAndOpacity(FSlateColor(Style->TitleColor));
	}
	if (PopupMessageText && Style->bOverrideTextColors)
	{
		PopupMessageText->SetColorAndOpacity(FSlateColor(Style->MessageColor));
	}
	if (PopupConfirmButton && Style->bOverrideButtonColors)
	{
		PopupConfirmButton->SetBackgroundColor(Style->ConfirmButtonColor);
	}
	if (PopupCancelButton && Style->bOverrideButtonColors)
	{
		PopupCancelButton->SetBackgroundColor(Style->CancelButtonColor);
	}
}
