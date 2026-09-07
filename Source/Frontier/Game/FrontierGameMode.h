#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "FrontierGameMode.generated.h"

class AFrontierPlayerController;
class AFrontierTeamPlayerStart;
enum class EFrontierRaidOutcome : uint8;

UENUM(BlueprintType)
enum class EFrontierTravelMode : uint8
{
	MapPackage UMETA(DisplayName="Map Package"),
	ServerAddress UMETA(DisplayName="Server Address")
};

UCLASS()
class FRONTIER_API AFrontierGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFrontierGameMode();
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual FString InitNewPlayer(
		APlayerController* NewPlayerController,
		const FUniqueNetIdRepl& UniqueId,
		const FString& Options,
		const FString& Portal) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	UFUNCTION(BlueprintCallable, Category="Frontier|Extraction")
	void HandlePlayerExtraction(AController* ExtractingController);

	UFUNCTION(BlueprintCallable, Category="Frontier|Death")
	void HandlePlayerDeathReturnToLobby(AController* DeadController);
	void HandleSpectatorEndRequested(AFrontierPlayerController* FrontierPlayerController);
	void HandleBackendSettlementReady(
		AFrontierPlayerController* FrontierPlayerController,
		const FString& RaidSessionId,
		EFrontierRaidOutcome Outcome,
		int64 AwardedExperience);

	UFUNCTION(BlueprintCallable, Category="Frontier|Spawn")
	bool TryRelocateControllerToRaidSpawn(AFrontierPlayerController* FrontierPlayerController);
	void NotifyRaidControllerPrepared(AFrontierPlayerController* FrontierPlayerController);
	void NotifyRaidClientGameplayReady(AFrontierPlayerController* FrontierPlayerController);
	void HandleRaidJoinAuthorizationFailed(
		AFrontierPlayerController* FrontierPlayerController,
		const FString& Error);

	UFUNCTION(BlueprintPure, Category="Frontier|Extraction")
	FString GetLobbyLevelPackageName() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Travel")
	FString GetResolvedLobbyTravelDestination() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Travel")
	FString GetResolvedRaidTravelDestination(const TSoftObjectPtr<UWorld>& RaidLevel) const;

	UFUNCTION(BlueprintPure, Category="Frontier|Lobby")
	bool IsCurrentWorldLobbyLevel() const;

	/** Runtime state exposed to the dedicated-server status endpoint. */
	FString GetRaidStatusState() const;

private:
	bool IsEditorRaidPlaySession() const;
	void AssignTemporaryTeamByJoinOrder(APlayerController* NewPlayer);
	void StartRaidCountdown();
	void UpdateRaidCountdown();
	void HandleRaidTimerExpired();
	void FailActiveRaidPlayer(AFrontierPlayerController* FrontierPlayerController);
	void ResetRaidPlayerLobbyState() const;
	void TravelControllerToLobby(AFrontierPlayerController* FrontierPlayerController, const TCHAR* FailureContext);
	void TravelControllerToDestination(AFrontierPlayerController* FrontierPlayerController, const FString& Destination, const TCHAR* FailureContext) const;
	FString ResolveTravelDestination(EFrontierTravelMode TravelMode, const FString& ServerAddress, const TSoftObjectPtr<UWorld>& LevelAsset) const;
	void ExecuteDelayedLobbyReturn(TWeakObjectPtr<AFrontierPlayerController> FrontierPlayerController);
	void TransitionControllerToRaidSpectator(AFrontierPlayerController* FrontierPlayerController, bool bRemoveObservedPawnFromWorld, const TCHAR* Context);
	AActor* FindSpectatorViewTarget(const AFrontierPlayerController* ObservingController) const;
	int32 GetActiveRaidParticipantCount() const;
	bool HasAnyActiveRaidParticipants() const;
	void ScheduleRaidEndTravelIfNeeded();
	void ReturnAllRaidPlayersToLobby();
	AFrontierTeamPlayerStart* ResolveRaidSpawnForTeam(int32 TeamId);
	bool AreAllRaidControllersPrepared() const;
	void ReleaseRaidStartGate();
	void FinalizePlayerDeath(TWeakObjectPtr<AFrontierPlayerController> FrontierPlayerController);
	bool StartPlayerSettlement(AFrontierPlayerController* FrontierPlayerController, EFrontierRaidOutcome Outcome);
	bool HasLivingTeammate(const AFrontierPlayerController* FrontierPlayerController) const;
	void SettleSpectatorsWithoutLivingTeammates();
	void ScheduleDedicatedServerShutdownIfEmpty();
	void ShutdownDedicatedServerIfEmpty();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Extraction")
	TSoftObjectPtr<UWorld> LobbyLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Travel")
	EFrontierTravelMode LobbyTravelMode = EFrontierTravelMode::MapPackage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Travel", meta=(EditCondition="LobbyTravelMode==EFrontierTravelMode::ServerAddress", EditConditionHides))
	FString LobbyServerAddress;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Death", meta=(ClampMin="0.0"))
	float DeathReturnDelaySeconds = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Raid", meta=(ClampMin="1"))
	int32 RaidTimeLimitSeconds = 300;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Dedicated Server", meta=(ClampMin="0.1"))
	float EmptyServerShutdownDelaySeconds = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Team", meta=(ClampMin="1"))
	int32 TemporaryTeamMaxPlayers = 1;

	bool bRaidEndTravelTriggered = false;
	double RaidEndTimeSeconds = 0.0;
	FTimerHandle RaidEndTravelTimerHandle;
	FTimerHandle RaidCountdownTimerHandle;
	FTimerHandle EmptyServerShutdownTimerHandle;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<AFrontierTeamPlayerStart>> AssignedRaidSpawnByTeam;

	int32 NextTemporaryTeamId = 1;
	int32 PlayersAssignedToCurrentTemporaryTeam = 0;

	UPROPERTY(Transient)
	TSet<TObjectPtr<AFrontierPlayerController>> PreparedRaidControllers;

	UPROPERTY(Transient)
	TSet<TObjectPtr<AFrontierPlayerController>> ClientGameplayReadyControllers;

	bool bRaidGameplayStarted = false;
	bool bHadConnectedRaidPlayer = false;
	bool bDedicatedServerShutdownRequested = false;
	TSet<TWeakObjectPtr<APlayerController>> ConnectedRaidControllers;
	TSet<TWeakObjectPtr<AFrontierPlayerController>> PendingDeathFinalizations;
	TSet<TWeakObjectPtr<AFrontierPlayerController>> PendingBackendSettlements;
};


