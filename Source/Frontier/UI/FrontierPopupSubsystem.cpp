#include "UI/FrontierPopupSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Frontier.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UI/FrontierCommonPopupWidget.h"
#include "UI/FrontierUISettings.h"

UFrontierPopupSubsystem* UFrontierPopupSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	APlayerController* PlayerController = Cast<APlayerController>(const_cast<UObject*>(WorldContextObject));
	if (!PlayerController)
	{
		if (const UUserWidget* UserWidget = Cast<UUserWidget>(WorldContextObject))
		{
			PlayerController = UserWidget->GetOwningPlayer();
		}
	}
	if (!PlayerController)
	{
		PlayerController = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	}
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UFrontierPopupSubsystem>() : nullptr;
}

int32 UFrontierPopupSubsystem::ShowPopup(const FFrontierPopupRequest& Request)
{
	if (!EnsurePopupWidget())
	{
		return INDEX_NONE;
	}

	if (NextRequestId <= 0)
	{
		NextRequestId = 1;
	}
	FQueuedPopup QueuedPopup;
	QueuedPopup.RequestId = NextRequestId++;
	QueuedPopup.Request = Request;

	if (!bHasActiveRequest)
	{
		DisplayRequest(QueuedPopup);
	}
	else if (Request.QueuePolicy == EFrontierPopupQueuePolicy::ReplaceCurrent)
	{
		PopupQueue.Insert(QueuedPopup, 0);
		ResolveActivePopup(EFrontierPopupResult::Dismissed);
	}
	else
	{
		PopupQueue.Add(QueuedPopup);
	}
	return QueuedPopup.RequestId;
}

int32 UFrontierPopupSubsystem::ShowMessage(
	const EFrontierPopupType Type,
	const FText& Title,
	const FText& Message)
{
	FFrontierPopupRequest Request;
	Request.Type = Type;
	Request.Title = Title;
	Request.Message = Message;
	return ShowPopup(Request);
}

int32 UFrontierPopupSubsystem::ShowDismissibleMessage(
	const EFrontierPopupType Type,
	const FText& Title,
	const FText& Message)
{
	FFrontierPopupRequest Request;
	Request.Type = Type;
	Request.Title = Title;
	Request.Message = Message;
	Request.bShowConfirmButton = false;
	Request.bShowCancelButton = true;
	return ShowPopup(Request);
}

int32 UFrontierPopupSubsystem::ShowConfirmation(const FText& Title, const FText& Message)
{
	FFrontierPopupRequest Request;
	Request.Type = EFrontierPopupType::Confirmation;
	Request.Title = Title;
	Request.Message = Message;
	Request.bShowConfirmButton = true;
	Request.bShowCancelButton = true;
	return ShowPopup(Request);
}

bool UFrontierPopupSubsystem::CancelPopup(const int32 RequestId)
{
	if (RequestId == INDEX_NONE)
	{
		return false;
	}
	if (bHasActiveRequest && ActiveRequestId == RequestId)
	{
		ResolveActivePopup(EFrontierPopupResult::Dismissed);
		return true;
	}

	const int32 QueuedIndex = PopupQueue.IndexOfByPredicate(
		[RequestId](const FQueuedPopup& Entry)
		{
			return Entry.RequestId == RequestId;
		});
	if (QueuedIndex == INDEX_NONE)
	{
		return false;
	}
	PopupQueue.RemoveAt(QueuedIndex);
	OnPopupResolved.Broadcast(RequestId, EFrontierPopupResult::Dismissed);
	return true;
}

void UFrontierPopupSubsystem::DismissAllPopups()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoCloseTimerHandle);
	}

	TArray<int32> DismissedRequestIds;
	if (bHasActiveRequest)
	{
		DismissedRequestIds.Add(ActiveRequestId);
	}
	for (const FQueuedPopup& Entry : PopupQueue)
	{
		DismissedRequestIds.Add(Entry.RequestId);
	}

	const bool bWasVisible = bHasActiveRequest;
	bHasActiveRequest = false;
	ActiveRequestId = INDEX_NONE;
	PopupQueue.Reset();
	if (PopupWidget)
	{
		PopupWidget->HidePopup();
	}
	RestorePreviousInputMode();
	if (bWasVisible)
	{
		OnPopupVisibilityChanged.Broadcast(false);
	}
	for (const int32 RequestId : DismissedRequestIds)
	{
		OnPopupResolved.Broadcast(RequestId, EFrontierPopupResult::Dismissed);
	}
}

void UFrontierPopupSubsystem::SetPopupWidgetClassOverride(
	const TSubclassOf<UFrontierCommonPopupWidget> PopupWidgetClass)
{
	if (PopupWidgetClassOverride == PopupWidgetClass)
	{
		return;
	}
	DismissAllPopups();
	ReleasePopupWidget();
	PopupWidgetClassOverride = PopupWidgetClass;
}

void UFrontierPopupSubsystem::Deinitialize()
{
	DismissAllPopups();
	ReleasePopupWidget();
	Super::Deinitialize();
}

void UFrontierPopupSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	const bool bShouldRedisplayActiveRequest = bHasActiveRequest;
	if (PopupWidget && PopupWidget->GetOwningPlayer() != NewPlayerController)
	{
		RestorePreviousInputMode();
		ReleasePopupWidget();
	}
	if (bShouldRedisplayActiveRequest && EnsurePopupWidget())
	{
		PopupWidget->PresentRequest(ActiveRequest);
		ApplyPopupInputMode();
	}
}

bool UFrontierPopupSubsystem::EnsurePopupWidget()
{
	APlayerController* PlayerController = GetLocalPlayer()
		? GetLocalPlayer()->GetPlayerController(GetWorld())
		: nullptr;
	if (!PlayerController)
	{
		return false;
	}
	if (PopupWidget && PopupWidget->GetOwningPlayer() == PlayerController)
	{
		return true;
	}

	ReleasePopupWidget();
	TSubclassOf<UFrontierCommonPopupWidget> WidgetClass = PopupWidgetClassOverride;
	if (!WidgetClass)
	{
		const UFrontierUISettings* Settings = GetDefault<UFrontierUISettings>();
		WidgetClass = Settings ? Settings->CommonPopupWidgetClass.LoadSynchronous() : nullptr;
	}
	if (!WidgetClass)
	{
		FRONTIER_LOG(Error, TEXT("Common popup requested but CommonPopupWidgetClass is not configured in Project Settings > Game > Frontier UI."));
		return false;
	}

	PopupWidget = CreateWidget<UFrontierCommonPopupWidget>(PlayerController, WidgetClass);
	if (!PopupWidget)
	{
		FRONTIER_LOG(Error, TEXT("Failed to create common popup widget. Class=%s"), *GetNameSafe(WidgetClass));
		return false;
	}
	PopupWidget->OnPopupAction.AddUniqueDynamic(this, &UFrontierPopupSubsystem::HandlePopupAction);
	const UFrontierUISettings* Settings = GetDefault<UFrontierUISettings>();
	// Normal project screens (including the full-screen skill tree) use AddToViewport.
	// Using AddToPlayerScreen here creates a separate layer where Z-order cannot place
	// this popup above those screens, leaving an active but visually hidden modal.
	PopupWidget->AddToViewport(Settings ? Settings->CommonPopupZOrder : 1000);
	PopupWidget->HidePopup();
	return true;
}

void UFrontierPopupSubsystem::ReleasePopupWidget()
{
	if (!PopupWidget)
	{
		return;
	}
	PopupWidget->OnPopupAction.RemoveDynamic(this, &UFrontierPopupSubsystem::HandlePopupAction);
	PopupWidget->RemoveFromParent();
	PopupWidget = nullptr;
}

void UFrontierPopupSubsystem::DisplayRequest(const FQueuedPopup& QueuedPopup)
{
	if (!EnsurePopupWidget())
	{
		OnPopupResolved.Broadcast(QueuedPopup.RequestId, EFrontierPopupResult::Dismissed);
		DisplayNextQueuedPopup();
		return;
	}

	const bool bWasPopupLayerActive = InputOwnerController != nullptr;
	bHasActiveRequest = true;
	ActiveRequestId = QueuedPopup.RequestId;
	ActiveRequest = QueuedPopup.Request;
	PopupWidget->PresentRequest(ActiveRequest);
	ApplyPopupInputMode();
	if (!bWasPopupLayerActive)
	{
		OnPopupVisibilityChanged.Broadcast(true);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoCloseTimerHandle);
		if (ActiveRequest.AutoCloseSeconds > 0.0f)
		{
			World->GetTimerManager().SetTimer(
				AutoCloseTimerHandle,
				this,
				&UFrontierPopupSubsystem::HandleAutoClose,
				ActiveRequest.AutoCloseSeconds,
				false);
		}
	}
}

void UFrontierPopupSubsystem::DisplayNextQueuedPopup()
{
	if (bHasActiveRequest || PopupQueue.IsEmpty())
	{
		return;
	}
	const FQueuedPopup NextPopup = PopupQueue[0];
	PopupQueue.RemoveAt(0);
	DisplayRequest(NextPopup);
}

void UFrontierPopupSubsystem::ResolveActivePopup(const EFrontierPopupResult Result)
{
	if (!bHasActiveRequest)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoCloseTimerHandle);
	}

	const int32 ResolvedRequestId = ActiveRequestId;
	bHasActiveRequest = false;
	ActiveRequestId = INDEX_NONE;
	if (PopupWidget)
	{
		PopupWidget->HidePopup();
	}
	OnPopupResolved.Broadcast(ResolvedRequestId, Result);

	if (!bHasActiveRequest && !PopupQueue.IsEmpty())
	{
		DisplayNextQueuedPopup();
	}
	if (!bHasActiveRequest)
	{
		RestorePreviousInputMode();
		OnPopupVisibilityChanged.Broadcast(false);
	}
}

void UFrontierPopupSubsystem::ApplyPopupInputMode()
{
	APlayerController* PlayerController = PopupWidget ? PopupWidget->GetOwningPlayer() : nullptr;
	if (!PlayerController)
	{
		return;
	}

	if (!InputOwnerController)
	{
		InputOwnerController = PlayerController;
		bPreviousShowMouseCursor = PlayerController->bShowMouseCursor;
		if (FSlateApplication::IsInitialized())
		{
			PreviousFocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
		}
		if (!bPreviousShowMouseCursor)
		{
			PlayerController->bShowMouseCursor = true;
			FInputModeGameAndUI InputMode;
			InputMode.SetWidgetToFocus(PopupWidget->TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PlayerController->SetInputMode(InputMode);
			bInputModeOverridden = true;
		}
	}
	PopupWidget->SetUserFocus(PlayerController);
	PopupWidget->SetKeyboardFocus();
}

void UFrontierPopupSubsystem::RestorePreviousInputMode()
{
	if (!InputOwnerController)
	{
		return;
	}
	InputOwnerController->bShowMouseCursor = bPreviousShowMouseCursor;
	if (bInputModeOverridden)
	{
		InputOwnerController->SetInputMode(FInputModeGameOnly());
	}
	else if (FSlateApplication::IsInitialized() && PreviousFocusedWidget.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(PreviousFocusedWidget.Pin(), EFocusCause::SetDirectly);
	}

	InputOwnerController = nullptr;
	bInputModeOverridden = false;
	bPreviousShowMouseCursor = false;
	PreviousFocusedWidget.Reset();
}

void UFrontierPopupSubsystem::HandlePopupAction(const EFrontierPopupResult Result)
{
	ResolveActivePopup(Result);
}

void UFrontierPopupSubsystem::HandleAutoClose()
{
	ResolveActivePopup(EFrontierPopupResult::Dismissed);
}
