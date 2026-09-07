#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierChangeNicknameWidget.generated.h"

class UButton;
class UEditableTextBox;

UCLASS()
class FRONTIER_API UFrontierChangeNicknameWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Open(bool bInCanCancel, const FString& CurrentNickname);
	void Close();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleNicknameUpdateSucceeded(const FString& Nickname);

	UFUNCTION()
	void HandleNicknameUpdateFailed(const FString& ErrorMessage);

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UEditableTextBox> EditableText_NickName;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_Confirm;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_Cancel;

	bool bCanCancel = true;
	bool bRequestInProgress = false;
};
