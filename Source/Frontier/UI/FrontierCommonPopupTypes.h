#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "FrontierCommonPopupTypes.generated.h"

UENUM(BlueprintType)
enum class EFrontierPopupType : uint8
{
	Information,
	Success,
	Warning,
	Error,
	Confirmation
};

UENUM(BlueprintType)
enum class EFrontierPopupResult : uint8
{
	Confirmed,
	Cancelled,
	Dismissed
};

UENUM(BlueprintType)
enum class EFrontierPopupQueuePolicy : uint8
{
	/** Wait until the currently visible popup and earlier queued requests finish. */
	Enqueue,

	/** Dismiss the current popup and display this request before the existing queue. */
	ReplaceCurrent
};

/** Optional per-type visuals authored once on WBP_CommonPopup Class Defaults. */
USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierPopupVisualStyle
{
	GENERATED_BODY()

	/** Disabled keeps the icon authored directly in WBP_CommonPopup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup Style")
	bool bOverrideIconBrush = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup Style")
	FSlateBrush IconBrush;

	/** Disabled keeps the background authored directly in WBP_CommonPopup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup Style")
	bool bOverrideBackgroundBrush = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup Style")
	FSlateBrush BackgroundBrush;

	/** Disabled keeps both text colors authored directly in WBP_CommonPopup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup Style")
	bool bOverrideTextColors = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup Style")
	FLinearColor TitleColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup Style")
	FLinearColor MessageColor = FLinearColor::White;

	/** Disabled keeps both button colors authored directly in WBP_CommonPopup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup Style")
	bool bOverrideButtonColors = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup Style")
	FLinearColor ConfirmButtonColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup Style")
	FLinearColor CancelButtonColor = FLinearColor::White;
};

/** Display data shared by every common popup in lobby and gameplay. */
USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierPopupRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup")
	EFrontierPopupType Type = EFrontierPopupType::Information;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup", meta=(MultiLine=true))
	FText Message;

	/** Empty uses the type's localized default text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup")
	FText ConfirmButtonText;

	/** Empty uses the localized default cancel text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup")
	FText CancelButtonText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup")
	bool bShowConfirmButton = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup")
	bool bShowCancelButton = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup")
	bool bAllowEscape = true;

	/** Zero waits for user input. A positive value automatically dismisses the popup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup", meta=(ClampMin="0.0"))
	float AutoCloseSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup")
	EFrontierPopupQueuePolicy QueuePolicy = EFrontierPopupQueuePolicy::Enqueue;
};
