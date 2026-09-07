#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UI/FrontierCommonPopupTypes.h"
#include "FrontierPopupSubsystem.generated.h"

class APlayerController;
class SWidget;
class UFrontierCommonPopupWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FFrontierPopupResolvedSignature,
	int32,
	RequestId,
	EFrontierPopupResult,
	Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FFrontierPopupVisibilityChangedSignature,
	bool,
	bVisible);

/**
 * Per-local-player popup manager shared by lobby, gameplay, and all feature UI.
 * It owns one WBP instance, serializes requests through a FIFO queue, and routes results by request ID.
 */
UCLASS()
class FRONTIER_API UFrontierPopupSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	/** Finds the correct local-player popup subsystem from a widget, controller, actor, or world context. */
	UFUNCTION(BlueprintPure, Category="Frontier|Popup", meta=(WorldContext="WorldContextObject"))
	static UFrontierPopupSubsystem* Get(const UObject* WorldContextObject);

	/** Returns INDEX_NONE when no local player or CommonPopupWidgetClass is configured. */
	UFUNCTION(BlueprintCallable, Category="Frontier|Popup")
	int32 ShowPopup(const FFrontierPopupRequest& Request);

	UFUNCTION(BlueprintCallable, Category="Frontier|Popup")
	int32 ShowMessage(EFrontierPopupType Type, const FText& Title, const FText& Message);

	/** Shows a single Cancel/Close button instead of the normal Confirm/Okay button. */
	UFUNCTION(BlueprintCallable, Category="Frontier|Popup")
	int32 ShowDismissibleMessage(EFrontierPopupType Type, const FText& Title, const FText& Message);

	UFUNCTION(BlueprintCallable, Category="Frontier|Popup")
	int32 ShowConfirmation(const FText& Title, const FText& Message);

	/** Dismisses an active request or removes a queued request. */
	UFUNCTION(BlueprintCallable, Category="Frontier|Popup")
	bool CancelPopup(int32 RequestId);

	UFUNCTION(BlueprintCallable, Category="Frontier|Popup")
	void DismissAllPopups();

	UFUNCTION(BlueprintPure, Category="Frontier|Popup")
	bool IsPopupVisible() const { return bHasActiveRequest; }

	UFUNCTION(BlueprintPure, Category="Frontier|Popup")
	int32 GetActiveRequestId() const { return bHasActiveRequest ? ActiveRequestId : INDEX_NONE; }

	/** Runtime override useful for tests or projects that assign UI classes outside Developer Settings. */
	UFUNCTION(BlueprintCallable, Category="Frontier|Popup")
	void SetPopupWidgetClassOverride(TSubclassOf<UFrontierCommonPopupWidget> PopupWidgetClass);

	UPROPERTY(BlueprintAssignable, Category="Frontier|Popup")
	FFrontierPopupResolvedSignature OnPopupResolved;

	UPROPERTY(BlueprintAssignable, Category="Frontier|Popup")
	FFrontierPopupVisibilityChangedSignature OnPopupVisibilityChanged;

private:
	struct FQueuedPopup
	{
		int32 RequestId = INDEX_NONE;
		FFrontierPopupRequest Request;
	};

	bool EnsurePopupWidget();
	void ReleasePopupWidget();
	void DisplayRequest(const FQueuedPopup& QueuedPopup);
	void DisplayNextQueuedPopup();
	void ResolveActivePopup(EFrontierPopupResult Result);
	void ApplyPopupInputMode();
	void RestorePreviousInputMode();

	UFUNCTION()
	void HandlePopupAction(EFrontierPopupResult Result);

	UFUNCTION()
	void HandleAutoClose();

	UPROPERTY(Transient)
	TObjectPtr<UFrontierCommonPopupWidget> PopupWidget;

	UPROPERTY(Transient)
	TSubclassOf<UFrontierCommonPopupWidget> PopupWidgetClassOverride;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> InputOwnerController;

	TArray<FQueuedPopup> PopupQueue;
	FFrontierPopupRequest ActiveRequest;
	int32 ActiveRequestId = INDEX_NONE;
	int32 NextRequestId = 1;
	bool bHasActiveRequest = false;
	bool bInputModeOverridden = false;
	bool bPreviousShowMouseCursor = false;
	TWeakPtr<SWidget> PreviousFocusedWidget;
	FTimerHandle AutoCloseTimerHandle;
};
