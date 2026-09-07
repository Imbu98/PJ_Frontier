#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Warning/AttackWarningTypes.h"
#include "FrontierGameState.generated.h"

class AFrontierPlayerState;

UCLASS()
class FRONTIER_API AFrontierGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShowAttackWarning(FGuid WarningId, const FAttackWarningData& WarningData);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHideAttackWarning(FGuid WarningId);

	UFUNCTION(BlueprintCallable, Category="Frontier|Team")
	void GetPlayersInTeam(int32 TeamId, TArray<AFrontierPlayerState*>& OutPlayers) const;

	UFUNCTION(BlueprintCallable, Category="Frontier|Lobby")
	void GetConnectedFrontierPlayers(TArray<AFrontierPlayerState*>& OutPlayers) const;

	UFUNCTION(BlueprintPure, Category="Frontier|Lobby")
	int32 GetReadyPlayerCount() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Lobby")
	int32 GetConnectedPlayerCount() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Lobby")
	bool AreAllConnectedPlayersReady() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Raid")
	int32 GetRaidRemainingTimeSeconds() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Raid")
	bool IsRaidTimerActive() const;

	void SetRaidTimerState(bool bInActive, int32 InRemainingTimeSeconds);

protected:
	UFUNCTION()
	void OnRep_RaidRemainingTimeSeconds();

	UFUNCTION()
	void OnRep_RaidTimerActive();

	UPROPERTY(ReplicatedUsing=OnRep_RaidRemainingTimeSeconds, VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Raid")
	int32 RaidRemainingTimeSeconds = 0;

	UPROPERTY(ReplicatedUsing=OnRep_RaidTimerActive, VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Raid")
	bool bRaidTimerActive = false;
};
