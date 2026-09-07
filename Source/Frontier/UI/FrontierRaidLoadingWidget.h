#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierRaidLoadingWidget.generated.h"

class UTextBlock;
class UProgressBar;

UCLASS()
class FRONTIER_API UFrontierRaidLoadingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetStatusMessage(const FText& InStatusMessage);
	void SetProgress(float InProgress);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar;

private:
	FText CachedStatusMessage = FText::FromString(TEXT("Loading..."));
	float CachedProgress = 0.0f;
};
