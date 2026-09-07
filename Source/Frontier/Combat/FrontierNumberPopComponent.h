#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Inventory/FrontierItemSharedTypes.h"
#include "FrontierNumberPopComponent.generated.h"

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierNumberPopRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Number Pop")
	FVector WorldLocation = FVector::ZeroVector;

	/** Replicated target used by the receiving client to resolve DamageSocket. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Number Pop")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Number Pop")
	FGameplayTagContainer SourceTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Number Pop")
	FGameplayTagContainer TargetTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Number Pop")
	int32 NumberToDisplay = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Number Pop")
	bool bIsCriticalDamage = false;

	/** True for the recipient that owns the damaged character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Number Pop")
	bool bIsReceivedDamage = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Number Pop")
	EFrontierElementalType ElementalType = EFrontierElementalType::Normal;
};

UCLASS(Abstract, ClassGroup=(Frontier))
class FRONTIER_API UFrontierNumberPopComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFrontierNumberPopComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category="Frontier|Number Pop")
	virtual void AddNumberPop(const FFrontierNumberPopRequest& NewRequest);
};
