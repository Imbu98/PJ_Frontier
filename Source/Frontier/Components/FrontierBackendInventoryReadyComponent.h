#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FrontierBackendInventoryReadyComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFrontierBackendInventoryReadySignature);

/** Waits for replicated backend inventory state to become available after travel. */
UCLASS(ClassGroup=(Frontier))
class FRONTIER_API UFrontierBackendInventoryReadyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFrontierBackendInventoryReadyComponent();

	void StartObservation(bool bInLobbyContext);

	UPROPERTY(BlueprintAssignable, Category="Frontier|Backend")
	FFrontierBackendInventoryReadySignature OnBackendInventoryReady;

protected:
	UFUNCTION(Server, Reliable)
	void ServerConfirmBackendInventoryReady(bool bInLobbyContext);

private:
	void AttemptObservation();

	FTimerHandle InventoryInitTimerHandle;
	bool bLobbyContext = false;
	bool bObservationStarted = false;
};
