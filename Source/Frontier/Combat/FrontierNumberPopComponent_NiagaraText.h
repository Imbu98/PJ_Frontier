#pragma once

#include "Combat/FrontierNumberPopComponent.h"
#include "FrontierNumberPopComponent_NiagaraText.generated.h"

class UNiagaraSystem;

UCLASS(Blueprintable, ClassGroup=(Frontier), meta=(BlueprintSpawnableComponent))
class FRONTIER_API UFrontierNumberPopComponent_NiagaraText : public UFrontierNumberPopComponent
{
	GENERATED_BODY()

public:
	UFrontierNumberPopComponent_NiagaraText(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void AddNumberPop(const FFrontierNumberPopRequest& NewRequest) override;

protected:
	FLinearColor ResolveDamageColor(const FFrontierNumberPopRequest& Request) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Number Pop|Style")
	TObjectPtr<UNiagaraSystem> TextNiagara;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Number Pop|Style")
	FName NiagaraArrayName = TEXT("User.DamageInfo");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Number Pop|Style")
	FName NiagaraColorParameterName = TEXT("User.DamageColor");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Number Pop|Style")
	TMap<EFrontierElementalType, FLinearColor> ElementalDamageColors;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Number Pop|Style")
	FLinearColor CriticalDamageColor = FLinearColor(1.0f, 0.75f, 0.05f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Number Pop|Style")
	FLinearColor ReceivedDamageColor = FLinearColor::Red;

};
