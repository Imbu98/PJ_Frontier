#include "Game/FrontierGameState.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Frontier.h"
#include "Game/FrontierPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Warning/AttackWarningSubsystem.h"

namespace
{
	void ForEachLocalWarningSubsystem(UWorld* World, TFunctionRef<void(UAttackWarningSubsystem&)> Callback)
	{
		if (!World || World->GetNetMode() == NM_DedicatedServer || !GEngine)
		{
			return;
		}

		UGameInstance* GameInstance = World->GetGameInstance();
		if (!GameInstance)
		{
			return;
		}

		for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
		{
			if (!LocalPlayer)
			{
				continue;
			}

			if (UAttackWarningSubsystem* WarningSubsystem = LocalPlayer->GetSubsystem<UAttackWarningSubsystem>())
			{
				Callback(*WarningSubsystem);
			}
		}
	}
}

void AFrontierGameState::MulticastShowAttackWarning_Implementation(const FGuid WarningId, const FAttackWarningData& WarningData)
{
	FRONTIER_LOG(Log, TEXT("Multicast show attack warning. WarningId=%s Location=%s Radius=%.2f Duration=%.2f"),
		*WarningId.ToString(),
		*WarningData.WarningLocation.ToCompactString(),
		WarningData.WarningRadius,
		WarningData.WarningDuration);

	ForEachLocalWarningSubsystem(GetWorld(), [WarningId, &WarningData](UAttackWarningSubsystem& WarningSubsystem)
	{
		WarningSubsystem.ShowWarning(WarningId, WarningData);
	});
}

void AFrontierGameState::MulticastHideAttackWarning_Implementation(const FGuid WarningId)
{
	FRONTIER_LOG(Log, TEXT("Multicast hide attack warning. WarningId=%s"), *WarningId.ToString());

	ForEachLocalWarningSubsystem(GetWorld(), [WarningId](UAttackWarningSubsystem& WarningSubsystem)
	{
		WarningSubsystem.HideWarning(WarningId);
	});
}

void AFrontierGameState::GetPlayersInTeam(const int32 TeamId, TArray<AFrontierPlayerState*>& OutPlayers) const
{
	OutPlayers.Reset();

	for (APlayerState* PlayerState : PlayerArray)
	{
		AFrontierPlayerState* FrontierPlayerState = Cast<AFrontierPlayerState>(PlayerState);
		if (FrontierPlayerState && FrontierPlayerState->GetTeamId() == TeamId)
		{
			OutPlayers.Add(FrontierPlayerState);
		}
	}
}

void AFrontierGameState::GetConnectedFrontierPlayers(TArray<AFrontierPlayerState*>& OutPlayers) const
{
	OutPlayers.Reset();

	for (APlayerState* PlayerState : PlayerArray)
	{
		AFrontierPlayerState* FrontierPlayerState = Cast<AFrontierPlayerState>(PlayerState);
		if (!FrontierPlayerState || FrontierPlayerState->IsOnlyASpectator())
		{
			continue;
		}

		OutPlayers.Add(FrontierPlayerState);
	}
}

int32 AFrontierGameState::GetReadyPlayerCount() const
{
	TArray<AFrontierPlayerState*> ConnectedPlayers;
	GetConnectedFrontierPlayers(ConnectedPlayers);

	int32 ReadyPlayerCount = 0;
	for (const AFrontierPlayerState* FrontierPlayerState : ConnectedPlayers)
	{
		if (FrontierPlayerState && FrontierPlayerState->IsRaidReady())
		{
			++ReadyPlayerCount;
		}
	}

	return ReadyPlayerCount;
}

int32 AFrontierGameState::GetConnectedPlayerCount() const
{
	TArray<AFrontierPlayerState*> ConnectedPlayers;
	GetConnectedFrontierPlayers(ConnectedPlayers);
	return ConnectedPlayers.Num();
}

bool AFrontierGameState::AreAllConnectedPlayersReady() const
{
	const int32 ConnectedPlayerCount = GetConnectedPlayerCount();
	return ConnectedPlayerCount > 0 && GetReadyPlayerCount() == ConnectedPlayerCount;
}

void AFrontierGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFrontierGameState, RaidRemainingTimeSeconds);
	DOREPLIFETIME(AFrontierGameState, bRaidTimerActive);
}

int32 AFrontierGameState::GetRaidRemainingTimeSeconds() const
{
	return RaidRemainingTimeSeconds;
}

bool AFrontierGameState::IsRaidTimerActive() const
{
	return bRaidTimerActive;
}

void AFrontierGameState::SetRaidTimerState(const bool bInActive, const int32 InRemainingTimeSeconds)
{
	bRaidTimerActive = bInActive;
	RaidRemainingTimeSeconds = FMath::Max(0, InRemainingTimeSeconds);
}

void AFrontierGameState::OnRep_RaidRemainingTimeSeconds()
{
}

void AFrontierGameState::OnRep_RaidTimerActive()
{
}
