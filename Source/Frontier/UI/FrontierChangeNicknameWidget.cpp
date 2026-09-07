#include "UI/FrontierChangeNicknameWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/FrontierBackendProtocolComponent.h"
#include "Game/FrontierLobbyPlayerController.h"
#include "Frontier.h"

void UFrontierChangeNicknameWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.RemoveAll(this);
		Button_Confirm->OnClicked.AddDynamic(this, &ThisClass::HandleConfirmClicked);
	}
	if (Button_Cancel)
	{
		Button_Cancel->OnClicked.RemoveAll(this);
		Button_Cancel->OnClicked.AddDynamic(this, &ThisClass::HandleCancelClicked);
	}

	if (AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwningPlayer()))
	{
		if (UFrontierBackendProtocolComponent* Backend = Controller->GetBackendProtocolComponent())
		{
			Backend->OnNicknameUpdateSucceeded.AddUniqueDynamic(
				this,
				&ThisClass::HandleNicknameUpdateSucceeded);
			Backend->OnNicknameUpdateFailed.AddUniqueDynamic(
				this,
				&ThisClass::HandleNicknameUpdateFailed);
		}
	}
}

void UFrontierChangeNicknameWidget::NativeDestruct()
{
	if (AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwningPlayer()))
	{
		if (UFrontierBackendProtocolComponent* Backend = Controller->GetBackendProtocolComponent())
		{
			Backend->OnNicknameUpdateSucceeded.RemoveDynamic(
				this,
				&ThisClass::HandleNicknameUpdateSucceeded);
			Backend->OnNicknameUpdateFailed.RemoveDynamic(
				this,
				&ThisClass::HandleNicknameUpdateFailed);
		}
	}

	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.RemoveAll(this);
	}
	if (Button_Cancel)
	{
		Button_Cancel->OnClicked.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UFrontierChangeNicknameWidget::Open(const bool bInCanCancel, const FString& CurrentNickname)
{
	bCanCancel = bInCanCancel;
	bRequestInProgress = false;

	if (EditableText_NickName)
	{
		EditableText_NickName->SetText(FText::FromString(CurrentNickname));
		EditableText_NickName->SetKeyboardFocus();
	}
	if (Button_Cancel)
	{
		Button_Cancel->SetVisibility(bCanCancel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Button_Confirm)
	{
		Button_Confirm->SetIsEnabled(true);
	}
	SetVisibility(ESlateVisibility::Visible);
}

void UFrontierChangeNicknameWidget::Close()
{
	bRequestInProgress = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UFrontierChangeNicknameWidget::HandleConfirmClicked()
{
	if (bRequestInProgress || !EditableText_NickName)
	{
		return;
	}

	FString Nickname = EditableText_NickName->GetText().ToString();
	Nickname.TrimStartAndEndInline();
	UE_LOG(LogFrontier, Log, TEXT("[Nickname] Confirm clicked. NicknameLength=%d"), Nickname.Len());
	if (Nickname.IsEmpty())
	{
		HandleNicknameUpdateFailed(TEXT("닉네임을 입력해 주세요."));
		return;
	}

	AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwningPlayer());
	UFrontierBackendProtocolComponent* Backend = Controller
		? Controller->GetBackendProtocolComponent()
		: nullptr;
	if (!Backend)
	{
		HandleNicknameUpdateFailed(TEXT("백엔드 연결을 확인할 수 없습니다."));
		return;
	}

	bRequestInProgress = true;
	if (Button_Confirm)
	{
		Button_Confirm->SetIsEnabled(false);
	}
	Backend->RequestUpdateNickname(Nickname);
	UE_LOG(LogFrontier, Log, TEXT("[Nickname] Update request submitted."));
}

void UFrontierChangeNicknameWidget::HandleCancelClicked()
{
	if (bCanCancel && !bRequestInProgress)
	{
		Close();
	}
}

void UFrontierChangeNicknameWidget::HandleNicknameUpdateSucceeded(const FString& Nickname)
{
	bRequestInProgress = false;
	UE_LOG(LogFrontier, Log, TEXT("[Nickname] UI received update success. NicknameLength=%d. Closing widget."), Nickname.Len());
	Close();
}

void UFrontierChangeNicknameWidget::HandleNicknameUpdateFailed(const FString& ErrorMessage)
{
	bRequestInProgress = false;
	UE_LOG(LogFrontier, Warning, TEXT("[Nickname] UI received update failure. Error=%s"), *ErrorMessage);
	if (Button_Confirm)
	{
		Button_Confirm->SetIsEnabled(true);
	}
}
