#include "UI/FrontierNicknameEntryWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

void UFrontierNicknameEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.RemoveAll(this);
		ConfirmButton->OnClicked.AddDynamic(this, &UFrontierNicknameEntryWidget::HandleConfirmClicked);
	}

	if (NicknameTextBox)
	{
		NicknameTextBox->OnTextCommitted.RemoveAll(this);
		NicknameTextBox->OnTextCommitted.AddDynamic(this, &UFrontierNicknameEntryWidget::HandleNicknameTextCommitted);
		NicknameTextBox->SetKeyboardFocus();
	}

	SetSuggestedNickname(SuggestedNickname);
	ClearValidationMessage();
}

void UFrontierNicknameEntryWidget::SetSuggestedNickname(const FString& InSuggestedNickname)
{
	SuggestedNickname = InSuggestedNickname;

	if (NicknameTextBox)
	{
		NicknameTextBox->SetText(FText::FromString(SuggestedNickname));
	}
}

void UFrontierNicknameEntryWidget::ShowValidationMessage(const FText& InMessage)
{
	if (ValidationText)
	{
		ValidationText->SetText(InMessage);
		ValidationText->SetVisibility(ESlateVisibility::Visible);
	}
}

void UFrontierNicknameEntryWidget::ClearValidationMessage()
{
	if (ValidationText)
	{
		ValidationText->SetText(FText::GetEmpty());
		ValidationText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UWidget* UFrontierNicknameEntryWidget::GetPreferredFocusTarget() const
{
	return NicknameTextBox ? static_cast<UWidget*>(NicknameTextBox.Get()) : ConfirmButton.Get();
}

void UFrontierNicknameEntryWidget::HandleConfirmClicked()
{
	if (!NicknameTextBox)
	{
		return;
	}

	OnNicknameSubmitted.Broadcast(NicknameTextBox->GetText().ToString());
}

void UFrontierNicknameEntryWidget::HandleNicknameTextCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		OnNicknameSubmitted.Broadcast(Text.ToString());
	}
}
