#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "FrontierAbilitySystemComponent.generated.h"

UCLASS(ClassGroup=(Frontier), meta=(BlueprintSpawnableComponent))
class FRONTIER_API UFrontierAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UFrontierAbilitySystemComponent();

	void InitializeAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor);

	/** Applies an authoritative stamina delta through GAS. Positive values restore stamina. */
	bool ApplyStaminaDelta(float StaminaDelta);
};


