#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierCrossbowWidget.generated.h"

class UWidget;

UCLASS(Abstract, Blueprintable)
class FRONTIER_API UFrontierCrossbowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Frontier|Bow")
	void SetChargeProgress(float InChargeProgress);

	UFUNCTION(BlueprintPure, Category="Frontier|Bow")
	float GetChargeProgress() const { return ChargeProgress; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category="Frontier|Bow", meta=(DisplayName="On Charge Progress Changed"))
	void BP_OnChargeProgressChanged(float NewChargeProgress);

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> ChargeReticle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Bow|Reticle", meta=(ClampMin="0.01"))
	float UnchargedReticleScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Bow|Reticle", meta=(ClampMin="0.01"))
	float FullyChargedReticleScale = 0.35f;

private:
	UPROPERTY(Transient)
	float ChargeProgress = 0.0f;
};
