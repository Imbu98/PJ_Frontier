#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "FrontierBowAnimInstance.generated.h"

UCLASS(Blueprintable)
class FRONTIER_API UFrontierBowAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Bow")
	void SetChargeProgress(const float NewChargeProgress)
	{
		ChargeProgress = FMath::Clamp(NewChargeProgress, 0.0f, 1.0f);
	}

	UFUNCTION(BlueprintPure, Category="Bow")
	float GetChargeProgress() const
	{
		return ChargeProgress;
	}

	UPROPERTY(BlueprintReadOnly, Category="Bow")
	float ChargeProgress = 0.0f;
};
