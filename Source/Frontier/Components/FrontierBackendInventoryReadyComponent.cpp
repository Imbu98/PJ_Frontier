#include "Components/FrontierBackendInventoryReadyComponent.h"

#include "FrontierPlayerController.h"
#include "Game/FrontierGameMode.h"
#include "Game/FrontierPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

namespace
{
constexpr float BackendInventoryObservationRetryDelaySeconds = 0.2f;
}

UFrontierBackendInventoryReadyComponent::UFrontierBackendInventoryReadyComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UFrontierBackendInventoryReadyComponent::StartObservation(const bool bInLobbyContext)
{
	if (bObservationStarted)
	{
		return;
	}

	bObservationStarted = true;
	bLobbyContext = bInLobbyContext;
	AttemptObservation();
}

void UFrontierBackendInventoryReadyComponent::AttemptObservation()
{
	APlayerController* OwnerController = Cast<APlayerController>(GetOwner());
	if (!OwnerController || !OwnerController->IsLocalController())
	{
		return;
	}

	if (!OwnerController->GetPlayerState<AFrontierPlayerState>())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				InventoryInitTimerHandle,
				this,
				&UFrontierBackendInventoryReadyComponent::AttemptObservation,
				BackendInventoryObservationRetryDelaySeconds,
				false);
		}
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InventoryInitTimerHandle);
	}

	ServerConfirmBackendInventoryReady(bLobbyContext);
	OnBackendInventoryReady.Broadcast();
}

void UFrontierBackendInventoryReadyComponent::ServerConfirmBackendInventoryReady_Implementation(
	const bool bInLobbyContext)
{
	if (bInLobbyContext)
	{
		return;
	}

	AFrontierPlayerController* OwnerController = Cast<AFrontierPlayerController>(GetOwner());
	AFrontierPlayerState* PlayerState = OwnerController
		? OwnerController->GetPlayerState<AFrontierPlayerState>()
		: nullptr;
	if (!OwnerController || !PlayerState)
	{
		return;
	}

	if (AFrontierGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AFrontierGameMode>() : nullptr)
	{
		GameMode->TryRelocateControllerToRaidSpawn(OwnerController);
		GameMode->NotifyRaidControllerPrepared(OwnerController);
	}
}
