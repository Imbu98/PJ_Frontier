#include "Interaction/FrontierRaidDeployActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Frontier.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierGameMode.h"
#include "Game/FrontierGameState.h"
#include "Game/FrontierPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

AFrontierRaidDeployActor::AFrontierRaidDeployActor()
{
	InteractionDisplayName = FText::FromString(TEXT("Raid Ready"));
}

bool AFrontierRaidDeployActor::CanInteract(const AFrontierPlayerController* InteractingController) const
{
	if (!InteractingController)
	{
		return false;
	}

	if (RaidTravelMode == EFrontierTravelMode::MapPackage && RaidLevel.IsNull())
	{
		return false;
	}

	if (RaidTravelMode == EFrontierTravelMode::ServerAddress && RaidServerAddress.IsEmpty())
	{
		return false;
	}

	const APawn* InteractingPawn = InteractingController->GetPawn();
	return InteractingPawn && FVector::DistSquared(InteractingPawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(InteractionRange);
}

void AFrontierRaidDeployActor::Interacted(AFrontierPlayerController* InteractingController)
{
	if (!HasAuthority() || !CanInteract(InteractingController) || !InteractingController || bRaidTravelQueued)
	{
		return;
	}

	const FString RaidDestination = RaidTravelMode == EFrontierTravelMode::ServerAddress
		? RaidServerAddress
		: RaidLevel.ToSoftObjectPath().GetLongPackageName();
	if (RaidDestination.IsEmpty())
	{
		FRONTIER_LOG(Warning, TEXT("Raid deploy interaction failed because raid destination is empty. Actor=%s"), *GetNameSafe(this));
		return;
	}

	AFrontierPlayerState* FrontierPlayerState = InteractingController->GetPlayerState<AFrontierPlayerState>();
	if (!FrontierPlayerState)
	{
		return;
	}

	if (FrontierPlayerState->GetTeamId() <= 0)
	{
		//InteractingController->ClientShowPlayerStatusWarning(TEXT("숫자키를 눌러 팀을 먼저 정하세요."));
		return;
	}

	const bool bNewReadyState = !FrontierPlayerState->IsRaidReady();
	FrontierPlayerState->SetRaidReady(bNewReadyState);
	InteractingController->ClientNotifyRaidReadyState(bNewReadyState);

	const AFrontierGameState* FrontierGameState = GetWorld() ? GetWorld()->GetGameState<AFrontierGameState>() : nullptr;
	if (!FrontierGameState || !FrontierGameState->AreAllConnectedPlayersReady())
	{
		return;
	}

	PendingRaidDestination = RaidDestination;
	bRaidTravelQueued = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RaidTravelTimerHandle,
			this,
			&AFrontierRaidDeployActor::BeginRaidTravelDeferred,
			FMath::Max(0.0f, ReadySyncDelaySeconds),
			false);
	}
}

FText AFrontierRaidDeployActor::GetInteractionDisplayName(const AFrontierPlayerController* InteractingController) const
{
	return InteractionDisplayName;
}

FText AFrontierRaidDeployActor::GetInteractionActionText(const AFrontierPlayerController* InteractingController) const
{
	return NSLOCTEXT("FrontierInteraction", "OpenRaidAction", "열기");
}

FText AFrontierRaidDeployActor::GetInteractionPromptText(const AFrontierPlayerController* InteractingController) const
{
	return FText::FromString(TEXT("Press F to toggle ready"));
}

FVector AFrontierRaidDeployActor::GetInteractionWorldLocation() const
{
	return MeshComponent ? MeshComponent->GetComponentLocation() : GetActorLocation();
}

void AFrontierRaidDeployActor::GetInteractionHighlightComponents(TArray<UPrimitiveComponent*>& OutComponents) const
{
	OutComponents.Reset();
	if (MeshComponent)
	{
		OutComponents.Add(MeshComponent);
	}
}

void AFrontierRaidDeployActor::TryBeginRaidTravel(const FString& RaidDestination) const
{
	UWorld* World = GetWorld();
	if (!World || RaidDestination.IsEmpty())
	{
		return;
	}

	if (RaidTravelMode == EFrontierTravelMode::MapPackage)
	{
		const FString TravelURL = GetNetMode() == NM_ListenServer
			? FString::Printf(TEXT("%s?listen"), *RaidDestination)
			: RaidDestination;
		World->ServerTravel(TravelURL);
		return;
	}

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (PlayerController)
		{
			PlayerController->ClientTravel(RaidDestination, TRAVEL_Absolute);
		}
	}
}

void AFrontierRaidDeployActor::BeginRaidTravelDeferred()
{
	bRaidTravelQueued = false;
	const FString RaidDestination = PendingRaidDestination;
	PendingRaidDestination.Reset();
	TryBeginRaidTravel(RaidDestination);
}
