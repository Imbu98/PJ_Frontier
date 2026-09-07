#include "Game/FrontierGameMode.h"

#include "Character/FrontierPlayerCharacter.h"
#include "FrontierPlayerController.h"
#include "Frontier.h"
#include "Game/FrontierGameState.h"
#include "Game/FrontierPlayerState.h"
#include "Game/FrontierTeamPlayerStart.h"
#include "EngineUtils.h"
#include "FrontierSteamSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Tags/FrontierGameplayTags.h"
#include "TimerManager.h"
#include "Progression/FrontierRaidExperienceSubsystem.h"
#include "Progression/FrontierInternalApiConfig.h"
#include "Progression/FrontierRaidSessionSubsystem.h"
#include "Progression/FrontierRaidLootPoolSubsystem.h"

namespace
{
constexpr TCHAR MainLobbyLevelPackage[] = TEXT("/Game/Levels/MainLobby/L_MainLobbyLevel");
}

AFrontierGameMode::AFrontierGameMode()
{
	

	DefaultPawnClass = AFrontierPlayerCharacter::StaticClass();
	GameStateClass = AFrontierGameState::StaticClass();
	PlayerStateClass = AFrontierPlayerState::StaticClass();
	// Map-package raid travel must preserve the server-verified Backend identity in PlayerState::CopyProperties.
	bUseSeamlessTravel = true;
	FFrontierGameplayTags::InitializeNativeGameplayTags();
}

void AFrontierGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	const bool bIsLobbyLevel = IsCurrentWorldLobbyLevel();
	FRONTIER_LOG(
		Log,
		TEXT("[RaidStartup] GameMode BeginPlay. World=%s NetMode=%d Dedicated=%d Lobby=%d"),
		World ? *World->GetOutermost()->GetName() : TEXT("<null>"),
		static_cast<int32>(GetNetMode()),
		GetNetMode() == NM_DedicatedServer ? 1 : 0,
		bIsLobbyLevel ? 1 : 0);

	if (!bIsLobbyLevel)
	{
		if (AFrontierGameState* FrontierGameState = GetGameState<AFrontierGameState>())
		{
			FrontierGameState->SetRaidTimerState(false, 0);
		}
		if (GetNetMode() != NM_DedicatedServer)
		{
			FRONTIER_LOG(Log, TEXT("[RaidStartup] Skipping dedicated-server loot initialization because this world is not a dedicated server."));
		}
		else
		{
			// Defer one frame so level actors and initially spawned enemies have registered
			// their LootComponents before the unique table request is frozen.
			FRONTIER_LOG(Log, TEXT("[RaidStartup] Scheduling dedicated-server loot initialization for the next tick."));
			GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				UWorld* CallbackWorld = GetWorld();
				FRONTIER_LOG(
					Log,
					TEXT("[RaidStartup] Deferred loot initialization callback entered. WorldValid=%d"),
					CallbackWorld ? 1 : 0);
				if (!CallbackWorld)
				{
					FRONTIER_LOG(Error, TEXT("[RaidStartup] Cannot start loot initialization because the world is no longer valid."));
					return;
				}

				UFrontierRaidLootPoolSubsystem* LootPool = CallbackWorld->GetSubsystem<UFrontierRaidLootPoolSubsystem>();
				if (!LootPool)
				{
					FRONTIER_LOG(Error, TEXT("[RaidStartup] Cannot start loot initialization because FrontierRaidLootPoolSubsystem is unavailable."));
					return;
				}

				FRONTIER_LOG(Log, TEXT("[RaidStartup] Starting FrontierRaidLootPoolSubsystem initialization."));
				LootPool->StartDedicatedServerInitialization();
			}));
		}
	}
	else
	{
		FRONTIER_LOG(Log, TEXT("[RaidStartup] Lobby world detected; dedicated-server raid initialization is not required."));
	}
	
}

void AFrontierGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (GetNetMode() == NM_DedicatedServer && !IsCurrentWorldLobbyLevel() && NewPlayer)
	{
		bHadConnectedRaidPlayer = true;
		ConnectedRaidControllers.Add(NewPlayer);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(EmptyServerShutdownTimerHandle);
		}
	}

	if (bRaidGameplayStarted && GetWorld())
	{
		if (UFrontierRaidExperienceSubsystem* RaidExperience = GetWorld()->GetSubsystem<UFrontierRaidExperienceSubsystem>())
		{
			RaidExperience->RegisterPlayer(NewPlayer ? NewPlayer->GetPlayerState<AFrontierPlayerState>() : nullptr);
		}
	}
}

void AFrontierGameMode::Logout(AController* Exiting)
{
	AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(Exiting);
	FRONTIER_LOG(
		Log,
		TEXT("[RaidJoinAuthorization] Player logout. Controller=%s ConnectedRaidControllersBefore=%d"),
		*GetNameSafe(Exiting),
		ConnectedRaidControllers.Num());
	ConnectedRaidControllers.Remove(Cast<APlayerController>(Exiting));
	PreparedRaidControllers.Remove(FrontierPlayerController);
	ClientGameplayReadyControllers.Remove(FrontierPlayerController);
	PendingDeathFinalizations.Remove(FrontierPlayerController);
	PendingBackendSettlements.Remove(FrontierPlayerController);

	if (UFrontierRaidSessionSubsystem* RaidSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr)
	{
		RaidSession->RemoveServerRaidContext(FrontierPlayerController);
	}

	Super::Logout(Exiting);
	ScheduleDedicatedServerShutdownIfEmpty();
}

void AFrontierGameMode::ScheduleDedicatedServerShutdownIfEmpty()
{
	UWorld* World = GetWorld();
	if (!World
		|| GetNetMode() != NM_DedicatedServer
		|| IsCurrentWorldLobbyLevel()
		|| !bHadConnectedRaidPlayer
		|| bDedicatedServerShutdownRequested)
	{
		return;
	}

	for (auto Iterator = ConnectedRaidControllers.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Iterator->IsValid())
		{
			Iterator.RemoveCurrent();
		}
	}

	if (!ConnectedRaidControllers.IsEmpty()
		|| World->GetTimerManager().IsTimerActive(EmptyServerShutdownTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		EmptyServerShutdownTimerHandle,
		this,
		&AFrontierGameMode::ShutdownDedicatedServerIfEmpty,
		FMath::Max(0.1f, EmptyServerShutdownDelaySeconds),
		false);

	FRONTIER_LOG(
		Log,
		TEXT("[DedicatedServer] Shutdown scheduled because no connected raid players remain. DelaySeconds=%.2f"),
		EmptyServerShutdownDelaySeconds);
}

void AFrontierGameMode::ShutdownDedicatedServerIfEmpty()
{
	if (GetNetMode() != NM_DedicatedServer
		|| IsCurrentWorldLobbyLevel()
		|| bDedicatedServerShutdownRequested)
	{
		return;
	}

	for (auto Iterator = ConnectedRaidControllers.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Iterator->IsValid())
		{
			Iterator.RemoveCurrent();
		}
	}

	if (!ConnectedRaidControllers.IsEmpty())
	{
		return;
	}

	bDedicatedServerShutdownRequested = true;
	FRONTIER_LOG(Log, TEXT("[DedicatedServer] No connected raid players remain. Requesting process exit."));
	FPlatformMisc::RequestExit(false);
}

FString AFrontierGameMode::InitNewPlayer(
	APlayerController* NewPlayerController,
	const FUniqueNetIdRepl& UniqueId,
	const FString& Options,
	const FString& Portal)
{
	const FString SuperError = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (!SuperError.IsEmpty() || IsCurrentWorldLobbyLevel())
	{
		return SuperError;
	}

	if (IsEditorRaidPlaySession())
	{
		FRONTIER_LOG(
			Log,
			TEXT("[RaidPIE] Skipping backend join authorization for direct raid-map PIE. Controller=%s"),
			*GetNameSafe(NewPlayerController));
		return FString();
	}

	const FString RaidSessionId = UGameplayStatics::ParseOption(Options, TEXT("RaidSessionId"));
	const FString JoinToken = UGameplayStatics::ParseOption(Options, TEXT("JoinToken"));
	TArray<FString> JoinTokenSegments;
	JoinToken.ParseIntoArray(JoinTokenSegments, TEXT("."), false);
	FRONTIER_LOG(
		Log,
		TEXT("[RaidJoinAuthorization] Login options parsed. Controller=%s HasRaidSessionId=%d RaidSessionId=%s JoinTokenLength=%d JoinTokenSegments=%d"),
		*GetNameSafe(NewPlayerController),
		RaidSessionId.IsEmpty() ? 0 : 1,
		RaidSessionId.IsEmpty() ? TEXT("<empty>") : *RaidSessionId,
		JoinToken.Len(),
		JoinTokenSegments.Num());
	if (RaidSessionId.IsEmpty() || JoinToken.IsEmpty())
	{
		FRONTIER_LOG(Error, TEXT("[RaidJoinAuthorization] Login rejected before authorization because RaidSessionId or JoinToken is missing."));
		return TEXT("RaidSessionId and JoinToken are required.");
	}

	AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(NewPlayerController);
	UFrontierRaidSessionSubsystem* RaidSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr;
	if (!FrontierPlayerController || !RaidSession)
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidJoinAuthorization] Login rejected because required authorization objects are unavailable. PlayerController=%d RaidSessionSubsystem=%d"),
			FrontierPlayerController ? 1 : 0,
			RaidSession ? 1 : 0);
		return TEXT("Raid authorization service is unavailable.");
	}

	FFrontierOnlineRaidEntryDTO Entry;
	Entry.RaidSession.RaidSessionId = RaidSessionId;
	Entry.RaidSession.bHasServerId = true;
	Entry.RaidSession.ServerId = FFrontierInternalApiConfig::Load().RaidServerId;
	Entry.JoinToken = JoinToken;

	FString AuthorizationError;
	if (!RaidSession->BeginServerJoinAuthorization(FrontierPlayerController, Entry, AuthorizationError))
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidJoinAuthorization] Authorization request could not start. RaidSessionId=%s Error=%s"),
			*RaidSessionId,
			AuthorizationError.IsEmpty() ? TEXT("<empty>") : *AuthorizationError);
		return AuthorizationError.IsEmpty()
			? TEXT("Raid join authorization could not start.")
			: AuthorizationError;
	}

	return FString();
}

void AFrontierGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (IsCurrentWorldLobbyLevel())
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
		return;
	}

	if (IsEditorRaidPlaySession())
	{
		AssignTemporaryTeamByJoinOrder(NewPlayer);
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);

		if (AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(NewPlayer))
		{
			if (FrontierPlayerController->GetPawn())
			{
				PreparedRaidControllers.Add(FrontierPlayerController);
			}
		}

		if (AreAllRaidControllersPrepared())
		{
			ReleaseRaidStartGate();
		}
		return;
	}

	UFrontierRaidSessionSubsystem* RaidSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr;
	if (RaidSession && RaidSession->IsServerRaidActive(NewPlayer))
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
		return;
	}

	FRONTIER_LOG(
		Log,
		TEXT("[RaidJoinAuthorization] Pawn spawn deferred until backend authorization completes. Controller=%s"),
		*GetNameSafe(NewPlayer));
}

void AFrontierGameMode::AssignTemporaryTeamByJoinOrder(APlayerController* NewPlayer)
{
	AFrontierPlayerState* FrontierPlayerState =
		NewPlayer ? NewPlayer->GetPlayerState<AFrontierPlayerState>() : nullptr;
	if (!FrontierPlayerState || !HasAuthority())
	{
		return;
	}

	const int32 AssignedTeamId = NextTemporaryTeamId;
	FrontierPlayerState->SetTeamId(AssignedTeamId);

	++PlayersAssignedToCurrentTemporaryTeam;
	if (PlayersAssignedToCurrentTemporaryTeam >= FMath::Max(1, TemporaryTeamMaxPlayers))
	{
		PlayersAssignedToCurrentTemporaryTeam = 0;
		++NextTemporaryTeamId;
	}

	FRONTIER_LOG(
		Log,
		TEXT("Assigned temporary raid team by join order. Player=%s TeamId=%d MaxPlayersPerTeam=%d"),
		*GetNameSafe(NewPlayer),
		AssignedTeamId,
		TemporaryTeamMaxPlayers);
}

AActor* AFrontierGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (!Player || IsCurrentWorldLobbyLevel())
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

#if 0
	// Temporarily disabled until the lobby/backend provides authoritative party teams.
	const AFrontierPlayerState* FrontierPlayerState = Player->GetPlayerState<AFrontierPlayerState>();
	const int32 DesiredTeamId = FrontierPlayerState ? FrontierPlayerState->GetTeamId() : 0;
	if (DesiredTeamId <= 0)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	if (AFrontierTeamPlayerStart* AssignedStart = ResolveRaidSpawnForTeam(DesiredTeamId))
	{
		return AssignedStart;
	}
#endif

	return Super::ChoosePlayerStart_Implementation(Player);
}

AFrontierTeamPlayerStart* AFrontierGameMode::ResolveRaidSpawnForTeam(const int32 TeamId)
{
#if 0
	// Temporarily disabled. Players currently use normal PlayerStart selection.
	if (TeamId <= 0 || !GetWorld())
	{
		return nullptr;
	}

	if (TObjectPtr<AFrontierTeamPlayerStart>* ExistingStart = AssignedRaidSpawnByTeam.Find(TeamId))
	{
		if (*ExistingStart)
		{
			return ExistingStart->Get();
		}
	}

	TArray<AFrontierTeamPlayerStart*> AllStarts;
	TArray<AFrontierTeamPlayerStart*> UnusedStarts;
	for (TActorIterator<AFrontierTeamPlayerStart> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		AFrontierTeamPlayerStart* Candidate = *Iterator;
		if (!Candidate)
		{
			continue;
		}
		AllStarts.Add(Candidate);
		if (!AssignedRaidSpawnByTeam.FindKey(Candidate))
		{
			UnusedStarts.Add(Candidate);
		}
	}

	TArray<AFrontierTeamPlayerStart*>& CandidatePool = UnusedStarts.Num() > 0 ? UnusedStarts : AllStarts;
	if (CandidatePool.Num() <= 0)
	{
		return nullptr;
	}

	const int32 RandomIndex = FMath::RandRange(0, CandidatePool.Num() - 1);
	AFrontierTeamPlayerStart* ChosenStart = CandidatePool[RandomIndex];
	AssignedRaidSpawnByTeam.FindOrAdd(TeamId) = ChosenStart;
	return ChosenStart;
#else
	return nullptr;
#endif
}

bool AFrontierGameMode::TryRelocateControllerToRaidSpawn(AFrontierPlayerController* FrontierPlayerController)
{
#if 0
	// Temporarily disabled with ResolveRaidSpawnForTeam.
	if (!FrontierPlayerController || IsCurrentWorldLobbyLevel())
	{
		return false;
	}

	AFrontierPlayerState* FrontierPlayerState = FrontierPlayerController->GetPlayerState<AFrontierPlayerState>();
	APawn* ControlledPawn = FrontierPlayerController->GetPawn();
	if (!FrontierPlayerState || FrontierPlayerState->GetTeamId() <= 0 || !ControlledPawn)
	{
		return false;
	}

	AFrontierTeamPlayerStart* TeamStart = ResolveRaidSpawnForTeam(FrontierPlayerState->GetTeamId());
	if (!TeamStart)
	{
		return false;
	}

	if (ACharacter* Character = Cast<ACharacter>(ControlledPawn))
	{
		if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
	}

	const FVector TargetLocation = TeamStart->GetActorLocation();
	FRotator TargetRotation = TeamStart->GetActorRotation();
	TargetRotation.Pitch = 0.0f;
	TargetRotation.Roll = 0.0f;
	TargetRotation.Normalize();
	ControlledPawn->TeleportTo(TargetLocation, TargetRotation, false, true);
	FrontierPlayerController->ClientSetRotation(TargetRotation, true);
	FrontierPlayerController->SetControlRotation(TargetRotation);

	return true;
#else
	return false;
#endif
}

void AFrontierGameMode::NotifyRaidControllerPrepared(AFrontierPlayerController* FrontierPlayerController)
{
	if (!FrontierPlayerController || IsCurrentWorldLobbyLevel())
	{
		return;
	}

	if (UFrontierRaidSessionSubsystem* RaidSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr;
		!RaidSession || !RaidSession->IsServerRaidActive(FrontierPlayerController))
	{
		return;
	}

	AFrontierPlayerState* FrontierPlayerState = FrontierPlayerController->GetPlayerState<AFrontierPlayerState>();
	if (!FrontierPlayerState)
	{
		return;
	}

	if (FrontierPlayerState->GetTeamId() <= 0)
	{
		AssignTemporaryTeamByJoinOrder(FrontierPlayerController);
	}
	if (FrontierPlayerState->GetTeamId() <= 0)
	{
		HandleRaidJoinAuthorizationFailed(
			FrontierPlayerController,
			TEXT("레이드 팀을 배정하지 못했습니다."));
		return;
	}

	if (!FrontierPlayerController->GetPawn())
	{
		RestartPlayer(FrontierPlayerController);
	}
	if (!FrontierPlayerController->GetPawn())
	{
		HandleRaidJoinAuthorizationFailed(
			FrontierPlayerController,
			TEXT("승인된 플레이어 캐릭터를 생성하지 못했습니다."));
		return;
	}

	PreparedRaidControllers.Add(FrontierPlayerController);
	if (bRaidGameplayStarted)
	{
		if (ClientGameplayReadyControllers.Contains(FrontierPlayerController))
		{
			if (FrontierPlayerController->IsLocalController())
			{
				FrontierPlayerController->ClientSetRaidLoadingScreen_Implementation(false);
			}
			FrontierPlayerController->ClientSetRaidLoadingScreen(false);
		}
		return;
	}

	if (AreAllRaidControllersPrepared())
	{
		ReleaseRaidStartGate();
	}
}

void AFrontierGameMode::NotifyRaidClientGameplayReady(AFrontierPlayerController* FrontierPlayerController)
{
	if (!FrontierPlayerController || IsCurrentWorldLobbyLevel())
	{
		return;
	}

	ClientGameplayReadyControllers.Add(FrontierPlayerController);
	FRONTIER_LOG(
		Log,
		TEXT("Raid client gameplay ready confirmed. Controller=%s ServerPrepared=%d RaidStarted=%d"),
		*GetNameSafe(FrontierPlayerController),
		PreparedRaidControllers.Contains(FrontierPlayerController) ? 1 : 0,
		bRaidGameplayStarted ? 1 : 0);

	if (bRaidGameplayStarted)
	{
		if (PreparedRaidControllers.Contains(FrontierPlayerController))
		{
			if (FrontierPlayerController->IsLocalController())
			{
				FrontierPlayerController->ClientSetRaidLoadingScreen_Implementation(false);
			}
			FrontierPlayerController->ClientSetRaidLoadingScreen(false);
		}
		return;
	}

	if (AreAllRaidControllersPrepared())
	{
		ReleaseRaidStartGate();
	}
}

void AFrontierGameMode::HandleRaidJoinAuthorizationFailed(
	AFrontierPlayerController* FrontierPlayerController,
	const FString& Error)
{
	if (!FrontierPlayerController)
	{
		FRONTIER_LOG(Error, TEXT("[RaidJoinAuthorization] Failure handling skipped because the controller is invalid. Error=%s"), *Error);
		return;
	}

	FRONTIER_LOG(
		Error,
		TEXT("[RaidJoinAuthorization] Join authorization failed; notifying client and returning to lobby. Controller=%s Error=%s"),
		*GetNameSafe(FrontierPlayerController),
		Error.IsEmpty() ? TEXT("<empty>") : *Error);

	PreparedRaidControllers.Remove(FrontierPlayerController);
	ClientGameplayReadyControllers.Remove(FrontierPlayerController);
	FrontierPlayerController->ClientReceiveRaidFlowFailed(
		Error.IsEmpty() ? TEXT("레이드 입장 승인이 거부되었습니다.") : Error,
		false);
	TravelControllerToLobby(
		FrontierPlayerController,
		TEXT("Raid join authorization failed"));
}

void AFrontierGameMode::HandlePlayerExtraction(AController* ExtractingController)
{
	AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(ExtractingController);
	if (!FrontierPlayerController)
	{
		return;
	}

	if (APawn* ExtractingPawn = FrontierPlayerController->GetPawn())
	{
		ExtractingPawn->DisableInput(FrontierPlayerController);
		ExtractingPawn->SetActorEnableCollision(false);
	}

	TransitionControllerToRaidSpectator(FrontierPlayerController, true, TEXT("Extraction succeeded; settlement pending"));
	StartPlayerSettlement(FrontierPlayerController, EFrontierRaidOutcome::Extracted);
}

void AFrontierGameMode::HandlePlayerDeathReturnToLobby(AController* DeadController)
{
	AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(DeadController);
	if (!FrontierPlayerController)
	{
		return;
	}
	if (PendingDeathFinalizations.Contains(FrontierPlayerController))
	{
		return;
	}

	if (APawn* DeadPawn = FrontierPlayerController->GetPawn())
	{
		DeadPawn->DisableInput(FrontierPlayerController);
	}

	// Damage application triggers the character death callback synchronously. Defer result
	// finalization one tick so the same damage event can first record killer and assists.
	PendingDeathFinalizations.Add(FrontierPlayerController);
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(
		this,
		&AFrontierGameMode::FinalizePlayerDeath,
		TWeakObjectPtr<AFrontierPlayerController>(FrontierPlayerController)));
}

void AFrontierGameMode::FinalizePlayerDeath(TWeakObjectPtr<AFrontierPlayerController> FrontierPlayerController)
{
	PendingDeathFinalizations.Remove(FrontierPlayerController);
	if (!FrontierPlayerController.IsValid())
	{
		return;
	}
	TransitionControllerToRaidSpectator(FrontierPlayerController.Get(), false, TEXT("Player death spectate"));
	SettleSpectatorsWithoutLivingTeammates();
}

void AFrontierGameMode::HandleSpectatorEndRequested(AFrontierPlayerController* FrontierPlayerController)
{
	const AFrontierPlayerState* FrontierPlayerState = FrontierPlayerController
		? FrontierPlayerController->GetPlayerState<AFrontierPlayerState>()
		: nullptr;
	if (!FrontierPlayerController || !FrontierPlayerState || !FrontierPlayerState->IsOnlyASpectator())
	{
		return;
	}

	StartPlayerSettlement(FrontierPlayerController, EFrontierRaidOutcome::Dead);
}

bool AFrontierGameMode::StartPlayerSettlement(
	AFrontierPlayerController* FrontierPlayerController,
	const EFrontierRaidOutcome Outcome)
{
	if (!FrontierPlayerController || PendingBackendSettlements.Contains(FrontierPlayerController))
	{
		return false;
	}

	if (!bRaidGameplayStarted || !PreparedRaidControllers.Contains(FrontierPlayerController))
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Ignored premature raid settlement. Controller=%s Outcome=%d RaidStarted=%d Prepared=%d"),
			*GetNameSafe(FrontierPlayerController),
			static_cast<int32>(Outcome),
			bRaidGameplayStarted ? 1 : 0,
			PreparedRaidControllers.Contains(FrontierPlayerController) ? 1 : 0);
		return false;
	}

	if (IsEditorRaidPlaySession())
	{
		FRONTIER_LOG(
			Log,
			TEXT("[RaidPIE] Skipping backend settlement. Controller=%s Outcome=%d"),
			*GetNameSafe(FrontierPlayerController),
			static_cast<int32>(Outcome));
		TravelControllerToLobby(FrontierPlayerController, TEXT("PIE raid settlement"));
		return true;
	}

	UFrontierRaidSessionSubsystem* RaidSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr;
	FString Error;
	if (!RaidSession || !RaidSession->BeginServerSettlement(FrontierPlayerController, Outcome, Error))
	{
		FrontierPlayerController->ClientReceiveRaidFlowFailed(
			Error.IsEmpty() ? TEXT("레이드 정산을 시작하지 못했습니다.") : Error,
			false);
		return false;
	}

	PendingBackendSettlements.Add(FrontierPlayerController);
	return true;
}

void AFrontierGameMode::HandleBackendSettlementReady(
	AFrontierPlayerController* FrontierPlayerController,
	const FString& RaidSessionId,
	const EFrontierRaidOutcome Outcome,
	const int64 AwardedExperience)
{
	if (!FrontierPlayerController || !PendingBackendSettlements.Remove(FrontierPlayerController))
	{
		return;
	}

	const FString Destination = GetResolvedLobbyTravelDestination();
	if (Destination.IsEmpty())
	{
		FrontierPlayerController->ClientReceiveRaidFlowFailed(
			TEXT("백엔드 정산은 완료됐지만 로비 이동 주소가 설정되지 않았습니다."),
			false);
		return;
	}

	FrontierPlayerController->ClientCompleteRaidSettlement(
		RaidSessionId,
		Outcome,
		AwardedExperience,
		Destination);
}

bool AFrontierGameMode::HasLivingTeammate(const AFrontierPlayerController* FrontierPlayerController) const
{
	const UWorld* World = GetWorld();
	const AFrontierPlayerState* PlayerState = FrontierPlayerController
		? FrontierPlayerController->GetPlayerState<AFrontierPlayerState>()
		: nullptr;
	if (!World || !PlayerState || PlayerState->GetTeamId() <= 0)
	{
		return false;
	}

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const AFrontierPlayerController* CandidateController = Cast<AFrontierPlayerController>(Iterator->Get());
		const AFrontierPlayerState* CandidateState = CandidateController
			? CandidateController->GetPlayerState<AFrontierPlayerState>()
			: nullptr;
		const AFrontierPlayerCharacter* CandidateCharacter = CandidateController
			? Cast<AFrontierPlayerCharacter>(CandidateController->GetPawn())
			: nullptr;
		if (CandidateController
			&& CandidateController != FrontierPlayerController
			&& CandidateState
			&& CandidateState->GetTeamId() == PlayerState->GetTeamId()
			&& !CandidateState->IsOnlyASpectator()
			&& CandidateCharacter
			&& !CandidateCharacter->IsDead())
		{
			return true;
		}
	}

	return false;
}

void AFrontierGameMode::SettleSpectatorsWithoutLivingTeammates()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AFrontierPlayerController*> ControllersToSettle;
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(Iterator->Get());
		const AFrontierPlayerState* FrontierPlayerState = FrontierPlayerController
			? FrontierPlayerController->GetPlayerState<AFrontierPlayerState>()
			: nullptr;
		if (FrontierPlayerController
			&& FrontierPlayerState
			&& FrontierPlayerState->IsOnlyASpectator()
			&& !PendingBackendSettlements.Contains(FrontierPlayerController)
			&& !HasLivingTeammate(FrontierPlayerController))
		{
			ControllersToSettle.Add(FrontierPlayerController);
		}
	}

	for (AFrontierPlayerController* FrontierPlayerController : ControllersToSettle)
	{
		StartPlayerSettlement(FrontierPlayerController, EFrontierRaidOutcome::Dead);
	}
}

bool AFrontierGameMode::AreAllRaidControllersPrepared() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	int32 RequiredControllerCount = 0;
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(Iterator->Get());
		const AFrontierPlayerState* FrontierPlayerState = FrontierPlayerController ? FrontierPlayerController->GetPlayerState<AFrontierPlayerState>() : nullptr;
		if (!FrontierPlayerController || !FrontierPlayerState || FrontierPlayerState->IsOnlyASpectator())
		{
			continue;
		}

		++RequiredControllerCount;
		AFrontierPlayerController* MutableController =
			const_cast<AFrontierPlayerController*>(FrontierPlayerController);
		if (!PreparedRaidControllers.Contains(MutableController)
			|| !ClientGameplayReadyControllers.Contains(MutableController))
		{
			return false;
		}
	}

	return RequiredControllerCount > 0;
}

void AFrontierGameMode::ReleaseRaidStartGate()
{
	if (bRaidGameplayStarted)
	{
		return;
	}

	bRaidGameplayStarted = true;
	if (UFrontierRaidExperienceSubsystem* RaidExperience = GetWorld()->GetSubsystem<UFrontierRaidExperienceSubsystem>())
	{
		RaidExperience->BeginRaid();
	}
	StartRaidCountdown();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(Iterator->Get());
		AFrontierPlayerState* FrontierPlayerState = FrontierPlayerController ? FrontierPlayerController->GetPlayerState<AFrontierPlayerState>() : nullptr;
		if (!FrontierPlayerController || !FrontierPlayerState || FrontierPlayerState->IsOnlyASpectator())
		{
			continue;
		}
		if (UFrontierRaidExperienceSubsystem* RaidExperience = World->GetSubsystem<UFrontierRaidExperienceSubsystem>())
		{
			RaidExperience->RegisterPlayer(FrontierPlayerState);
		}

		if (FrontierPlayerController->IsLocalController())
		{
			FrontierPlayerController->ClientSetRaidLoadingScreen_Implementation(false);
		}
		FrontierPlayerController->ClientSetRaidLoadingScreen(false);
	}

}

FString AFrontierGameMode::GetLobbyLevelPackageName() const
{
	return MainLobbyLevelPackage;
}

FString AFrontierGameMode::GetResolvedLobbyTravelDestination() const
{
	return LobbyTravelMode == EFrontierTravelMode::ServerAddress
		? LobbyServerAddress
		: GetLobbyLevelPackageName();
}

FString AFrontierGameMode::GetResolvedRaidTravelDestination(const TSoftObjectPtr<UWorld>& RaidLevel) const
{
	return ResolveTravelDestination(EFrontierTravelMode::MapPackage, FString(), RaidLevel);
}

bool AFrontierGameMode::IsCurrentWorldLobbyLevel() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FString LobbyLevelPackageName = GetLobbyLevelPackageName();
	if (LobbyLevelPackageName.IsEmpty())
	{
		return false;
	}

	const FString CurrentWorldPackageName = World->GetOutermost()->GetName();
	return CurrentWorldPackageName.Equals(LobbyLevelPackageName, ESearchCase::CaseSensitive)
		|| CurrentWorldPackageName.EndsWith(LobbyLevelPackageName, ESearchCase::CaseSensitive);
}

FString AFrontierGameMode::GetRaidStatusState() const
{
	if (IsCurrentWorldLobbyLevel())
	{
		return TEXT("WAITING");
	}

	if (bRaidEndTravelTriggered || GetWorldTimerManager().IsTimerActive(RaidEndTravelTimerHandle))
	{
		return TEXT("ENDING");
	}

	return bRaidGameplayStarted ? TEXT("IN_PROGRESS") : TEXT("WAITING");
}

bool AFrontierGameMode::IsEditorRaidPlaySession() const
{
#if WITH_EDITOR
	const UWorld* World = GetWorld();
	return World && World->WorldType == EWorldType::PIE;
#else
	return false;
#endif
}

void AFrontierGameMode::StartRaidCountdown()
{
	UWorld* World = GetWorld();
	AFrontierGameState* FrontierGameState = GetGameState<AFrontierGameState>();
	if (!World || !FrontierGameState || RaidTimeLimitSeconds <= 0)
	{
		return;
	}

	RaidEndTimeSeconds = World->GetTimeSeconds() + static_cast<double>(RaidTimeLimitSeconds);
	FrontierGameState->SetRaidTimerState(true, RaidTimeLimitSeconds);
	World->GetTimerManager().SetTimer(RaidCountdownTimerHandle, this, &AFrontierGameMode::UpdateRaidCountdown, 1.0f, true);

	FRONTIER_LOG(Log, TEXT("Raid countdown started. LimitSeconds=%d"), RaidTimeLimitSeconds);
}

void AFrontierGameMode::UpdateRaidCountdown()
{
	UWorld* World = GetWorld();
	AFrontierGameState* FrontierGameState = GetGameState<AFrontierGameState>();
	if (!World || !FrontierGameState || bRaidEndTravelTriggered)
	{
		return;
	}

	const int32 RemainingTimeSeconds = FMath::Max(0, FMath::CeilToInt(RaidEndTimeSeconds - World->GetTimeSeconds()));
	FrontierGameState->SetRaidTimerState(true, RemainingTimeSeconds);

	if (RemainingTimeSeconds <= 0)
	{
		HandleRaidTimerExpired();
	}
}

void AFrontierGameMode::HandleRaidTimerExpired()
{
	if (bRaidEndTravelTriggered)
	{
		return;
	}

	UWorld* World = GetWorld();
	AFrontierGameState* FrontierGameState = GetGameState<AFrontierGameState>();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(RaidCountdownTimerHandle);
	if (FrontierGameState)
	{
		FrontierGameState->SetRaidTimerState(false, 0);
	}

	TArray<AFrontierPlayerController*> ControllersToFail;
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(Iterator->Get());
		const AFrontierPlayerState* FrontierPlayerState = FrontierPlayerController ? FrontierPlayerController->GetPlayerState<AFrontierPlayerState>() : nullptr;
		const AFrontierPlayerCharacter* FrontierCharacter = FrontierPlayerController ? Cast<AFrontierPlayerCharacter>(FrontierPlayerController->GetPawn()) : nullptr;
		if (!FrontierPlayerController || !FrontierPlayerState || FrontierPlayerState->IsOnlyASpectator() || !FrontierCharacter || FrontierCharacter->IsDead())
		{
			continue;
		}

		ControllersToFail.Add(FrontierPlayerController);
	}

	for (AFrontierPlayerController* FrontierPlayerController : ControllersToFail)
	{
		FailActiveRaidPlayer(FrontierPlayerController);
	}

}

void AFrontierGameMode::FailActiveRaidPlayer(AFrontierPlayerController* FrontierPlayerController)
{
	AFrontierPlayerState* FrontierPlayerState = FrontierPlayerController ? FrontierPlayerController->GetPlayerState<AFrontierPlayerState>() : nullptr;
	if (!FrontierPlayerController || !FrontierPlayerState)
	{
		return;
	}

	FrontierPlayerState->LoseRaidItemsOnDeath();

	if (APawn* ControlledPawn = FrontierPlayerController->GetPawn())
	{
		ControlledPawn->DisableInput(FrontierPlayerController);
	}

	TransitionControllerToRaidSpectator(FrontierPlayerController, true, TEXT("Raid timeout failure"));
	StartPlayerSettlement(FrontierPlayerController, EFrontierRaidOutcome::Dead);
}

void AFrontierGameMode::TravelControllerToLobby(AFrontierPlayerController* FrontierPlayerController, const TCHAR* FailureContext)
{
	TravelControllerToDestination(FrontierPlayerController, GetResolvedLobbyTravelDestination(), FailureContext);
}

void AFrontierGameMode::TravelControllerToDestination(
	AFrontierPlayerController* FrontierPlayerController,
	const FString& Destination,
	const TCHAR* FailureContext) const
{
	if (!FrontierPlayerController)
	{
		return;
	}

	if (Destination.IsEmpty())
	{
		FRONTIER_LOG(Warning, TEXT("%s but travel destination could not be resolved. Controller=%s"), FailureContext, *GetNameSafe(FrontierPlayerController));
		return;
	}

	FRONTIER_LOG(
		Log,
		TEXT("[RaidJoinAuthorization] ClientTravel requested after flow transition. Controller=%s Context=%s Destination=%s"),
		*GetNameSafe(FrontierPlayerController),
		FailureContext ? FailureContext : TEXT("<none>"),
		*Destination);
	FrontierPlayerController->ClientTravel(Destination, TRAVEL_Absolute);
}

FString AFrontierGameMode::ResolveTravelDestination(
	const EFrontierTravelMode TravelMode,
	const FString& ServerAddress,
	const TSoftObjectPtr<UWorld>& LevelAsset) const
{
	if (TravelMode == EFrontierTravelMode::ServerAddress)
	{
		return ServerAddress;
	}

	if (LevelAsset.IsNull())
	{
		return FString();
	}

	return LevelAsset.ToSoftObjectPath().GetLongPackageName();
}

void AFrontierGameMode::ExecuteDelayedLobbyReturn(TWeakObjectPtr<AFrontierPlayerController> FrontierPlayerController)
{
	if (!FrontierPlayerController.IsValid())
	{
		return;
	}

	TravelControllerToLobby(FrontierPlayerController.Get(), TEXT("Player death return"));
}

void AFrontierGameMode::ResetRaidPlayerLobbyState() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(Iterator->Get());
		AFrontierPlayerState* FrontierPlayerState = FrontierPlayerController ? FrontierPlayerController->GetPlayerState<AFrontierPlayerState>() : nullptr;
		if (!FrontierPlayerController || !FrontierPlayerState)
		{
			continue;
		}

		FrontierPlayerState->SetRaidReady(false);
		FrontierPlayerState->SetTeamId(0);

	}
}

void AFrontierGameMode::TransitionControllerToRaidSpectator(
	AFrontierPlayerController* FrontierPlayerController,
	const bool bRemoveObservedPawnFromWorld,
	const TCHAR* Context)
{
	if (!FrontierPlayerController)
	{
		return;
	}

	APawn* PreviousPawn = FrontierPlayerController->GetPawn();
	FrontierPlayerController->StartSpectatingOnly();

	if (PreviousPawn)
	{
		PreviousPawn->DisableInput(FrontierPlayerController);

		if (bRemoveObservedPawnFromWorld)
		{
			PreviousPawn->SetActorEnableCollision(false);
			PreviousPawn->SetActorHiddenInGame(true);
			PreviousPawn->SetLifeSpan(0.2f);
		}
	}

	if (AActor* SpectatorTarget = FindSpectatorViewTarget(FrontierPlayerController))
	{
		FrontierPlayerController->SetViewTargetWithBlend(SpectatorTarget, 0.35f);
	}

	FRONTIER_LOG(Log, TEXT("%s -> spectator mode. Controller=%s Target=%s"),
		Context,
		*GetNameSafe(FrontierPlayerController),
		*GetNameSafe(FrontierPlayerController->GetViewTarget()));

}

AActor* AFrontierGameMode::FindSpectatorViewTarget(const AFrontierPlayerController* ObservingController) const
{
	UWorld* World = GetWorld();
	const AFrontierPlayerState* ObservingPlayerState = ObservingController ? ObservingController->GetPlayerState<AFrontierPlayerState>() : nullptr;
	if (!World || !ObservingPlayerState)
	{
		return nullptr;
	}

	TArray<AActor*> TeamTargets;
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const AFrontierPlayerController* CandidateController = Cast<AFrontierPlayerController>(Iterator->Get());
		if (!CandidateController || CandidateController == ObservingController)
		{
			continue;
		}

		const AFrontierPlayerState* CandidatePlayerState = CandidateController->GetPlayerState<AFrontierPlayerState>();
		AFrontierPlayerCharacter* CandidateCharacter = Cast<AFrontierPlayerCharacter>(CandidateController->GetPawn());
		if (!CandidatePlayerState || CandidatePlayerState->IsOnlyASpectator() || !CandidateCharacter || CandidateCharacter->IsDead())
		{
			continue;
		}

		if (ObservingPlayerState->GetTeamId() > 0 && CandidatePlayerState->GetTeamId() == ObservingPlayerState->GetTeamId())
		{
			TeamTargets.Add(CandidateCharacter);
		}
	}

	return TeamTargets.Num() > 0 ? TeamTargets[0] : nullptr;
}

int32 AFrontierGameMode::GetActiveRaidParticipantCount() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	int32 ActiveParticipantCount = 0;
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(Iterator->Get());
		if (!FrontierPlayerController)
		{
			continue;
		}

		const AFrontierPlayerState* FrontierPlayerState = FrontierPlayerController->GetPlayerState<AFrontierPlayerState>();
		const AFrontierPlayerCharacter* FrontierCharacter = Cast<AFrontierPlayerCharacter>(FrontierPlayerController->GetPawn());
		if (!FrontierPlayerState || FrontierPlayerState->IsOnlyASpectator() || !FrontierCharacter || FrontierCharacter->IsDead())
		{
			continue;
		}

		++ActiveParticipantCount;
	}

	return ActiveParticipantCount;
}

bool AFrontierGameMode::HasAnyActiveRaidParticipants() const
{
	return GetActiveRaidParticipantCount() > 0;
}

void AFrontierGameMode::ScheduleRaidEndTravelIfNeeded()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (HasAnyActiveRaidParticipants())
	{
		World->GetTimerManager().ClearTimer(RaidEndTravelTimerHandle);
		return;
	}

	if (bRaidEndTravelTriggered || World->GetTimerManager().IsTimerActive(RaidEndTravelTimerHandle))
	{
		return;
	}

	const float DelaySeconds = FMath::Max(0.2f, DeathReturnDelaySeconds);
	World->GetTimerManager().SetTimer(
		RaidEndTravelTimerHandle,
		this,
		&AFrontierGameMode::ReturnAllRaidPlayersToLobby,
		DelaySeconds,
		false);

	FRONTIER_LOG(Log, TEXT("Scheduled raid end lobby return in %.2f seconds because no active raid participants remain."), DelaySeconds);
}

void AFrontierGameMode::ReturnAllRaidPlayersToLobby()
{
	if (bRaidEndTravelTriggered)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (UFrontierRaidExperienceSubsystem* RaidExperience = World->GetSubsystem<UFrontierRaidExperienceSubsystem>())
	{
		if (RaidExperience->HasPendingExperienceGrants())
		{
			World->GetTimerManager().SetTimer(
				RaidEndTravelTimerHandle,
				this,
				&AFrontierGameMode::ReturnAllRaidPlayersToLobby,
				0.5f,
				false);
			return;
		}
	}

	bRaidEndTravelTriggered = true;

	World->GetTimerManager().ClearTimer(RaidEndTravelTimerHandle);
	World->GetTimerManager().ClearTimer(RaidCountdownTimerHandle);

	if (AFrontierGameState* FrontierGameState = GetGameState<AFrontierGameState>())
	{
		FrontierGameState->SetRaidTimerState(false, 0);
	}

	if (HasAnyActiveRaidParticipants())
	{
		FRONTIER_LOG(Log, TEXT("Cancelled raid end travel because active raid participants reappeared."));
		return;
	}

	ResetRaidPlayerLobbyState();

	const FString Destination = GetResolvedLobbyTravelDestination();
	if (Destination.IsEmpty())
	{
		return;
	}

	if (LobbyTravelMode == EFrontierTravelMode::MapPackage)
	{
		const FString TravelURL = GetNetMode() == NM_ListenServer
			? FString::Printf(TEXT("%s?listen"), *Destination)
			: Destination;
		World->ServerTravel(TravelURL);
		return;
	}

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(Iterator->Get()))
		{
			TravelControllerToDestination(FrontierPlayerController, Destination, TEXT("Raid session completed"));
		}
	}
}


