#include "Game/FrontierLobbyPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Components/FrontierBackendProtocolComponent.h"
#include "Components/FrontierInventoryCommandService.h"
#include "Components/FrontierSkillTreePersistenceComponent.h"
#include "Components/FrontierSkillTreeComponent.h"
#include "Engine/World.h"
#include "Frontier.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Progression/FrontierRaidSessionSubsystem.h"
#include "Game/FrontierPlayerState.h"
#include "UI/FrontierLoadingScreenSubsystem.h"
#include "UI/FrontierSkillTreeWidget.h"
#include "UI/FrontierLobbyWidget.h"

AFrontierLobbyPlayerController::AFrontierLobbyPlayerController()
{
	BackendProtocolComponent = CreateDefaultSubobject<UFrontierBackendProtocolComponent>(TEXT("BackendProtocolComponent"));
	InventoryCommandService = CreateDefaultSubobject<UFrontierInventoryCommandService>(TEXT("InventoryCommandService"));
	SkillTreePersistenceComponent = CreateDefaultSubobject<UFrontierSkillTreePersistenceComponent>(TEXT("SkillTreePersistenceComponent"));
	SkillTreeWidgetClass = TSoftClassPtr<UFrontierSkillTreeWidget>(
		FSoftObjectPath(TEXT("/Game/SY/WBP/SkillTree/WBP_SkillTree.WBP_SkillTree_C")));
}

void AFrontierLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}
	bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	if (!LobbyWidgetClass)
	{
		FRONTIER_LOG(Warning, TEXT("Lobby widget class is not assigned. Controller=%s"), *GetNameSafe(this));
		return;
	}

	LobbyWidget = CreateWidget<UFrontierLobbyWidget>(this, LobbyWidgetClass);
	if (!LobbyWidget)
	{
		FRONTIER_LOG(Warning, TEXT("Lobby widget creation failed. Controller=%s WidgetClass=%s"),
			*GetNameSafe(this),
			*GetNameSafe(LobbyWidgetClass.Get()));
		return;
	}

	LobbyWidget->AddToViewport(0);
	LobbyWidget->SetKeyboardFocus();
}

void AFrontierLobbyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RaidLoadingScreenTimerHandle);
	}
	ClearPendingRaidConnection();

	if (SkillTreeWidget)
	{
		SkillTreeWidget->OnCloseRequested.RemoveDynamic(
			this,
			&AFrontierLobbyPlayerController::HandleSkillTreeCloseRequested);
		SkillTreeWidget->RemoveFromParent();
		SkillTreeWidget = nullptr;
	}
	if (LobbyWidget)
	{
		LobbyWidget->RemoveFromParent();
		LobbyWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AFrontierLobbyPlayerController::ShowChangeNicknameWidget(const bool bCanCancel)
{
	if (LobbyWidget)
	{
		LobbyWidget->ShowChangeNicknameWidget(bCanCancel);
	}
}

void AFrontierLobbyPlayerController::ShowRaidLoadingScreenAfterDelay(const float DelaySeconds)
{
	if (!IsLocalController() || !GetWorld())
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFrontierLoadingScreenSubsystem* LoadingScreen =
			GameInstance->GetSubsystem<UFrontierLoadingScreenSubsystem>())
		{
			if (LoadingScreen->IsRaidLoadingScreenActive())
			{
				return;
			}
		}
	}

	// SERVER_STARTING can be delivered repeatedly by HTTP recovery polling.
	// Keep the first transition instead of recreating the widget on every update.
	if (GetWorld()->GetTimerManager().IsTimerActive(RaidLoadingScreenTimerHandle))
	{
		return;
	}

	if (DelaySeconds <= 0.0f)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UFrontierLoadingScreenSubsystem* LoadingScreen =
				GameInstance->GetSubsystem<UFrontierLoadingScreenSubsystem>())
			{
				LoadingScreen->StartRaidLoadingScreen();
				LoadingScreen->SetRaidLoadingProgress(
					0.1f,
					NSLOCTEXT("FrontierLoading", "DedicatedServerStarting", "게임 서버 준비 중..."));
			}
		}
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		RaidLoadingScreenTimerHandle,
		this,
		&AFrontierLobbyPlayerController::ShowRaidLoadingScreen,
		DelaySeconds,
		false);
}

void AFrontierLobbyPlayerController::ShowRaidLoadingScreen()
{
	if (!IsLocalController())
	{
		return;
	}
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RaidLoadingScreenTimerHandle);
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFrontierLoadingScreenSubsystem* LoadingScreen =
			GameInstance->GetSubsystem<UFrontierLoadingScreenSubsystem>())
		{
			LoadingScreen->StartRaidLoadingScreen();
			LoadingScreen->SetRaidLoadingProgress(
				0.35f,
				NSLOCTEXT("FrontierLoading", "DedicatedServerReady", "게임 서버 접속 준비 완료"));
		}
	}
}

void AFrontierLobbyPlayerController::CancelRaidLoadingScreenTransition()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RaidLoadingScreenTimerHandle);
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFrontierLoadingScreenSubsystem* LoadingScreen =
			GameInstance->GetSubsystem<UFrontierLoadingScreenSubsystem>())
		{
			LoadingScreen->StopRaidLoadingScreen();
		}
	}
}

void AFrontierLobbyPlayerController::ShowSkillTreeFromLobby()
{
	if (!IsLocalController() || !LobbyWidget)
	{
		return;
	}
	UClass* ResolvedSkillTreeWidgetClass = SkillTreeWidgetClass.LoadSynchronous();
	if (!ResolvedSkillTreeWidgetClass)
	{
		FRONTIER_LOG(Warning, TEXT("Skill tree widget class is not assigned. Controller=%s"), *GetNameSafe(this));
		return;
	}
	if (!SkillTreeWidget)
	{
		SkillTreeWidget = CreateWidget<UFrontierSkillTreeWidget>(this, ResolvedSkillTreeWidgetClass);
		if (!SkillTreeWidget)
		{
			FRONTIER_LOG(Warning, TEXT("Skill tree widget creation failed. Controller=%s WidgetClass=%s"),
				*GetNameSafe(this),
				*GetNameSafe(ResolvedSkillTreeWidgetClass));
			return;
		}
	}

	SkillTreeWidget->OnCloseRequested.RemoveDynamic(
		this,
		&AFrontierLobbyPlayerController::HandleSkillTreeCloseRequested);
	SkillTreeWidget->OnCloseRequested.AddUniqueDynamic(
		this,
		&AFrontierLobbyPlayerController::HandleSkillTreeCloseRequested);

	LobbyWidget->SetVisibility(ESlateVisibility::Collapsed);
	if (!SkillTreeWidget->IsInViewport())
	{
		SkillTreeWidget->AddToViewport(10);
	}
	SkillTreeWidget->RefreshSkillTree();
	SkillTreeWidget->SetKeyboardFocus();

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(SkillTreeWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void AFrontierLobbyPlayerController::RequestDebugSkillPoints()
{
#if !UE_BUILD_SHIPPING
	if (IsLocalController())
	{
		ServerGrantDebugSkillPoints();
	}
#endif
}

void AFrontierLobbyPlayerController::ServerGrantDebugSkillPoints_Implementation()
{
#if !UE_BUILD_SHIPPING
	AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>();
	UFrontierSkillTreeComponent* SkillTree = FrontierPlayerState
		? FrontierPlayerState->GetSkillTreeComponent()
		: nullptr;
	if (SkillTree && SkillTree->GrantSkillPoints(3))
	{
		FRONTIER_LOG(Log, TEXT("Granted 3 development skill points. Controller=%s Available=%d"), *GetName(), SkillTree->GetAvailableSkillPoints());
	}
	else
	{
		FRONTIER_LOG(Warning, TEXT("Could not grant development skill points. Controller=%s"), *GetName());
	}
#endif
}

void AFrontierLobbyPlayerController::HandleSkillTreeCloseRequested()
{
	if (SkillTreeWidget)
	{
		SkillTreeWidget->RemoveFromParent();
	}
	if (!LobbyWidget)
	{
		return;
	}

	LobbyWidget->SetVisibility(ESlateVisibility::Visible);
	LobbyWidget->SetKeyboardFocus();

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(LobbyWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void AFrontierLobbyPlayerController::RequestStartRaidFromLobby(
	const FString& MapId,
	const FString& PartyId)
{
	FRONTIER_LOG_FUNC();

	if (BackendProtocolComponent)
	{
		FRONTIER_LOG(
			Log,
			TEXT("[RaidEntry] Lobby controller forwarding request. Controller=%s Local=%d Authority=%d BackendRequestInProgress=%d MapId=%s HasPartyId=%d"),
			*GetName(),
			IsLocalController() ? 1 : 0,
			HasAuthority() ? 1 : 0,
			BackendProtocolComponent->IsRequestInProgress() ? 1 : 0,
			MapId.IsEmpty() ? TEXT("<empty>") : *MapId,
			PartyId.IsEmpty() ? 0 : 1);
		BackendProtocolComponent->RequestStartMatchmaking(MapId, PartyId);
	}
	else
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidEntry] Request aborted: BackendProtocolComponent is null. Controller=%s Local=%d Authority=%d"),
			*GetName(),
			IsLocalController() ? 1 : 0,
			HasAuthority() ? 1 : 0);
	}
}

void AFrontierLobbyPlayerController::HandleRaidEntryCreated(
	const FFrontierOnlineRaidEntryDTO& Entry)
{
	FRONTIER_LOG_FUNC();

	FString ServerEndpoint = Entry.ServerEndpoint;
	ServerEndpoint.TrimStartAndEndInline();
	const bool bInvalidEndpoint = ServerEndpoint.IsEmpty()
		|| ServerEndpoint.Contains(TEXT("?"))
		|| ServerEndpoint.Contains(TEXT("#"))
		|| ServerEndpoint.Contains(TEXT("\r"))
		|| ServerEndpoint.Contains(TEXT("\n"))
		|| ServerEndpoint.Contains(TEXT("\t"));
	if (Entry.RaidSession.RaidSessionId.IsEmpty() || Entry.JoinToken.IsEmpty() || bInvalidEndpoint)
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidEntry] Connection response is invalid. HasRaidSessionId=%d HasEndpoint=%d HasJoinToken=%d"),
			Entry.RaidSession.RaidSessionId.IsEmpty() ? 0 : 1,
			ServerEndpoint.IsEmpty() ? 0 : 1,
			Entry.JoinToken.IsEmpty() ? 0 : 1);
		ClientReceiveRaidFlowFailed(TEXT("레이드 서버 접속 정보가 올바르지 않습니다."), false);
		return;
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFrontierLoadingScreenSubsystem* LoadingScreen =
			GameInstance->GetSubsystem<UFrontierLoadingScreenSubsystem>())
		{
			LoadingScreen->SetRaidLoadingProgress(
				0.55f,
				NSLOCTEXT("FrontierLoading", "RaidEntryCreated", "레이드 입장 정보를 받는 중..."));
		}
	}

	FRONTIER_LOG(
		Log,
		TEXT("[RaidEntry] Forwarding ready connection to client. RaidSessionId=%s Endpoint=%s"),
		*Entry.RaidSession.RaidSessionId,
		*ServerEndpoint);
	ClientConnectToRaid(Entry.RaidSession.RaidSessionId, ServerEndpoint, Entry.JoinToken);
}

void AFrontierLobbyPlayerController::ClientConnectToRaid_Implementation(
	const FString& RaidSessionId,
	const FString& ServerEndpoint,
	const FString& JoinToken)
{
	FRONTIER_LOG_FUNC();

	if (!IsLocalController() || RaidSessionId.IsEmpty() || ServerEndpoint.IsEmpty() || JoinToken.IsEmpty())
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidEntry] Client travel rejected invalid connection data. Local=%d HasRaidSessionId=%d HasEndpoint=%d HasJoinToken=%d"),
			IsLocalController() ? 1 : 0,
			RaidSessionId.IsEmpty() ? 0 : 1,
			ServerEndpoint.IsEmpty() ? 0 : 1,
			JoinToken.IsEmpty() ? 0 : 1);
		ClientReceiveRaidFlowFailed(TEXT("레이드 서버 접속 정보를 처리하지 못했습니다."), false);
		return;
	}

	ClearPendingRaidConnection();
	PendingRaidSessionId = RaidSessionId;
	PendingRaidServerEndpoint = ServerEndpoint;
	PendingRaidJoinToken = JoinToken;

	PerformPendingRaidConnection();
}

void AFrontierLobbyPlayerController::PerformPendingRaidConnection()
{
	FRONTIER_LOG_FUNC();

	if (!IsLocalController()
		|| PendingRaidSessionId.IsEmpty()
		|| PendingRaidServerEndpoint.IsEmpty()
		|| PendingRaidJoinToken.IsEmpty())
	{
		ClearPendingRaidConnection();
		ClientReceiveRaidFlowFailed(TEXT("예약된 레이드 서버 접속 정보가 올바르지 않습니다."), false);
		return;
	}

	if (UFrontierRaidSessionSubsystem* RaidSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr)
	{
		RaidSession->AcceptClientRaidEntryContext(PendingRaidSessionId);
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFrontierLoadingScreenSubsystem* LoadingScreen =
			GameInstance->GetSubsystem<UFrontierLoadingScreenSubsystem>())
		{
			LoadingScreen->SetRaidLoadingProgress(
				0.65f,
				NSLOCTEXT("FrontierLoading", "EnteringRaid", "레이드 서버에 접속하는 중..."));
		}
	}

	const FString TravelUrl = FString::Printf(
		TEXT("%s?RaidSessionId=%s?JoinToken=%s"),
		*PendingRaidServerEndpoint,
		*FGenericPlatformHttp::UrlEncode(PendingRaidSessionId),
		*FGenericPlatformHttp::UrlEncode(PendingRaidJoinToken));
	FRONTIER_LOG(
		Log,
		TEXT("[RaidEntry] Starting client travel. RaidSessionId=%s Endpoint=%s"),
		*PendingRaidSessionId,
		*PendingRaidServerEndpoint);

	ClientTravel(TravelUrl, TRAVEL_Absolute);
}

void AFrontierLobbyPlayerController::ClearPendingRaidConnection()
{
	PendingRaidSessionId.Reset();
	PendingRaidServerEndpoint.Reset();
	PendingRaidJoinToken.Reset();
}

void AFrontierLobbyPlayerController::RequestDebugRaidExperience()
{
	FRONTIER_LOG_FUNC();

#if !UE_BUILD_SHIPPING
	ServerAwardDebugRaidExperience(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
#endif
}

void AFrontierLobbyPlayerController::RequestEndSimulatedRaid(
	const EFrontierRaidOutcome Outcome)
{
	FRONTIER_LOG_FUNC();

	UFrontierRaidSessionSubsystem* RaidSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr;
	if (!RaidSession || !RaidSession->IsClientRaidActive())
	{
		return;
	}
	// Lock the local controls before the reliable RPC is queued. Server authority
	// still validates the state and produces the immutable settlement snapshot.
	RaidSession->MarkClientSettlementState(EFrontierRaidFlowState::FinalizingLocalResult);
	ServerEndSimulatedRaid(Outcome == EFrontierRaidOutcome::Extracted);
}

void AFrontierLobbyPlayerController::ServerAwardDebugRaidExperience_Implementation(
	const FString& EventId)
{
#if UE_BUILD_SHIPPING
	return;
#else
	FString Error;
	UFrontierRaidSessionSubsystem* RaidSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr;
	if (!RaidSession || !RaidSession->TryAwardDebugExperience(this, EventId, Error))
	{
		FRONTIER_LOG(Warning, TEXT("Debug raid experience rejected. Error=%s"), *Error);
	}
#endif
}

void AFrontierLobbyPlayerController::ServerEndSimulatedRaid_Implementation(
	const bool bExtracted)
{
	FString Error;
	UFrontierRaidSessionSubsystem* RaidSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr;
	if (!RaidSession || !RaidSession->BeginServerSettlement(
		this,
		bExtracted ? EFrontierRaidOutcome::Extracted : EFrontierRaidOutcome::Dead,
		Error))
	{
		ClientReceiveRaidFlowFailed(
			Error.IsEmpty() ? TEXT("Raid settlement could not start.") : Error,
			false);
	}
}

void AFrontierLobbyPlayerController::ClientReceiveSimulatedRaidActivated_Implementation(
	const FString& RaidSessionId)
{
	if (UFrontierRaidSessionSubsystem* RaidSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr)
	{
		RaidSession->MarkClientRaidActive(RaidSessionId);
	}
}

void AFrontierLobbyPlayerController::ClientReceiveRaidSettlementState_Implementation(
	const EFrontierRaidFlowState State,
	const FString& Message)
{
	if (UFrontierRaidSessionSubsystem* RaidSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr)
	{
		RaidSession->MarkClientSettlementState(State, Message);
	}
}

void AFrontierLobbyPlayerController::ClientReceiveRaidSettlementReady_Implementation(
	const FString& RaidSessionId)
{
	UFrontierRaidSessionSubsystem* RaidSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr;
	if (RaidSession)
	{
		if (RaidSession->GetClientState() == EFrontierRaidFlowState::CommittingRaidResult)
		{
			RaidSession->MarkClientSettlementState(EFrontierRaidFlowState::GrantingExperience);
		}
		RaidSession->MarkClientSettlementState(EFrontierRaidFlowState::ReturningToLobby);
		RaidSession->BeginClientLobbyRefresh();
	}
	if (BackendProtocolComponent)
	{
		BackendProtocolComponent->RefreshLobbyAfterRaid(RaidSessionId);
	}
}

void AFrontierLobbyPlayerController::ClientReceiveRaidFlowFailed_Implementation(
	const FString& Error,
	const bool bRetryable)
{
	CancelRaidLoadingScreenTransition();
	if (LobbyWidget)
	{
		LobbyWidget->HandleRaidFlowFailure(Error);
	}
	ClearPendingRaidConnection();

	UFrontierRaidSessionSubsystem* RaidSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr;
	const EFrontierRaidFlowState CurrentState = RaidSession
		? RaidSession->GetClientState()
		: EFrontierRaidFlowState::Idle;
	FRONTIER_LOG(
		Error,
		TEXT("[RaidEntry] Client received raid-flow failure. Error=%s Retryable=%d HasRaidSubsystem=%d State=%s(%d) RaidSessionId=%s"),
		Error.IsEmpty() ? TEXT("<empty>") : *Error,
		bRetryable ? 1 : 0,
		RaidSession ? 1 : 0,
		RaidSession
			? *StaticEnum<EFrontierRaidFlowState>()->GetNameStringByValue(static_cast<int64>(CurrentState))
			: TEXT("<unavailable>"),
		static_cast<int32>(CurrentState),
		RaidSession && !RaidSession->GetLastRaidSessionId().IsEmpty()
			? *RaidSession->GetLastRaidSessionId()
			: TEXT("<empty>"));
	if (RaidSession)
	{
		RaidSession->FailClientFlow(Error, bRetryable);
	}
}

void AFrontierLobbyPlayerController::ServerEquipRaidInventoryItemToLoadout_Implementation(const int32 SourceSlotIndex, const EFrontierEquipmentSlot SlotType)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->EquipRaidInventoryItemToLoadout(SourceSlotIndex, SlotType);
	}
}

void AFrontierLobbyPlayerController::ServerUnequipLoadoutItemToRaidInventory_Implementation(const EFrontierEquipmentSlot SlotType)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->UnequipLoadoutItemToRaidInventory(SlotType);
	}
}

void AFrontierLobbyPlayerController::ServerUnequipLoadoutItemToRaidInventorySlot_Implementation(const EFrontierEquipmentSlot SlotType, const int32 TargetSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->UnequipLoadoutItemToRaidInventorySlot(SlotType, TargetSlotIndex);
	}
}

void AFrontierLobbyPlayerController::ServerSwapRaidInventorySlots_Implementation(const int32 SourceSlotIndex, const int32 TargetSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->SwapRaidInventorySlots(SourceSlotIndex, TargetSlotIndex);
	}
}

void AFrontierLobbyPlayerController::ServerSwapStorageSlots_Implementation(const int32 SourceSlotIndex, const int32 TargetSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->SwapStorageSlots(SourceSlotIndex, TargetSlotIndex);
	}
}

void AFrontierLobbyPlayerController::ServerStoreRaidItemInStorage_Implementation(const int32 SourceRaidSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->StoreRaidItemInStorage(SourceRaidSlotIndex);
	}
}

void AFrontierLobbyPlayerController::ServerStoreRaidItemInStorageSlot_Implementation(const int32 SourceRaidSlotIndex, const int32 TargetLobbySlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->StoreRaidItemInStorageSlot(SourceRaidSlotIndex, TargetLobbySlotIndex);
	}
}

void AFrontierLobbyPlayerController::ServerWithdrawLobbyItemToRaidInventory_Implementation(const int32 SourceLobbySlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->WithdrawLobbyItemToRaidInventory(SourceLobbySlotIndex);
	}
}

void AFrontierLobbyPlayerController::ServerWithdrawLobbyItemToRaidInventorySlot_Implementation(const int32 SourceLobbySlotIndex, const int32 TargetRaidSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->WithdrawLobbyItemToRaidInventorySlot(SourceLobbySlotIndex, TargetRaidSlotIndex);
	}
}

void AFrontierLobbyPlayerController::ServerRequestUpgradeItem_Implementation(
	const FGuid ItemInstanceId,
	const int32 ExpectedEnhancementLevel,
	const FString& IdempotencyKey)
{
	if (BackendProtocolComponent)
	{
		BackendProtocolComponent->RequestUpgradeItem(
			ItemInstanceId,
			ExpectedEnhancementLevel,
			IdempotencyKey);
	}
}
