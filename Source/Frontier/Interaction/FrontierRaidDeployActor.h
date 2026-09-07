#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Game/FrontierGameMode.h"
#include "Interaction/FrontierInteractableActor.h"
#include "FrontierRaidDeployActor.generated.h"

class AFrontierPlayerController;
class UPrimitiveComponent;
class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class FRONTIER_API AFrontierRaidDeployActor : public AFrontierInteractableActor
{
	GENERATED_BODY()

public:
	AFrontierRaidDeployActor();

	virtual bool CanInteract(const AFrontierPlayerController* InteractingController) const override;
	virtual void Interacted(AFrontierPlayerController* InteractingController) override;
	virtual FText GetInteractionDisplayName(const AFrontierPlayerController* InteractingController) const override;
	virtual FText GetInteractionActionText(const AFrontierPlayerController* InteractingController) const override;
	virtual FText GetInteractionPromptText(const AFrontierPlayerController* InteractingController) const override;
	virtual FVector GetInteractionWorldLocation() const override;
	virtual void GetInteractionHighlightComponents(TArray<UPrimitiveComponent*>& OutComponents) const override;

protected:
	void TryBeginRaidTravel(const FString& RaidDestination) const;
	void BeginRaidTravelDeferred();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Raid")
	TSoftObjectPtr<UWorld> RaidLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Raid")
	EFrontierTravelMode RaidTravelMode = EFrontierTravelMode::MapPackage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Raid", meta=(EditCondition="RaidTravelMode==EFrontierTravelMode::ServerAddress", EditConditionHides))
	FString RaidServerAddress;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Raid", meta=(ClampMin="0.0"))
	float ReadySyncDelaySeconds = 0.2f;

	UPROPERTY(Transient)
	bool bRaidTravelQueued = false;

	FTimerHandle RaidTravelTimerHandle;
	FString PendingRaidDestination;
};
