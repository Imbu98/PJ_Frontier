#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/FrontierCommonPopupTypes.h"
#include "FrontierCommonPopupWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FFrontierCommonPopupActionSignature,
	EFrontierPopupResult,
	Result);

/** Visual parent for the single reusable popup WBP managed by UFrontierPopupSubsystem. */
UCLASS(Abstract, Blueprintable)
class FRONTIER_API UFrontierCommonPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void PresentRequest(const FFrontierPopupRequest& Request);
	void HidePopup();

	UPROPERTY(BlueprintAssignable, Category="Frontier|Popup")
	FFrontierCommonPopupActionSignature OnPopupAction;

	/** Different icon/background/color data for Information, Success, Warning, Error, and Confirmation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Popup|Style")
	TMap<EFrontierPopupType, FFrontierPopupVisualStyle> PopupStyles;

	UFUNCTION(BlueprintPure, Category="Frontier|Popup")
	EFrontierPopupType GetCurrentPopupType() const { return CurrentPopupType; }

	UFUNCTION(BlueprintImplementableEvent, Category="Frontier|Popup", meta=(DisplayName="On Popup Presented"))
	void BP_OnPopupPresented(const FFrontierPopupRequest& Request);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PopupTitleText;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PopupMessageText;

	/** Receives the icon brush from PopupStyles for the current popup type. */
	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> PopupIconImage;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> PopupBackgroundImage;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> PopupConfirmButton;

	/** Optional SizeBox/container used to give the confirm button a fixed width. */
	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> PopupConfirmButtonContainer;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PopupConfirmButtonText;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> PopupCancelButton;

	/** Optional SizeBox/container used to give the cancel button a fixed width. */
	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> PopupCancelButtonContainer;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PopupCancelButtonText;

	/** The fixed middle gap between the two buttons. Side fill spacers remain unbound. */
	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> PopupButtonGap;

private:
	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	static FText GetDefaultTitle(EFrontierPopupType Type);
	static FText GetDefaultConfirmText(EFrontierPopupType Type);
	void CaptureAuthoredVisualStyle();
	void ApplyVisualStyle(EFrontierPopupType Type);

	EFrontierPopupType CurrentPopupType = EFrontierPopupType::Information;
	bool bCurrentShowsConfirmButton = true;
	bool bCurrentShowsCancelButton = false;
	bool bCurrentAllowsEscape = true;
	bool bHasCapturedAuthoredVisualStyle = false;
	FSlateBrush AuthoredIconBrush;
	FSlateBrush AuthoredBackgroundBrush;
	FSlateColor AuthoredTitleColor = FSlateColor(FLinearColor::White);
	FSlateColor AuthoredMessageColor = FSlateColor(FLinearColor::White);
	FLinearColor AuthoredConfirmButtonColor = FLinearColor::White;
	FLinearColor AuthoredCancelButtonColor = FLinearColor::White;
	ESlateVisibility AuthoredIconVisibility = ESlateVisibility::Collapsed;
};
