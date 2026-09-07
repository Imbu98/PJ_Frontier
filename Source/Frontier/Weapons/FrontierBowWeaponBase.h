#pragma once

#include "CoreMinimal.h"
#include "Weapons/FrontierWeaponBase.h"
#include "FrontierBowWeaponBase.generated.h"

UCLASS()
class FRONTIER_API AFrontierBowWeaponBase : public AFrontierWeaponBase
{
	GENERATED_BODY()

public:
	AFrontierBowWeaponBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category="Weapon|Bow")
	void SetBowChargeProgress(float NewChargeProgress);

	UFUNCTION(BlueprintPure, Category="Weapon|Bow")
	float GetBowChargeProgress() const { return BowChargeProgress; }

protected:
	UFUNCTION()
	void OnRep_BowChargeProgress();

	void ApplyBowChargeProgressToAnimInstance();

	UPROPERTY(ReplicatedUsing=OnRep_BowChargeProgress, VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Bow")
	float BowChargeProgress = 0.0f;
};
