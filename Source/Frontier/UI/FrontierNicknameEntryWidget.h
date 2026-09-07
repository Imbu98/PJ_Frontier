#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierNicknameEntryWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FFrontierNicknameSubmittedSignature, const FString&);

UCLASS()
class FRONTIER_API UFrontierNicknameEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void SetSuggestedNickname(const FString& InSuggestedNickname);
	void ShowValidationMessage(const FText& InMessage);
	void ClearValidationMessage();
	UWidget* GetPreferredFocusTarget() const;

	FFrontierNicknameSubmittedSignature OnNicknameSubmitted;

protected:
	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleNicknameTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UEditableTextBox> NicknameTextBox;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ValidationText;

private:
	FString SuggestedNickname;
};
