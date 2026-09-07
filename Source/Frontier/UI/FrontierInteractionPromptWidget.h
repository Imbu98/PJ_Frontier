#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierInteractionPromptWidget.generated.h"

class UTextBlock;
class UVerticalBox;

UCLASS()
class FRONTIER_API UFrontierInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetPromptText(const FText& InPromptText);

protected:
	virtual void NativeConstruct() override;

private:
	void BuildWidgetTreeIfNeeded();

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> RootBox;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptText;

	FText CachedPromptText;
};
