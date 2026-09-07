#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierInteractionProgressWidget.generated.h"

/** Logic-only parent for a designer-authored interaction progress WBP. */
UCLASS(Abstract, Blueprintable)
class FRONTIER_API UFrontierInteractionProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Frontier|Interaction")
	void SetInteractionProgress(float InProgress);

	UFUNCTION(BlueprintImplementableEvent, Category="Frontier|Interaction", meta=(DisplayName="On Interaction Progress Changed"))
	void BP_OnInteractionProgressChanged(float NormalizedProgress);
private:
	UPROPERTY(Transient, BlueprintReadOnly, Category="Frontier|Interaction", meta=(AllowPrivateAccess="true"))
	float Progress = 0.0f;
};
