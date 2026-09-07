#include "UI/FrontierLobbyWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "Frontier.h"
#include "Game/FrontierLobbyPlayerController.h"
#include "Game/FrontierPlayerState.h"
#include "Game/FrontierSteamSubsystem.h"
#include "Game/FrontierSteamPartySubsystem.h"
#include "Components/FrontierInventoryComponent.h"
#include "Components/FrontierLoadoutComponent.h"
#include "Components/FrontierRaidInventoryComponent.h"
#include "Components/FrontierStorageComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Input/Reply.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Online/FrontierPlayerSessionSubsystem.h"
#include "Progression/FrontierUpgradeBalanceSubsystem.h"
#include "Progression/FrontierRaidSessionSubsystem.h"
#include "UI/FrontierLobbyMapIconWidget.h"
#include "UI/FrontierMapInfoTypes.h"
#include "UI/FrontierRaidEntryWarningWidget.h"
#include "UI/FrontierChangeNicknameWidget.h"
#include "UI/FrontierPopupSubsystem.h"
#include "UI/FrontierRaidSettlementWidget.h"
#include "UI/FrontierSettingsWidget.h"
#include "UI/FrontierUserCurrencySlot.h"
#include "UI/OutGameInventoryWrapperWidget.h"
#include "UI/UpgradePanel.h"

UFrontierLobbyWidget::UFrontierLobbyWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UFrontierLobbyWidget::ShowChangeNicknameWidget(const bool bCanCancel)
{
	if (!WBP_ChangeNickname)
	{
		return;
	}

	const UFrontierPlayerSessionSubsystem* PlayerSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	const FString CurrentNickname = PlayerSession ? PlayerSession->GetNickname() : FString();
	if (UFrontierChangeNicknameWidget* NativeWidget = Cast<UFrontierChangeNicknameWidget>(WBP_ChangeNickname))
	{
		NativeWidget->Open(bCanCancel, CurrentNickname);
		return;
	}

	if (!FallbackNicknameTextBox || !FallbackNicknameConfirmButton || !FallbackNicknameCancelButton)
	{
		BindFallbackChangeNicknameWidget();
	}
	if (FallbackNicknameTextBox)
	{
		FallbackNicknameTextBox->SetText(FText::FromString(CurrentNickname));
		FallbackNicknameTextBox->SetKeyboardFocus();
	}
	if (FallbackNicknameCancelButton)
	{
		FallbackNicknameCancelButton->SetVisibility(
			bCanCancel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (FallbackNicknameConfirmButton)
	{
		FallbackNicknameConfirmButton->SetIsEnabled(true);
	}
	WBP_ChangeNickname->SetVisibility(ESlateVisibility::Visible);
}

void UFrontierLobbyWidget::BindFallbackChangeNicknameWidget()
{
	UUserWidget* ChangeNicknameWidget = Cast<UUserWidget>(WBP_ChangeNickname);
	if (!ChangeNicknameWidget || Cast<UFrontierChangeNicknameWidget>(ChangeNicknameWidget))
	{
		return;
	}

	FallbackNicknameTextBox = Cast<UEditableTextBox>(
		ChangeNicknameWidget->GetWidgetFromName(TEXT("EditableText_NickName")));
	FallbackNicknameConfirmButton = Cast<UButton>(
		ChangeNicknameWidget->GetWidgetFromName(TEXT("Button_Confirm")));
	FallbackNicknameCancelButton = Cast<UButton>(
		ChangeNicknameWidget->GetWidgetFromName(TEXT("Button_Cancel")));

	if (FallbackNicknameConfirmButton)
	{
		FallbackNicknameConfirmButton->OnClicked.RemoveAll(this);
		FallbackNicknameConfirmButton->OnClicked.AddDynamic(
			this,
			&ThisClass::HandleFallbackNicknameConfirmClicked);
	}
	if (FallbackNicknameCancelButton)
	{
		FallbackNicknameCancelButton->OnClicked.RemoveAll(this);
		FallbackNicknameCancelButton->OnClicked.AddDynamic(
			this,
			&ThisClass::HandleFallbackNicknameCancelClicked);
	}
}

void UFrontierLobbyWidget::UnbindFallbackChangeNicknameWidget()
{
	if (FallbackNicknameConfirmButton)
	{
		FallbackNicknameConfirmButton->OnClicked.RemoveAll(this);
	}
	if (FallbackNicknameCancelButton)
	{
		FallbackNicknameCancelButton->OnClicked.RemoveAll(this);
	}
	FallbackNicknameTextBox = nullptr;
	FallbackNicknameConfirmButton = nullptr;
	FallbackNicknameCancelButton = nullptr;
}

void UFrontierLobbyWidget::HandleFallbackNicknameConfirmClicked()
{
	if (bFallbackNicknameRequestInProgress || !FallbackNicknameTextBox || !BackendProtocolComponent)
	{
		return;
	}

	FString Nickname = FallbackNicknameTextBox->GetText().ToString();
	Nickname.TrimStartAndEndInline();
	if (Nickname.IsEmpty())
	{
		return;
	}

	bFallbackNicknameRequestInProgress = true;
	if (FallbackNicknameConfirmButton)
	{
		FallbackNicknameConfirmButton->SetIsEnabled(false);
	}
	BackendProtocolComponent->RequestUpdateNickname(Nickname);
}

void UFrontierLobbyWidget::HandleFallbackNicknameCancelClicked()
{
	if (!bFallbackNicknameRequestInProgress && WBP_ChangeNickname)
	{
		WBP_ChangeNickname->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UFrontierLobbyWidget::HandleRaidFlowFailure(const FString& ErrorMessage)
{
	const bool bShouldShowFailurePopup = bMatchmakingActive
		|| bMatchmakingCancelInProgress
		|| bMatchmakingServerReady
		|| (Box_MatchMaking && Box_MatchMaking->GetVisibility() != ESlateVisibility::Collapsed);

	bMatchmakingActive = false;
	bMatchmakingCancelInProgress = false;
	bMatchmakingServerReady = false;
	PendingMatchmakingFailureMessage.Reset();

	if (Box_MatchMaking)
	{
		Box_MatchMaking->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshRaidControls();

	// Avoid showing a duplicate popup when the FAILED status delegate already handled it.
	if (bShouldShowFailurePopup)
	{
		ShowMatchmakingFailurePopup(ErrorMessage);
	}
}

void UFrontierLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindSteamSubsystem();
	BindSteamPartySubsystem();
	BindBackendProtocolComponent();
	BindRaidIntegration();

	if (Button_Start)
	{
		Button_Start->OnClicked.RemoveAll(this);
		Button_Start->OnClicked.AddDynamic(this, &UFrontierLobbyWidget::HandleStartButtonClicked);
		Button_Start->SetIsEnabled(true);
	}

	if (Button_Retry)
	{
		Button_Retry->OnClicked.RemoveAll(this);
		Button_Retry->OnClicked.AddDynamic(this, &UFrontierLobbyWidget::HandleRetryButtonClicked);
		Button_Retry->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (Button_Quit)
	{
		Button_Quit->OnClicked.RemoveAll(this);
		Button_Quit->OnClicked.AddDynamic(this, &UFrontierLobbyWidget::HandleQuitButtonClicked);
	}
	if (Button_CancelMatchMaking)
	{
		Button_CancelMatchMaking->OnClicked.RemoveAll(this);
		Button_CancelMatchMaking->OnClicked.AddDynamic(
			this,
			&UFrontierLobbyWidget::HandleCancelMatchMakingButtonClicked);
	}
	if (Button_lobbySettings)
	{
		Button_lobbySettings->OnClicked.AddUniqueDynamic(
			this,
			&UFrontierLobbyWidget::HandleSettingsButtonClicked);
	}
	if (Button_startSettings)
	{
		Button_startSettings->OnClicked.AddUniqueDynamic(
			this,
			&UFrontierLobbyWidget::HandleSettingsButtonClicked);
	}
	if (WBP_SettingsWidget)
	{
		WBP_SettingsWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (WBP_ChangeNickname)
	{
		WBP_ChangeNickname->SetVisibility(ESlateVisibility::Collapsed);
	}
	BindFallbackChangeNicknameWidget();

	BindContentButtons();
	BindMapSelectButtons();
	BindLoadoutEvents();
	RefreshUpgradeStoneDisplay();
	RefreshCurrentEquipmentScoreDisplay();
	if (MapInfoPanel)
	{
		MapInfoPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Button_InviteFriend1)
	{
		Button_InviteFriend1->OnClicked.RemoveAll(this);
		Button_InviteFriend1->OnClicked.AddDynamic(this, &ThisClass::HandleInviteFriend1Clicked);
	}
	if (Button_InviteFriend2)
	{
		Button_InviteFriend2->OnClicked.RemoveAll(this);
		Button_InviteFriend2->OnClicked.AddDynamic(this, &ThisClass::HandleInviteFriend2Clicked);
	}
	RefreshSteamPartyDisplay();
	SetMapSelectPanelVisible(false);

	bAuthenticationInProgress = false;
	bLobbyInitialized = false;
	SetLobbyInitState(EFrontierLobbyInitState::None, TEXT(""));
	SetActiveLobbyPanel(EFrontierLobbyPanel::Start);
	SetActiveContentPanel(EFrontierLobbyContentPanel::Lobby);
	bMatchmakingActive = false;
	bMatchmakingCancelInProgress = false;
	bMatchmakingServerReady = false;
	PendingMatchmakingFailureMessage.Reset();
	if (Box_MatchMaking)
	{
		Box_MatchMaking->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshRaidControls();

	if (WBP_RaidSettlement)
	{
		WBP_RaidSettlement->OnReturnToLobbyRequested.RemoveDynamic(
			this,
			&UFrontierLobbyWidget::HandleSettlementReturnToLobbyRequested);
		WBP_RaidSettlement->OnReturnToLobbyRequested.AddUniqueDynamic(
			this,
			&UFrontierLobbyWidget::HandleSettlementReturnToLobbyRequested);

		FFrontierRaidSettlementPresentation Presentation;
		if (RaidSessionSubsystem && RaidSessionSubsystem->ConsumeSettlementPresentation(Presentation))
		{
			WBP_RaidSettlement->PlaySettlement(Presentation);
		}
		else
		{
			WBP_RaidSettlement->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (RaidSessionSubsystem && RaidSessionSubsystem->IsAwaitingSettlementLevelRefresh())
	{
		UFrontierPlayerSessionSubsystem* PlayerSession = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
			: nullptr;
		if (PlayerSession && PlayerSession->HasAuthenticatedSession())
		{
			ResumeLobbyInitialization();
		}
		else
		{
			StartLobbyInitialization();
		}
	}
}

void UFrontierLobbyWidget::NativeDestruct()
{
	UnbindFallbackChangeNicknameWidget();
	UnbindBackendProtocolComponent();
	UnbindSteamSubsystem();
	UnbindSteamPartySubsystem();
	UnbindContentButtons();
	UnbindRaidIntegration();
	UnbindMapSelectButtons();
	UnbindUpgradeStoneInventoryEvents();
	UnbindLoadoutEvents();

	if (Button_Start)
	{
		Button_Start->OnClicked.RemoveAll(this);
	}
	if (Button_Retry)
	{
		Button_Retry->OnClicked.RemoveAll(this);
	}
	if (Button_Quit)
	{
		Button_Quit->OnClicked.RemoveAll(this);
	}
	if (Button_CancelMatchMaking)
	{
		Button_CancelMatchMaking->OnClicked.RemoveAll(this);
	}
	if (Button_lobbySettings)
	{
		Button_lobbySettings->OnClicked.RemoveAll(this);
	}
	if (Button_startSettings)
	{
		Button_startSettings->OnClicked.RemoveAll(this);
	}
	if (Button_InviteFriend1)
	{
		Button_InviteFriend1->OnClicked.RemoveAll(this);
	}
	if (Button_InviteFriend2)
	{
		Button_InviteFriend2->OnClicked.RemoveAll(this);
	}
	if (bRaidEntryWarningCreatedAtRuntime && WBP_RaidEntryWarning)
	{
		WBP_RaidEntryWarning->RemoveFromParent();
		WBP_RaidEntryWarning = nullptr;
		bRaidEntryWarningCreatedAtRuntime = false;
	}
	if (WBP_RaidSettlement)
	{
		WBP_RaidSettlement->OnReturnToLobbyRequested.RemoveDynamic(
			this,
			&UFrontierLobbyWidget::HandleSettlementReturnToLobbyRequested);
	}
	Super::NativeDestruct();
}

void UFrontierLobbyWidget::HandleStartButtonClicked()
{
	StartLobbyInitialization();
}

void UFrontierLobbyWidget::HandleInviteFriend1Clicked()
{
	if (!SteamPartySubsystem || !SteamPartySubsystem->OpenSteamInviteOverlay())
	{
		FRONTIER_LOG(Warning, TEXT("Steam friend invite could not be started from party slot 1."));
	}
}

void UFrontierLobbyWidget::HandleInviteFriend2Clicked()
{
	if (!SteamPartySubsystem || !SteamPartySubsystem->OpenSteamInviteOverlay())
	{
		FRONTIER_LOG(Warning, TEXT("Steam friend invite could not be started from party slot 2."));
	}
}

void UFrontierLobbyWidget::HandleSteamPartyMembersChanged(
	const TArray<FFrontierSteamPartyMember>& Members)
{
	RefreshSteamPartyDisplay();
	RefreshRaidControls();
	FRONTIER_LOG(
		Log,
		TEXT("[Matchmaking] Steam party membership changed. Controller=%s MemberCount=%d"),
		*GetNameSafe(GetOwningPlayer()),
		Members.Num());
}

void UFrontierLobbyWidget::HandleRetryButtonClicked()
{
	StartLobbyInitialization();
}

void UFrontierLobbyWidget::HandleQuitButtonClicked()
{
	FRONTIER_LOG_FUNC();

	if (Button_Quit)
	{
		Button_Quit->SetIsEnabled(false);
	}

	UFrontierPlayerSessionSubsystem* PlayerSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!PlayerSession || !PlayerSession->HasAuthenticatedSession())
	{
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
		return;
	}

	const TWeakObjectPtr<UFrontierLobbyWidget> WeakThis(this);
	PlayerSession->RequestLogout(
		false,
		[WeakThis](const bool bSucceeded, const FString& Error)
		{
			if (UFrontierLobbyWidget* This = WeakThis.Get())
			{
				if (!bSucceeded)
				{
					FRONTIER_LOG(
						Warning,
						TEXT("Application is exiting after logout failure. Error=%s"),
						Error.IsEmpty() ? TEXT("<empty>") : *Error);
				}
				UKismetSystemLibrary::QuitGame(
					This,
					This->GetOwningPlayer(),
					EQuitPreference::Quit,
					false);
			}
		});
}

void UFrontierLobbyWidget::HandleSettingsButtonClicked()
{
	if (UFrontierSettingsWidget* SettingsWidget = Cast<UFrontierSettingsWidget>(WBP_SettingsWidget))
	{
		SettingsWidget->OpenSettings();
	}
	else if (WBP_SettingsWidget)
	{
		WBP_SettingsWidget->SetVisibility(ESlateVisibility::Visible);
	}
}

void UFrontierLobbyWidget::HandleLobbyButtonClicked()
{
	SetActiveContentPanel(EFrontierLobbyContentPanel::Lobby);
}

void UFrontierLobbyWidget::HandleStorageButtonClicked()
{
	SetActiveContentPanel(EFrontierLobbyContentPanel::Inventory);
	RefreshStoragePanel();
}

void UFrontierLobbyWidget::HandleStoreButtonClicked()
{
	SetActiveContentPanel(EFrontierLobbyContentPanel::Store);
}

void UFrontierLobbyWidget::HandleSkillTreeButtonClicked()
{
	if (AFrontierLobbyPlayerController* Controller =
		Cast<AFrontierLobbyPlayerController>(GetOwningPlayer()))
	{
		Controller->ShowSkillTreeFromLobby();
	}
}

void UFrontierLobbyWidget::HandleUpgradeButtonClicked()
{
	SetActiveContentPanel(EFrontierLobbyContentPanel::Upgrade);
	if (WBP_UpgradePanel)
	{
		WBP_UpgradePanel->RefreshUpgradePanel();
	}
}

void UFrontierLobbyWidget::HandleMapSelectButtonClicked()
{
	if (!bLobbyInitialized)
	{
		return;
	}

	SetMapSelectPanelVisible(true);
}

void UFrontierLobbyWidget::HandleDungeonMapButtonClicked()
{
	SelectRaidMap(Button_L_Dungeon && !Button_L_Dungeon->GetMapId().IsNone()
		? Button_L_Dungeon->GetMapId()
		: MedievalDungeonID);
}

void UFrontierLobbyWidget::HandleForestMapButtonClicked()
{
	SelectRaidMap(Button_L_Forest && !Button_L_Forest->GetMapId().IsNone()
		? Button_L_Forest->GetMapId()
		: DarkForestID);
}

void UFrontierLobbyWidget::HandleRaidEntryButtonClicked()
{
	const EFrontierRaidFlowState CurrentState = RaidSessionSubsystem
		? RaidSessionSubsystem->GetClientState()
		: EFrontierRaidFlowState::Idle;
	FRONTIER_LOG(
		Log,
		TEXT("[RaidEntry] UI button clicked. LobbyInitialized=%d SelectedMap=%s HasRaidSubsystem=%d State=%s(%d) RaidSessionId=%s"),
		bLobbyInitialized ? 1 : 0,
		SelectedRaidMapName.IsNone() ? TEXT("<none>") : *SelectedRaidMapName.ToString(),
		RaidSessionSubsystem ? 1 : 0,
		RaidSessionSubsystem
			? *StaticEnum<EFrontierRaidFlowState>()->GetNameStringByValue(static_cast<int64>(CurrentState))
			: TEXT("<unavailable>"),
		static_cast<int32>(CurrentState),
		RaidSessionSubsystem && !RaidSessionSubsystem->GetLastRaidSessionId().IsEmpty()
			? *RaidSessionSubsystem->GetLastRaidSessionId()
			: TEXT("<empty>"));

	if (!bLobbyInitialized || SelectedRaidMapName.IsNone())
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidEntry] UI request rejected. LobbyInitialized=%d SelectedMap=%s"),
			bLobbyInitialized ? 1 : 0,
			SelectedRaidMapName.IsNone() ? TEXT("<none>") : *SelectedRaidMapName.ToString());
		return;
	}

	if (ShowRaidEntryWarningIfNeeded())
	{
		return;
	}

	// The map picker is a selection step, not the matchmaking status screen.
	SetMapSelectPanelVisible(false);

	FString PartyId;
	bool bIsPartyLeader = true;
	if (UFrontierSteamPartySubsystem* SteamParty = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierSteamPartySubsystem>()
		: nullptr)
	{
		// PrepareSteamPartyLobby creates a Steam Lobby for solo users too.
		// Only a Lobby with another member is a real party matchmaking request.
		if (SteamParty->HasOtherPartyMembers())
		{
			bIsPartyLeader = SteamParty->IsLocalPartyLeader();
			if (!bIsPartyLeader)
			{
				FRONTIER_LOG(
					Warning,
					TEXT("[Matchmaking] Non-leader matchmaking request rejected. SteamLobbyId=%s BackendPartyId=%s"),
					*SteamParty->GetSteamLobbyId(),
					SteamParty->GetBackendPartyId().IsEmpty() ? TEXT("<empty>") : *SteamParty->GetBackendPartyId());
				return;
			}

			PartyId = SteamParty->GetBackendPartyId();
			if (PartyId.IsEmpty())
			{
				FRONTIER_LOG(Warning, TEXT("[Matchmaking] Party matchmaking rejected because the backend PartyId is unavailable."));
				return;
			}
		}
	}

	if (AFrontierLobbyPlayerController* Controller =
		Cast<AFrontierLobbyPlayerController>(GetOwningPlayer()))
	{
		if (RaidSessionSubsystem && bHasCachedPlayerLevel)
		{
			RaidSessionSubsystem->CachePreRaidLevel(CachedPlayerLevel);
		}
		HandleMatchmakingStatusChanged(TEXT("REQUESTING"));
		FRONTIER_LOG(
			Log,
			TEXT("[Matchmaking] Starting matchmaking from lobby. MapId=%s Mode=%s PartyId=%s Leader=%d"),
			*SelectedRaidMapName.ToString(),
			PartyId.IsEmpty() ? TEXT("SOLO") : TEXT("PARTY"),
			PartyId.IsEmpty() ? TEXT("<none>") : *PartyId,
			bIsPartyLeader ? 1 : 0);
		Controller->RequestStartRaidFromLobby(SelectedRaidMapName.ToString(), PartyId);
	}
	else
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidEntry] UI request rejected: owning player is not AFrontierLobbyPlayerController. OwningPlayer=%s"),
			*GetNameSafe(GetOwningPlayer()));
	}
}

void UFrontierLobbyWidget::HandleMapSelectCloseButtonClicked()
{
	SetMapSelectPanelVisible(false);
}

void UFrontierLobbyWidget::HandleCancelMatchMakingButtonClicked()
{
	if (!BackendProtocolComponent || !bMatchmakingActive || bMatchmakingCancelInProgress)
	{
		return;
	}

	bMatchmakingCancelInProgress = true;
	if (Text_MatchMakingStatus)
	{
		Text_MatchMakingStatus->SetText(NSLOCTEXT(
			"FrontierLobby",
			"MatchmakingCancelling",
			"매치메이킹 취소 중..."));
	}
	RefreshMatchmakingControls();
	BackendProtocolComponent->CancelMatchmaking();
}

void UFrontierLobbyWidget::HandleRaidFlowStateChanged(
	const EFrontierRaidFlowState State,
	const FString& Message)
{
	(void)State;
	(void)Message;
	RefreshRaidControls();
}

void UFrontierLobbyWidget::HandleSteamTicketReceived(const FString& SteamTicket)
{
	if (!bAuthenticationInProgress || bLobbyInitialized)
	{
		return;
	}

	if (SteamTicket.IsEmpty())
	{
		HandleLobbyProcessFailed(TEXT("Steam 인증 티켓이 비어 있습니다."));
		return;
	}

	FRONTIER_LOG(Log, TEXT("Steam auth ticket received for lobby. Length=%d"), SteamTicket.Len());
	SetLobbyInitState(EFrontierLobbyInitState::AuthenticatingBackend, TEXT("서버에 Steam 로그인을 요청하는 중입니다."));

	BindBackendProtocolComponent();
	if (!BackendProtocolComponent)
	{
		HandleLobbyProcessFailed(TEXT("백엔드 통신 컴포넌트를 찾을 수 없습니다."));
		return;
	}

	BackendProtocolComponent->LoginWithSteamAuthTicket(SteamTicket);
}

void UFrontierLobbyWidget::HandleSteamTicketFailed(const FString& ErrorMessage)
{
	if (!bAuthenticationInProgress)
	{
		return;
	}

	HandleLobbyProcessFailed(ErrorMessage.IsEmpty()
		? FString(TEXT("Steam 인증 티켓 발급에 실패했습니다."))
		: ErrorMessage);
}

void UFrontierLobbyWidget::HandleBackendSteamLoginSucceeded(const FFrontierBackendSteamLoginResult& Result)
{
	if (!bAuthenticationInProgress || bLobbyInitialized)
	{
		return;
	}

	CompleteLobbyInitialization(Result);
}

void UFrontierLobbyWidget::HandleBackendCurrenciesChanged()
{
	RefreshCurrencies();
	RefreshUpgradeStoneDisplay();
}

void UFrontierLobbyWidget::HandleUpgradeStoneInventoryChanged(
	const TArray<FFrontierInventorySlot>& Slots)
{
	RefreshUpgradeStoneDisplay();
}

void UFrontierLobbyWidget::HandleLoadoutChanged(
	const TArray<FFrontierLoadoutSlot>& Slots)
{
	RefreshCurrentEquipmentScoreDisplay();
	RefreshSelectedMapInfo();
}

void UFrontierLobbyWidget::HandleBackendRequestFailed(const FString& ErrorMessage)
{
	if (bMatchmakingCancelInProgress)
	{
		bMatchmakingCancelInProgress = false;
		if (Text_MatchMakingStatus)
		{
			Text_MatchMakingStatus->SetText(FText::FromString(
				ErrorMessage.IsEmpty() ? TEXT("매치메이킹 요청에 실패했습니다.") : ErrorMessage));
		}
		RefreshMatchmakingControls();
		return;
	}
	if (bMatchmakingActive)
	{
		PendingMatchmakingFailureMessage = ErrorMessage;
		HandleMatchmakingStatusChanged(TEXT("FAILED"));
		SetStatusText(ErrorMessage.IsEmpty()
			? TEXT("매치메이킹 요청에 실패했습니다.")
			: ErrorMessage);
		return;
	}

	if (!bAuthenticationInProgress)
	{
		return;
	}

	HandleLobbyProcessFailed(ErrorMessage.IsEmpty()
		? FString(TEXT("백엔드 로그인 요청에 실패했습니다."))
		: ErrorMessage);
}

void UFrontierLobbyWidget::HandleNicknameUpdateSucceeded(const FString& Nickname)
{
	bFallbackNicknameRequestInProgress = false;
	if (WBP_ChangeNickname && !Cast<UFrontierChangeNicknameWidget>(WBP_ChangeNickname))
	{
		WBP_ChangeNickname->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (FallbackNicknameConfirmButton)
	{
		FallbackNicknameConfirmButton->SetIsEnabled(true);
	}
	FFrontierBackendSteamLoginResult UpdatedLoginResult = BackendProtocolComponent
		? BackendProtocolComponent->GetLastSteamLoginResult()
		: FFrontierBackendSteamLoginResult();
	UpdatedLoginResult.Player.Nickname = Nickname;
	RefreshLobbyPlayerInfo(UpdatedLoginResult);
	if (UFrontierSettingsWidget* SettingsWidget = Cast<UFrontierSettingsWidget>(WBP_SettingsWidget))
	{
		SettingsWidget->RefreshProfileInfo();
	}
}

void UFrontierLobbyWidget::HandleNicknameUpdateFailed(const FString& ErrorMessage)
{
	bFallbackNicknameRequestInProgress = false;
	if (FallbackNicknameConfirmButton)
	{
		FallbackNicknameConfirmButton->SetIsEnabled(true);
	}
}

void UFrontierLobbyWidget::HandleMatchmakingStatusChanged(const FString& Status)
{
	FString NormalizedStatus = Status;
	NormalizedStatus.TrimStartAndEndInline();
	NormalizedStatus.ToUpperInline();

	if (NormalizedStatus == TEXT("CANCELLED") || NormalizedStatus == TEXT("FAILED"))
	{
		const bool bFailed = NormalizedStatus == TEXT("FAILED");
		const FString FailureMessage = PendingMatchmakingFailureMessage;
		PendingMatchmakingFailureMessage.Reset();
		bMatchmakingActive = false;
		bMatchmakingCancelInProgress = false;
		bMatchmakingServerReady = false;
		if (AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwningPlayer()))
		{
			Controller->CancelRaidLoadingScreenTransition();
		}
		if (Box_MatchMaking)
		{
			Box_MatchMaking->SetVisibility(ESlateVisibility::Collapsed);
		}
		RefreshRaidControls();
		if (bFailed)
		{
			ShowMatchmakingFailurePopup(FailureMessage);
		}
		return;
	}

	bMatchmakingActive = true;
	if (NormalizedStatus == TEXT("SERVER_STARTING"))
	{
		ScheduleRaidLoadingScreen();
	}
	else if (NormalizedStatus == TEXT("SERVER_READY"))
	{
		bMatchmakingServerReady = true;
		// If SERVER_STARTING was skipped during reconnect/recovery, make sure
		// the loading screen is visible before the delayed Raid Entry travel.
		if (AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwningPlayer()))
		{
			Controller->ShowRaidLoadingScreen();
		}
	}
	if (Box_MatchMaking)
	{
		Box_MatchMaking->SetVisibility(ESlateVisibility::Visible);
	}
	if (Text_MatchMakingStatus)
	{
		FText DisplayText;
		if (NormalizedStatus == TEXT("REQUESTING"))
		{
			DisplayText = NSLOCTEXT("FrontierLobby", "MatchmakingRequesting", "매치메이킹 요청 중...");
		}
		else if (NormalizedStatus == TEXT("WAITING"))
		{
			DisplayText = NSLOCTEXT("FrontierLobby", "MatchmakingWaiting", "매치메이킹 대기 중...");
		}
		else if (NormalizedStatus == TEXT("MATCHED"))
		{
			DisplayText = NSLOCTEXT("FrontierLobby", "MatchmakingMatched", "매칭 완료");
		}
		else if (NormalizedStatus == TEXT("SERVER_STARTING"))
		{
			DisplayText = NSLOCTEXT("FrontierLobby", "MatchmakingServerStarting", "게임 서버 준비 중...");
		}
		else if (NormalizedStatus == TEXT("SERVER_READY"))
		{
			DisplayText = NSLOCTEXT("FrontierLobby", "MatchmakingServerReady", "게임 서버 접속 준비 완료");
		}
		else
		{
			DisplayText = FText::FromString(NormalizedStatus);
		}
		Text_MatchMakingStatus->SetText(DisplayText);
	}
	RefreshMatchmakingControls();
}

void UFrontierLobbyWidget::HandleLobbyLevelReady(
	const FFrontierPlayerLevelSnapshot& Level,
	const FFrontierTemporarySkillPointResult& SkillPointResult)
{
	CachedPlayerLevel = Level;
	CachedSkillPointResult = SkillPointResult;
	bHasCachedPlayerLevel = true;
	RefreshLevelDisplay();

	if (RaidSessionSubsystem)
	{
		FFrontierRaidSettlementPresentation Presentation;
		if (RaidSessionSubsystem->CompletePendingSettlementPresentation(Level, Presentation)
			&& WBP_RaidSettlement)
		{
			WBP_RaidSettlement->PlaySettlement(Presentation);
		}
	}
}

void UFrontierLobbyWidget::HandleSettlementReturnToLobbyRequested()
{
	if (WBP_RaidSettlement)
	{
		WBP_RaidSettlement->SetVisibility(ESlateVisibility::Collapsed);
	}

	SetActiveLobbyPanel(EFrontierLobbyPanel::Lobby);
	SetActiveContentPanel(EFrontierLobbyContentPanel::Lobby);
}

void UFrontierLobbyWidget::ResumeLobbyInitialization()
{
	if (bAuthenticationInProgress)
	{
		return;
	}

	BindBackendProtocolComponent();
	if (!BackendProtocolComponent)
	{
		StartLobbyInitialization();
		return;
	}

	bAuthenticationInProgress = true;
	bLobbyInitialized = false;
	SetActiveLobbyPanel(EFrontierLobbyPanel::Status);
	SetLobbyInitState(
		EFrontierLobbyInitState::AuthenticatingBackend,
		TEXT("기존 로그인 세션으로 로비 정보를 갱신하는 중입니다."));
	BackendProtocolComponent->ResumeAuthenticatedLobbySession();
}

void UFrontierLobbyWidget::StartLobbyInitialization()
{
	if (bAuthenticationInProgress)
	{
		return;
	}

	bAuthenticationInProgress = true;
	bLobbyInitialized = false;

	if (Button_Start)
	{
		Button_Start->SetIsEnabled(false);
	}
	if (Button_Retry)
	{
		Button_Retry->SetVisibility(ESlateVisibility::Collapsed);
	}

	SetActiveLobbyPanel(EFrontierLobbyPanel::Status);
	SetLobbyInitState(EFrontierLobbyInitState::CheckingSteam, TEXT("Steam 로그인 상태를 확인하는 중입니다."));

	BindSteamSubsystem();
	if (!SteamSubsystem)
	{
		HandleLobbyProcessFailed(TEXT("Steam 서비스를 확인할 수 없습니다."));
		return;
	}

	if (!SteamSubsystem->CheckSteamLogin())
	{
		HandleLobbyProcessFailed(TEXT("Steam에 로그인되어 있지 않습니다."));
		return;
	}

	const FString& SteamId = SteamSubsystem->GetCachedSteamId();
	const FString& Nickname = SteamSubsystem->GetCachedNickname();
	
	if (SteamId.IsEmpty())
	{
		HandleLobbyProcessFailed(TEXT("Steam ID를 확인할 수 없습니다."));
		return;
	}

	FRONTIER_LOG(Log, TEXT("Steam user checked for lobby. SteamId=%s Nickname=%s"), *SteamId, *Nickname);
	SetLobbyInitState(EFrontierLobbyInitState::RequestingSteamTicket, TEXT("Steam 인증 티켓을 요청하는 중입니다."));
	
	
	SteamSubsystem->RequestSteamWebApiTicket();
}

void UFrontierLobbyWidget::CompleteLobbyInitialization(const FFrontierBackendSteamLoginResult& LoginResult)
{
	bAuthenticationInProgress = false;
	bLobbyInitialized = true;

	if (Button_Start)
	{
		Button_Start->SetIsEnabled(true);
	}
	if (Button_Retry)
	{
		Button_Retry->SetVisibility(ESlateVisibility::Collapsed);
	}

	//SetLobbyInitState(EFrontierLobbyInitState::Completed, TEXT("로비 준비가 완료되었습니다."));
	RefreshLobbyPlayerInfo(LoginResult);
	if (LoginResult.bIsNewPlayer && !bInitialNicknamePromptShown)
	{
		bInitialNicknamePromptShown = true;
		ShowChangeNicknameWidget(false);
	}
	RefreshCurrencies();
	RefreshUpgradeStoneDisplay();
	if (SteamPartySubsystem)
	{
		if (!SteamPartySubsystem->PrepareSteamPartyLobby())
		{
			FRONTIER_LOG(Warning, TEXT("Lobby initialization completed, but the Steam party Lobby could not be prepared."));
		}
		SteamPartySubsystem->RefreshPartyMembers();
	}
	SetActiveLobbyPanel(EFrontierLobbyPanel::Lobby);
	RefreshRaidControls();
}

void UFrontierLobbyWidget::HandleLobbyProcessFailed(const FString& ErrorMessage)
{
	bAuthenticationInProgress = false;
	bLobbyInitialized = false;
	RefreshRaidControls();

	if (Button_Start)
	{
		Button_Start->SetIsEnabled(true);
	}
	if (Button_Retry)
	{
		Button_Retry->SetVisibility(ESlateVisibility::Visible);
	}

	SetLobbyInitState(EFrontierLobbyInitState::Failed, ErrorMessage.IsEmpty()
		? FString(TEXT("로비 초기화에 실패했습니다."))
		: ErrorMessage);
	SetActiveLobbyPanel(EFrontierLobbyPanel::Status);
}

void UFrontierLobbyWidget::SetStatusText(const FString& NewStatus)
{
	if (Text_Status)
	{
		Text_Status->SetText(FText::FromString(NewStatus));
	}
}

void UFrontierLobbyWidget::SetActiveLobbyPanel(const EFrontierLobbyPanel Panel)
{
	if (WidgetSwitcher_Lobby)
	{
		WidgetSwitcher_Lobby->SetActiveWidgetIndex(static_cast<int32>(Panel));
	}
}

void UFrontierLobbyWidget::SetActiveContentPanel(const EFrontierLobbyContentPanel Panel)
{
	if (!WidgetSwitcher_Contents)
	{
		return;
	}

	// The skill tree is now a separate full-screen viewport widget. Resolve the
	// authored upgrade panel's switcher slot so removing the old embedded skill
	// tree does not make the upgrade screen dependent on a stale numeric index.
	if (Panel == EFrontierLobbyContentPanel::Upgrade && WBP_UpgradePanel)
	{
		UWidget* SwitcherChild = WBP_UpgradePanel;
		while (SwitcherChild && SwitcherChild->GetParent()
			&& SwitcherChild->GetParent() != WidgetSwitcher_Contents)
		{
			SwitcherChild = SwitcherChild->GetParent();
		}
		if (SwitcherChild && SwitcherChild->GetParent() == WidgetSwitcher_Contents)
		{
			WidgetSwitcher_Contents->SetActiveWidget(SwitcherChild);
			return;
		}
	}

	WidgetSwitcher_Contents->SetActiveWidgetIndex(static_cast<int32>(Panel));
}

void UFrontierLobbyWidget::RefreshStoragePanel()
{
	RefreshUpgradeStoneDisplay();

	if (!WBP_LobbyInventoryWrapper)
	{
		FRONTIER_LOG(Warning, TEXT("Lobby inventory refresh failed because WBP_StorageWrapper is not bound."));
		return;
	}

	AFrontierPlayerState* PlayerState = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>()
		: nullptr;

	WBP_LobbyInventoryWrapper->RefreshFromPlayerState(PlayerState);
}

int32 UFrontierLobbyWidget::CountUpgradeStoneQuantity(
	const UFrontierInventoryComponent* InventoryComponent) const
{
	if (!InventoryComponent || UpgradeStoneItemTemplateId.IsNone())
	{
		return 0;
	}

	int32 UpgradeStoneQuantity = 0;
	for (const FFrontierInventorySlot& InventorySlot : InventoryComponent->GetSlots())
	{
		if (InventorySlot.bOccupied
			&& IsUpgradeStoneTemplateId(InventorySlot.ItemInstance.GetTemplateId()))
		{
			UpgradeStoneQuantity += FMath::Max(0, InventorySlot.ItemInstance.Quantity);
		}
	}

	return UpgradeStoneQuantity;
}

bool UFrontierLobbyWidget::IsUpgradeStoneTemplateId(const FName ItemTemplateId) const
{
	if (ItemTemplateId.IsNone())
	{
		return false;
	}

	if (ItemTemplateId == UpgradeStoneItemTemplateId)
	{
		return true;
	}

	// The catalog currently uses the typed material DataTable RowName. Keep
	// the old asset-name value compatible with existing WBP defaults.
	static const FName CanonicalUpgradeStoneId(
		TEXT("Item_Miscellaneous_UpgradeStone_NormalUpgradeStone"));
	static const FName LegacyUpgradeStoneId(TEXT("UpgradeStone"));
	const bool bConfiguredAsUpgradeStone = UpgradeStoneItemTemplateId == CanonicalUpgradeStoneId
		|| UpgradeStoneItemTemplateId == LegacyUpgradeStoneId;
	return bConfiguredAsUpgradeStone
		&& (ItemTemplateId == CanonicalUpgradeStoneId || ItemTemplateId == LegacyUpgradeStoneId);
}

void UFrontierLobbyWidget::BindUpgradeStoneInventoryEvents()
{
	const AFrontierPlayerState* PlayerState = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>()
		: nullptr;
	if (!PlayerState)
	{
		return;
	}

	if (UFrontierRaidInventoryComponent* Inventory = PlayerState->GetRaidInventoryComponent())
	{
		Inventory->OnInventoryChanged.AddUniqueDynamic(
			this,
			&UFrontierLobbyWidget::HandleUpgradeStoneInventoryChanged);
	}
	if (UFrontierStorageComponent* Storage = PlayerState->GetStorageComponent())
	{
		Storage->OnInventoryChanged.AddUniqueDynamic(
			this,
			&UFrontierLobbyWidget::HandleUpgradeStoneInventoryChanged);
	}
}

void UFrontierLobbyWidget::UnbindUpgradeStoneInventoryEvents()
{
	const AFrontierPlayerState* PlayerState = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>()
		: nullptr;
	if (!PlayerState)
	{
		return;
	}

	if (UFrontierRaidInventoryComponent* Inventory = PlayerState->GetRaidInventoryComponent())
	{
		Inventory->OnInventoryChanged.RemoveDynamic(
			this,
			&UFrontierLobbyWidget::HandleUpgradeStoneInventoryChanged);
	}
	if (UFrontierStorageComponent* Storage = PlayerState->GetStorageComponent())
	{
		Storage->OnInventoryChanged.RemoveDynamic(
			this,
			&UFrontierLobbyWidget::HandleUpgradeStoneInventoryChanged);
	}
}

UTextBlock* UFrontierLobbyWidget::ResolveUpgradeStoneText()
{
	if (Text_PlayerUpgradeStone)
	{
		return Text_PlayerUpgradeStone;
	}

	// Supports a map-select UserWidget nested inside the lobby WBP.
	if (UUserWidget* MapSelectWidget = Cast<UUserWidget>(MapSelectPanel))
	{
		Text_PlayerUpgradeStone = Cast<UTextBlock>(
			MapSelectWidget->GetWidgetFromName(TEXT("Text_PlayerUpgradeStone")));
	}

	if (!Text_PlayerUpgradeStone)
	{
		Text_PlayerUpgradeStone = Cast<UTextBlock>(
			GetWidgetFromName(TEXT("Text_PlayerUpgradeStone")));
	}

	return Text_PlayerUpgradeStone;
}

void UFrontierLobbyWidget::RefreshUpgradeStoneDisplay()
{
	UTextBlock* UpgradeStoneText = ResolveUpgradeStoneText();
	if (!UpgradeStoneText)
	{
		return;
	}
	BindUpgradeStoneInventoryEvents();

	const AFrontierPlayerState* PlayerState = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>()
		: nullptr;

	int32 TotalUpgradeStoneQuantity = 0;
	if (PlayerState)
	{
		TotalUpgradeStoneQuantity += CountUpgradeStoneQuantity(
			PlayerState->GetRaidInventoryComponent());
		TotalUpgradeStoneQuantity += CountUpgradeStoneQuantity(
			PlayerState->GetStorageComponent());
	}

	UpgradeStoneText->SetText(FText::AsNumber(TotalUpgradeStoneQuantity));
}

void UFrontierLobbyWidget::RefreshCurrencies()
{
	if (!BackendProtocolComponent || !WBP_GoldCurrencySlot)
	{
		return;
	}

	for (const FFrontierBackendCurrencyInfo& CurrencyInfo : BackendProtocolComponent->GetLastCurrencies())
	{
		if (CurrencyInfo.CurrencyCode.Equals(TEXT("GOLD"), ESearchCase::IgnoreCase))
		{
			WBP_GoldCurrencySlot->SetCurrency(ECurrencyType::GOLD, CurrencyInfo.Balance);
			return;
		}
	}
}

void UFrontierLobbyWidget::RefreshLevelDisplay()
{
	if (!bHasCachedPlayerLevel)
	{
		return;
	}
	if (Text_PlayerLevel)
	{
		Text_PlayerLevel->SetText(FText::Format(NSLOCTEXT("FrontierLobby", "LevelFormat", "{0}"), FText::AsNumber(CachedPlayerLevel.Level)));
	}
	if (Text_CurrentLevelExperience)
	{
		Text_CurrentLevelExperience->SetText(FText::AsNumber(CachedPlayerLevel.CurrentLevelExperience));
	}
	if (Text_NextLevelExperience)
	{
		Text_NextLevelExperience->SetText(CachedPlayerLevel.bHasNextLevelRequiredExperience
			? FText::AsNumber(CachedPlayerLevel.NextLevelRequiredExperience)
			: NSLOCTEXT("FrontierLobby", "MaxLevel", "MAX"));
	}
	if (Text_TemporarySkillPoints)
	{
		Text_TemporarySkillPoints->SetText(FText::AsNumber(CachedSkillPointResult.TemporarySkillPoints));
	}
	if (ProgressBar_LevelExperience)
	{
		ProgressBar_LevelExperience->SetPercent(CachedPlayerLevel.GetProgress());
	}
}

void UFrontierLobbyWidget::SetLobbyInitState(const EFrontierLobbyInitState NewState, const FString& StatusMessage)
{
	CurrentInitState = NewState;
	SetStatusText(StatusMessage);
}

void UFrontierLobbyWidget::RefreshLobbyPlayerInfo(const FFrontierBackendSteamLoginResult& LoginResult)
{
	if (Text_PlayerName)
	{
		const FString PlayerName = LoginResult.Player.Nickname.IsEmpty() ? TEXT("Unknown Player") : LoginResult.Player.Nickname;
		const bool bHasCompleteDedicatedLevelDisplay = Text_PlayerLevel
			&& Text_CurrentLevelExperience
			&& Text_NextLevelExperience
			&& Text_TemporarySkillPoints
			&& ProgressBar_LevelExperience;
		FString PlayerDisplayName = PlayerName;
		if (!bHasCompleteDedicatedLevelDisplay && bHasCachedPlayerLevel)
		{
			const FString NextExperience = CachedPlayerLevel.bHasNextLevelRequiredExperience
				? LexToString(CachedPlayerLevel.NextLevelRequiredExperience)
				: TEXT("MAX");
			PlayerDisplayName = FString::Printf(
				TEXT("%s"),
				*PlayerName);
		}
		Text_PlayerName->SetText(FText::FromString(PlayerDisplayName));
	}
	RefreshLevelDisplay();
}

void UFrontierLobbyWidget::BindRaidIntegration()
{
	RaidSessionSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr;
	if (RaidSessionSubsystem)
	{
		RaidSessionSubsystem->OnRaidFlowStateChanged.RemoveDynamic(
			this,
			&UFrontierLobbyWidget::HandleRaidFlowStateChanged);
		RaidSessionSubsystem->OnRaidFlowStateChanged.AddUniqueDynamic(
			this,
			&UFrontierLobbyWidget::HandleRaidFlowStateChanged);
	}
}

void UFrontierLobbyWidget::UnbindRaidIntegration()
{
	if (RaidSessionSubsystem)
	{
		RaidSessionSubsystem->OnRaidFlowStateChanged.RemoveDynamic(
			this,
			&UFrontierLobbyWidget::HandleRaidFlowStateChanged);
		RaidSessionSubsystem = nullptr;
	}
}

void UFrontierLobbyWidget::RefreshRaidControls()
{
	const EFrontierRaidFlowState State = RaidSessionSubsystem
		? RaidSessionSubsystem->GetClientState()
		: EFrontierRaidFlowState::Idle;
	const UFrontierSteamPartySubsystem* SteamParty = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierSteamPartySubsystem>()
		: nullptr;
	const bool bHasOtherPartyMembers = SteamParty && SteamParty->HasOtherPartyMembers();
	const bool bCanStartPartyMatchmaking = !bHasOtherPartyMembers
		|| SteamParty->IsLocalPartyLeader();
	const bool bCanStartRaid = bCanStartPartyMatchmaking
		&& bLobbyInitialized
		&& !bMatchmakingActive
		&& (State == EFrontierRaidFlowState::Idle
			|| State == EFrontierRaidFlowState::Completed
			|| State == EFrontierRaidFlowState::RetryableFailure);
	if (Button_MapSelect)
	{
		Button_MapSelect->SetIsEnabled(bCanStartRaid);
	}
	RefreshRaidEntryButtonState();
	RefreshMatchmakingControls();
}

void UFrontierLobbyWidget::RefreshMatchmakingControls()
{
	const UFrontierSteamPartySubsystem* SteamParty = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierSteamPartySubsystem>()
		: nullptr;
	const bool bHasOtherPartyMembers = SteamParty && SteamParty->HasOtherPartyMembers();
	const bool bCanStartPartyMatchmaking = !bHasOtherPartyMembers
		|| SteamParty->IsLocalPartyLeader();
	const bool bCanSelectMap = bCanStartPartyMatchmaking && !bMatchmakingActive;
	if (Button_MapSelect)
	{
		Button_MapSelect->SetIsEnabled(bCanSelectMap);
	}
	if (Button_CancelMatchMaking)
	{
		Button_CancelMatchMaking->SetIsEnabled(
			bMatchmakingActive
			&& !bMatchmakingCancelInProgress
			&& !bMatchmakingServerReady);
	}
	bInitialNicknamePromptShown = false;
}

void UFrontierLobbyWidget::ScheduleRaidLoadingScreen()
{
	if (AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwningPlayer()))
	{
		Controller->ShowRaidLoadingScreenAfterDelay(0.0f);
	}
}

void UFrontierLobbyWidget::ShowMatchmakingFailurePopup(const FString& ErrorMessage)
{
	UFrontierPopupSubsystem* PopupSubsystem = UFrontierPopupSubsystem::Get(this);
	if (!PopupSubsystem)
	{
		FRONTIER_LOG(Warning, TEXT("[Matchmaking] Could not show failure popup because PopupSubsystem is unavailable."));
		return;
	}

	const FText Message = ErrorMessage.IsEmpty()
		? NSLOCTEXT(
			"FrontierLobby",
			"MatchmakingServerPreparationFailedMessage",
			"게임 서버 준비에 실패했습니다. 다시 시도해 주세요.")
		: FText::FromString(ErrorMessage);
	PopupSubsystem->ShowMessage(
		EFrontierPopupType::Error,
		NSLOCTEXT("FrontierLobby", "MatchmakingServerPreparationFailedTitle", "게임 서버 준비 실패"),
		Message);
}

void UFrontierLobbyWidget::BindMapSelectButtons()
{
	// Resolve legacy WBP names as a fallback. BindWidgetOptional leaves these
	// pointers null when an existing asset still uses the older naming.
	if (!MapSelectPanel)
	{
		MapSelectPanel = GetWidgetFromName(TEXT("MapSelectPanel"));
		if (!MapSelectPanel)
		{
			MapSelectPanel = GetWidgetFromName(TEXT("MapSelectionPanel"));
		}
	}
	if (!Button_RaidEntry)
	{
		Button_RaidEntry = Cast<UButton>(GetWidgetFromName(TEXT("Button_RaidEntry")));
		if (!Button_RaidEntry)
		{
			Button_RaidEntry = Cast<UButton>(GetWidgetFromName(TEXT("RaidEntryButton")));
		}
	}
	if (!Button_RaidButton)
	{
		Button_RaidButton = Cast<UButton>(GetWidgetFromName(TEXT("Button_RaidButton")));
	}
	if (!MapInfoPanel)
	{
		MapInfoPanel = GetWidgetFromName(TEXT("MapInfoPanel"));
	}
	if (!Image_MapImage)
	{
		Image_MapImage = Cast<UImage>(GetWidgetFromName(TEXT("Image_MapImage")));
	}
	if (!Text_SelectedMapName)
	{
		Text_SelectedMapName = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_SelectedMapName")));
	}
	if (!Text_MinTotalEquipmentScore)
	{
		Text_MinTotalEquipmentScore = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_MinTotalEquipmentScore")));
	}
	if (!Text_MaxTotalEquipmentScore)
	{
		Text_MaxTotalEquipmentScore = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_MaxTotalEquipmentScore")));
	}
	if (!Text_LimitEquipmentScore)
	{
		Text_LimitEquipmentScore = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_LimitEquipmentScore")));
	}
	if (!Text_CurrentEquimentScore)
	{
		Text_CurrentEquimentScore = Cast<UTextBlock>(
			GetWidgetFromName(TEXT("Text_CurrentEquimentScore")));
	}
	if (!WBP_RaidEntryWarning)
	{
		WBP_RaidEntryWarning = Cast<UFrontierRaidEntryWarningWidget>(
			GetWidgetFromName(TEXT("WBP_RaidEntryWarning")));
	}
	if (Button_L_Dungeon && Button_L_Dungeon->GetMapId().IsNone())
	{
		Button_L_Dungeon->SetMapId(MedievalDungeonID);
	}
	if (Button_L_Forest && Button_L_Forest->GetMapId().IsNone())
	{
		Button_L_Forest->SetMapId(DarkForestID);
	}

	// Button_StartRaid is the legacy widget name. It now opens map selection,
	// so existing WBP_LobbyWidget assets continue to work without a hard break.
	if (Button_MapSelect)
	{
		Button_MapSelect->OnClicked.RemoveAll(this);
		Button_MapSelect->OnClicked.AddDynamic(this, &UFrontierLobbyWidget::HandleMapSelectButtonClicked);
	}
	if (Button_L_Dungeon)
	{
		Button_L_Dungeon->OnMapIconClicked.RemoveDynamic(
			this,
			&UFrontierLobbyWidget::HandleDungeonMapButtonClicked);
		Button_L_Dungeon->OnMapIconClicked.AddUniqueDynamic(
			this,
			&UFrontierLobbyWidget::HandleDungeonMapButtonClicked);
	}
	if (Button_L_Forest)
	{
		Button_L_Forest->OnMapIconClicked.RemoveDynamic(
			this,
			&UFrontierLobbyWidget::HandleForestMapButtonClicked);
		Button_L_Forest->OnMapIconClicked.AddUniqueDynamic(
			this,
			&UFrontierLobbyWidget::HandleForestMapButtonClicked);
	}
	if (UButton* RaidButton = ResolveRaidEntryButton())
	{
		RaidButton->OnClicked.RemoveAll(this);
		RaidButton->OnClicked.AddUniqueDynamic(
			this,
			&UFrontierLobbyWidget::HandleRaidEntryButtonClicked);
		RefreshRaidEntryButtonState();
	}
	if (WBP_RaidEntryWarning)
	{
		WBP_RaidEntryWarning->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Button_MapSelectClose)
	{
		Button_MapSelectClose->OnClicked.RemoveAll(this);
		Button_MapSelectClose->OnClicked.AddDynamic(
			this,
			&UFrontierLobbyWidget::HandleMapSelectCloseButtonClicked);
	}
}

void UFrontierLobbyWidget::UnbindMapSelectButtons()
{
	if (Button_MapSelect)
	{
		Button_MapSelect->OnClicked.RemoveAll(this);
	}
	if (Button_L_Dungeon)
	{
		Button_L_Dungeon->OnMapIconClicked.RemoveDynamic(
			this,
			&UFrontierLobbyWidget::HandleDungeonMapButtonClicked);
	}
	if (Button_L_Forest)
	{
		Button_L_Forest->OnMapIconClicked.RemoveDynamic(
			this,
			&UFrontierLobbyWidget::HandleForestMapButtonClicked);
	}
	if (UButton* RaidButton = ResolveRaidEntryButton())
	{
		RaidButton->OnClicked.RemoveAll(this);
	}
	if (Button_MapSelectClose)
	{
		Button_MapSelectClose->OnClicked.RemoveAll(this);
	}
}

UButton* UFrontierLobbyWidget::ResolveRaidEntryButton()
{
	if (Button_RaidButton)
	{
		return Button_RaidButton;
	}

	return Button_RaidEntry;
}

float UFrontierLobbyWidget::GetTotalEquipmentScore() const
{
	const AFrontierPlayerState* PlayerState = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>()
		: nullptr;
	const UFrontierLoadoutComponent* Loadout = PlayerState
		? PlayerState->GetLoadoutComponent()
		: nullptr;
	return Loadout ? Loadout->GetTotalEquipmentScore() : 0.0f;
}

float UFrontierLobbyWidget::GetEquipmentScoreForItem(
	const FFrontierItemInstance& ItemInstance) const
{
	if (!ItemInstance.IsValid())
	{
		return 0.0f;
	}

	FFrontierItemInstance ScoredItem = ItemInstance;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UFrontierUpgradeBalanceSubsystem* UpgradeBalance =
			GameInstance->GetSubsystem<UFrontierUpgradeBalanceSubsystem>())
		{
			UpgradeBalance->RecalculateEquipmentScore(ScoredItem);
		}
	}

	return FMath::Max(0.0f, ScoredItem.EquipmentScore);
}

void UFrontierLobbyWidget::RefreshCurrentEquipmentScoreDisplay()
{
	if (Text_CurrentEquimentScore)
	{
		Text_CurrentEquimentScore->SetText(FText::FromString(FString::Printf(
			TEXT("%.1f"),
			FMath::Max(0.0f, GetTotalEquipmentScore()))));
	}
}

void UFrontierLobbyWidget::RefreshSelectedMapInfo()
{
	RefreshCurrentEquipmentScoreDisplay();
	UButton* RaidButton = ResolveRaidEntryButton();
	if (SelectedRaidMapName.IsNone())
	{
		if (MapInfoPanel)
		{
			MapInfoPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (RaidButton)
		{
			RaidButton->SetIsEnabled(false);
		}
		if (Text_CurrentEquimentScore)
		{
			Text_CurrentEquimentScore->SetColorAndOpacity(FSlateColor::UseForeground());
		}
		if (Text_MinTotalEquipmentScore)
		{
			Text_MinTotalEquipmentScore->SetColorAndOpacity(FSlateColor::UseForeground());
		}
		if (Text_MaxTotalEquipmentScore)
		{
			Text_MaxTotalEquipmentScore->SetColorAndOpacity(FSlateColor::UseForeground());
		}
		return;
	}

	UDataTable* MapInfoTable = MapInfoDataTable.LoadSynchronous();
	const FFrontierMapInfoTableRow* MapInfo = MapInfoTable
		? MapInfoTable->FindRow<FFrontierMapInfoTableRow>(
			SelectedRaidMapName,
			TEXT("RefreshSelectedMapInfo"),
			false)
		: nullptr;
	if (!MapInfo)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Map info is unavailable. MapId=%s DataTable=%s"),
			*SelectedRaidMapName.ToString(),
			*GetNameSafe(MapInfoTable));
		if (MapInfoPanel)
		{
			MapInfoPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (RaidButton)
		{
			RaidButton->SetIsEnabled(false);
		}
		if (Text_CurrentEquimentScore)
		{
			Text_CurrentEquimentScore->SetColorAndOpacity(FSlateColor::UseForeground());
		}
		if (Text_MinTotalEquipmentScore)
		{
			Text_MinTotalEquipmentScore->SetColorAndOpacity(FSlateColor::UseForeground());
		}
		if (Text_MaxTotalEquipmentScore)
		{
			Text_MaxTotalEquipmentScore->SetColorAndOpacity(FSlateColor::UseForeground());
		}
		return;
	}

	if (MapInfoPanel)
	{
		MapInfoPanel->SetVisibility(ESlateVisibility::Visible);
	}
	if (Text_SelectedMapName)
	{
		Text_SelectedMapName->SetText(MapInfo->MapName);
	}
	if (Image_MapImage)
	{
		Image_MapImage->SetBrushFromTexture(MapInfo->MapImage.LoadSynchronous(), true);
	}
	if (Text_MinTotalEquipmentScore)
	{
		Text_MinTotalEquipmentScore->SetText(FText::FromString(FString::Printf(
			TEXT("%.1f"),
			FMath::Max(0.0f, MapInfo->MinTotalEquipmentScore))));
		Text_MinTotalEquipmentScore->SetColorAndOpacity(
			GetTotalEquipmentScore() < MapInfo->MinTotalEquipmentScore
				? FSlateColor(FLinearColor::Red)
				: FSlateColor::UseForeground());
	}
	if (Text_MaxTotalEquipmentScore)
	{
		Text_MaxTotalEquipmentScore->SetText(FText::FromString(FString::Printf(
			TEXT("%.1f"),
			FMath::Max(0.0f, MapInfo->MaxTotalEquipmentScore))));
		Text_MaxTotalEquipmentScore->SetColorAndOpacity(
			GetTotalEquipmentScore() > MapInfo->MaxTotalEquipmentScore
				? FSlateColor(FLinearColor::Red)
				: FSlateColor::UseForeground());
	}
	if (Text_LimitEquipmentScore)
	{
		Text_LimitEquipmentScore->SetText(FText::FromString(FString::Printf(
			TEXT("%.1f"),
			FMath::Max(0.0f, MapInfo->LimitEquipmentScore))));
	}
	if (Text_CurrentEquimentScore)
	{
		Text_CurrentEquimentScore->SetColorAndOpacity(FSlateColor::UseForeground());
	}

	RefreshRaidEntryButtonState();
}

void UFrontierLobbyWidget::BindLoadoutEvents()
{
	const AFrontierPlayerState* PlayerState = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>()
		: nullptr;
	if (!PlayerState)
	{
		return;
	}

	if (UFrontierLoadoutComponent* Loadout = PlayerState->GetLoadoutComponent())
	{
		Loadout->OnLoadoutChanged.AddUniqueDynamic(
			this,
			&UFrontierLobbyWidget::HandleLoadoutChanged);
	}
}

void UFrontierLobbyWidget::UnbindLoadoutEvents()
{
	const AFrontierPlayerState* PlayerState = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>()
		: nullptr;
	if (!PlayerState)
	{
		return;
	}

	if (UFrontierLoadoutComponent* Loadout = PlayerState->GetLoadoutComponent())
	{
		Loadout->OnLoadoutChanged.RemoveDynamic(
			this,
			&UFrontierLobbyWidget::HandleLoadoutChanged);
	}
}

void UFrontierLobbyWidget::RefreshRaidEntryButtonState()
{
	UButton* RaidButton = ResolveRaidEntryButton();
	if (!RaidButton)
	{
		return;
	}

	bool bScoreInAllowedRange = false;
	if (!SelectedRaidMapName.IsNone())
	{
		if (UDataTable* MapInfoTable = MapInfoDataTable.LoadSynchronous())
		{
			if (const FFrontierMapInfoTableRow* MapInfo = MapInfoTable->FindRow<FFrontierMapInfoTableRow>(
				SelectedRaidMapName,
				TEXT("RefreshRaidEntryButtonState"),
				false))
			{
				const float TotalEquipmentScore = GetTotalEquipmentScore();
				bScoreInAllowedRange = TotalEquipmentScore >= MapInfo->MinTotalEquipmentScore
					&& TotalEquipmentScore < MapInfo->MaxTotalEquipmentScore;
			}
		}
	}

	const EFrontierRaidFlowState State = RaidSessionSubsystem
		? RaidSessionSubsystem->GetClientState()
		: EFrontierRaidFlowState::Idle;
	const UFrontierSteamPartySubsystem* SteamParty = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierSteamPartySubsystem>()
		: nullptr;
	const bool bHasOtherPartyMembers = SteamParty && SteamParty->HasOtherPartyMembers();
	const bool bCanStartPartyMatchmaking = !bHasOtherPartyMembers
		|| SteamParty->IsLocalPartyLeader();
	const bool bCanStartRaid = bCanStartPartyMatchmaking
		&& bLobbyInitialized
		&& !bMatchmakingActive
		&& (State == EFrontierRaidFlowState::Idle
			|| State == EFrontierRaidFlowState::Completed
			|| State == EFrontierRaidFlowState::RetryableFailure);

	RaidButton->SetIsEnabled(bCanStartRaid && bScoreInAllowedRange);
}

void UFrontierLobbyWidget::CollectLimitedEquipmentSlots(
	const float LimitEquipmentScore,
	TArray<FFrontierInventorySlot>& OutSlots) const
{
	OutSlots.Reset();
	TSet<FGuid> AddedItemIds;
	const AFrontierPlayerState* PlayerState = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>()
		: nullptr;
	if (!PlayerState)
	{
		return;
	}

	if (const UFrontierLoadoutComponent* Loadout = PlayerState->GetLoadoutComponent())
	{
		for (const FFrontierLoadoutSlot& LoadoutSlot : Loadout->GetLoadoutSlots())
		{
			const FFrontierItemInstance& Item = LoadoutSlot.ItemInstance;
			if (!LoadoutSlot.bOccupied
				|| !Item.IsValid()
				|| GetEquipmentScoreForItem(Item) <= LimitEquipmentScore
				|| (Item.ItemInstanceId.IsValid() && AddedItemIds.Contains(Item.ItemInstanceId)))
			{
				continue;
			}

			FFrontierInventorySlot DisplaySlot;
			DisplaySlot.SlotIndex = INDEX_NONE;
			DisplaySlot.bOccupied = true;
			DisplaySlot.ItemInstance = Item;
			OutSlots.Add(DisplaySlot);
			if (Item.ItemInstanceId.IsValid())
			{
				AddedItemIds.Add(Item.ItemInstanceId);
			}
		}
	}

	if (const UFrontierRaidInventoryComponent* Inventory = PlayerState->GetRaidInventoryComponent())
	{
		for (const FFrontierInventorySlot& InventorySlot : Inventory->GetSlots())
		{
			const FFrontierItemInstance& Item = InventorySlot.ItemInstance;
			if (!InventorySlot.bOccupied
				|| !Item.IsValid()
				|| GetEquipmentScoreForItem(Item) <= LimitEquipmentScore
				|| (Item.ItemInstanceId.IsValid() && AddedItemIds.Contains(Item.ItemInstanceId)))
			{
				continue;
			}

			OutSlots.Add(InventorySlot);
			if (Item.ItemInstanceId.IsValid())
			{
				AddedItemIds.Add(Item.ItemInstanceId);
			}
		}
	}
}

bool UFrontierLobbyWidget::ShowRaidEntryWarningIfNeeded()
{
	UDataTable* MapInfoTable = MapInfoDataTable.LoadSynchronous();
	const FFrontierMapInfoTableRow* MapInfo = MapInfoTable
		? MapInfoTable->FindRow<FFrontierMapInfoTableRow>(
			SelectedRaidMapName,
			TEXT("ShowRaidEntryWarningIfNeeded"),
			false)
		: nullptr;
	if (!MapInfo)
	{
		return false;
	}

	TArray<FFrontierInventorySlot> LimitedSlots;
	CollectLimitedEquipmentSlots(MapInfo->LimitEquipmentScore, LimitedSlots);
	if (LimitedSlots.IsEmpty())
	{
		return false;
	}

	if (!WBP_RaidEntryWarning && RaidEntryWarningWidgetClass)
	{
		WBP_RaidEntryWarning = CreateWidget<UFrontierRaidEntryWarningWidget>(
			this,
			RaidEntryWarningWidgetClass);
		if (WBP_RaidEntryWarning)
		{
			bRaidEntryWarningCreatedAtRuntime = true;
			WBP_RaidEntryWarning->AddToViewport();
		}
	}

	if (!WBP_RaidEntryWarning)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Raid entry warning widget is not bound or configured. LimitedItemCount=%d"),
			LimitedSlots.Num());
		return true;
	}

	if (WBP_RaidEntryWarning)
	{
		if (WBP_LobbyInventoryWrapper)
		{
			WBP_RaidEntryWarning->SetOwningInventoryWidget(
				WBP_LobbyInventoryWrapper->GetPlayerInventoryWidget());
		}
		WBP_RaidEntryWarning->ShowLimitedItems(LimitedSlots);
	}
	return true;
}

void UFrontierLobbyWidget::HideRaidEntryWarning()
{
	if (WBP_RaidEntryWarning)
	{
		WBP_RaidEntryWarning->HideWarning();
	}
}

void UFrontierLobbyWidget::SelectRaidMap(const FName MapName)
{
	SelectedRaidMapName = MapName;
	if (Button_L_Dungeon)
	{
		Button_L_Dungeon->SetSelected(
			!MapName.IsNone() && MapName == Button_L_Dungeon->GetMapId());
	}
	if (Button_L_Forest)
	{
		Button_L_Forest->SetSelected(
			!MapName.IsNone() && MapName == Button_L_Forest->GetMapId());
	}
	RefreshSelectedMapInfo();
	RefreshRaidControls();
}

void UFrontierLobbyWidget::SetMapSelectPanelVisible(const bool bVisible)
{
	if (bVisible)
	{
		RefreshUpgradeStoneDisplay();
		RefreshSelectedMapInfo();
	}
	else
	{
		HideRaidEntryWarning();
	}

	UWidget* ResolvedPanel = MapSelectPanel;
	if (!ResolvedPanel)
	{
		// Keep the C++ binding tolerant of older WBP assets that used the
		// alternate name before MapSelectPanel became the canonical one.
		ResolvedPanel = GetWidgetFromName(TEXT("MapSelectPanel"));
		if (!ResolvedPanel)
		{
			ResolvedPanel = GetWidgetFromName(TEXT("MapSelectionPanel"));
		}
	}

	if (ResolvedPanel)
	{
		ResolvedPanel->SetVisibility(
			bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UFrontierLobbyWidget::BindSteamSubsystem()
{
	if (!SteamSubsystem)
	{
		SteamSubsystem = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UFrontierSteamSubsystem>()
			: nullptr;
	}

	if (!SteamSubsystem)
	{
		return;
	}

	SteamSubsystem->OnSteamAuthTicketReceived.RemoveDynamic(this, &UFrontierLobbyWidget::HandleSteamTicketReceived);
	SteamSubsystem->OnSteamAuthTicketFailed.RemoveDynamic(this, &UFrontierLobbyWidget::HandleSteamTicketFailed);
	SteamSubsystem->OnSteamAuthTicketReceived.AddUniqueDynamic(this, &UFrontierLobbyWidget::HandleSteamTicketReceived);
	SteamSubsystem->OnSteamAuthTicketFailed.AddUniqueDynamic(this, &UFrontierLobbyWidget::HandleSteamTicketFailed);
}

void UFrontierLobbyWidget::UnbindSteamSubsystem()
{
	if (!SteamSubsystem)
	{
		return;
	}

	SteamSubsystem->OnSteamAuthTicketReceived.RemoveDynamic(this, &UFrontierLobbyWidget::HandleSteamTicketReceived);
	SteamSubsystem->OnSteamAuthTicketFailed.RemoveDynamic(this, &UFrontierLobbyWidget::HandleSteamTicketFailed);
}

void UFrontierLobbyWidget::BindSteamPartySubsystem()
{
	if (!SteamPartySubsystem)
	{
		SteamPartySubsystem = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UFrontierSteamPartySubsystem>()
			: nullptr;
	}

	if (!SteamPartySubsystem)
	{
		return;
	}

	SteamPartySubsystem->OnPartyMembersChanged.RemoveDynamic(
		this,
		&ThisClass::HandleSteamPartyMembersChanged);
	SteamPartySubsystem->OnPartyMembersChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleSteamPartyMembersChanged);
	SteamPartySubsystem->RefreshPartyMembers();
}

void UFrontierLobbyWidget::UnbindSteamPartySubsystem()
{
	if (!SteamPartySubsystem)
	{
		return;
	}

	SteamPartySubsystem->OnPartyMembersChanged.RemoveDynamic(
		this,
		&ThisClass::HandleSteamPartyMembersChanged);
	SteamPartySubsystem = nullptr;
}

void UFrontierLobbyWidget::RefreshSteamPartyDisplay()
{
	const TArray<FFrontierSteamPartyMember>* Members = SteamPartySubsystem
		? &SteamPartySubsystem->GetPartyMembers()
		: nullptr;
	const FFrontierSteamPartyMember* LocalMember = Members
		? Members->FindByPredicate([](const FFrontierSteamPartyMember& Member)
		{
			return Member.bIsLocalPlayer;
		})
		: nullptr;

	if (Image_MyProfileImage && LocalMember && LocalMember->AvatarTexture)
	{
		Image_MyProfileImage->SetBrushFromTexture(LocalMember->AvatarTexture, true);
		Image_MyProfileImage->SetVisibility(ESlateVisibility::Visible);
	}

	TArray<const FFrontierSteamPartyMember*> RemoteMembers;
	if (Members)
	{
		for (const FFrontierSteamPartyMember& Member : *Members)
		{
			if (!Member.bIsLocalPlayer)
			{
				RemoteMembers.Add(&Member);
			}
		}
	}

	const auto ApplyRemotePartySlot = [](
		UImage* TeamImage,
		UButton* InviteButton,
		const FFrontierSteamPartyMember* Member)
	{
		if (TeamImage)
		{
			if (Member)
			{
				if (Member->AvatarTexture)
				{
					TeamImage->SetBrushFromTexture(Member->AvatarTexture, true);
				}
				TeamImage->SetVisibility(ESlateVisibility::Visible);
			}
			else
			{
				TeamImage->SetBrushFromTexture(nullptr);
				TeamImage->SetVisibility(ESlateVisibility::Collapsed);
			}
		}

		if (InviteButton)
		{
			InviteButton->SetVisibility(Member
				? ESlateVisibility::Collapsed
				: ESlateVisibility::Visible);
		}
	};

	ApplyRemotePartySlot(
		Image_TeamProfileImage1,
		Button_InviteFriend1,
		RemoteMembers.IsValidIndex(0) ? RemoteMembers[0] : nullptr);
	ApplyRemotePartySlot(
		Image_TeamProfileImage2,
		Button_InviteFriend2,
		RemoteMembers.IsValidIndex(1) ? RemoteMembers[1] : nullptr);
}

void UFrontierLobbyWidget::BindBackendProtocolComponent()
{
	if (!BackendProtocolComponent)
	{
		const AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(GetOwningPlayer());
		BackendProtocolComponent = LobbyController ? LobbyController->GetBackendProtocolComponent() : nullptr;
	}

	if (!BackendProtocolComponent)
	{
		return;
	}

	BackendProtocolComponent->OnLobbyBackendDataReady.RemoveDynamic(this, &UFrontierLobbyWidget::HandleBackendSteamLoginSucceeded);
	BackendProtocolComponent->OnCurrenciesChanged.RemoveDynamic(this, &UFrontierLobbyWidget::HandleBackendCurrenciesChanged);
	BackendProtocolComponent->OnBackendRequestFailed.RemoveDynamic(this, &UFrontierLobbyWidget::HandleBackendRequestFailed);
	BackendProtocolComponent->OnNicknameUpdateSucceeded.RemoveDynamic(this, &UFrontierLobbyWidget::HandleNicknameUpdateSucceeded);
	BackendProtocolComponent->OnNicknameUpdateFailed.RemoveDynamic(this, &UFrontierLobbyWidget::HandleNicknameUpdateFailed);
	BackendProtocolComponent->OnMatchmakingStatusChanged.RemoveDynamic(this, &UFrontierLobbyWidget::HandleMatchmakingStatusChanged);
	BackendProtocolComponent->OnLobbyLevelReady.RemoveDynamic(this, &UFrontierLobbyWidget::HandleLobbyLevelReady);
	BackendProtocolComponent->OnLobbyBackendDataReady.AddUniqueDynamic(this, &UFrontierLobbyWidget::HandleBackendSteamLoginSucceeded);
	BackendProtocolComponent->OnCurrenciesChanged.AddUniqueDynamic(this, &UFrontierLobbyWidget::HandleBackendCurrenciesChanged);
	BackendProtocolComponent->OnBackendRequestFailed.AddUniqueDynamic(this, &UFrontierLobbyWidget::HandleBackendRequestFailed);
	BackendProtocolComponent->OnNicknameUpdateSucceeded.AddUniqueDynamic(this, &UFrontierLobbyWidget::HandleNicknameUpdateSucceeded);
	BackendProtocolComponent->OnNicknameUpdateFailed.AddUniqueDynamic(this, &UFrontierLobbyWidget::HandleNicknameUpdateFailed);
	BackendProtocolComponent->OnMatchmakingStatusChanged.AddUniqueDynamic(this, &UFrontierLobbyWidget::HandleMatchmakingStatusChanged);
	BackendProtocolComponent->OnLobbyLevelReady.AddUniqueDynamic(this, &UFrontierLobbyWidget::HandleLobbyLevelReady);
}

void UFrontierLobbyWidget::UnbindBackendProtocolComponent()
{
	if (!BackendProtocolComponent)
	{
		return;
	}

	BackendProtocolComponent->OnLobbyBackendDataReady.RemoveDynamic(this, &UFrontierLobbyWidget::HandleBackendSteamLoginSucceeded);
	BackendProtocolComponent->OnCurrenciesChanged.RemoveDynamic(this, &UFrontierLobbyWidget::HandleBackendCurrenciesChanged);
	BackendProtocolComponent->OnBackendRequestFailed.RemoveDynamic(this, &UFrontierLobbyWidget::HandleBackendRequestFailed);
	BackendProtocolComponent->OnNicknameUpdateSucceeded.RemoveDynamic(this, &UFrontierLobbyWidget::HandleNicknameUpdateSucceeded);
	BackendProtocolComponent->OnNicknameUpdateFailed.RemoveDynamic(this, &UFrontierLobbyWidget::HandleNicknameUpdateFailed);
	BackendProtocolComponent->OnMatchmakingStatusChanged.RemoveDynamic(this, &UFrontierLobbyWidget::HandleMatchmakingStatusChanged);
	BackendProtocolComponent->OnLobbyLevelReady.RemoveDynamic(this, &UFrontierLobbyWidget::HandleLobbyLevelReady);
}

void UFrontierLobbyWidget::BindContentButtons()
{
	if (Button_Lobby)
	{
		Button_Lobby->OnClicked.RemoveAll(this);
		Button_Lobby->OnClicked.AddDynamic(this, &UFrontierLobbyWidget::HandleLobbyButtonClicked);
	}
	if (Button_Storage)
	{
		Button_Storage->OnClicked.RemoveAll(this);
		Button_Storage->OnClicked.AddDynamic(this, &UFrontierLobbyWidget::HandleStorageButtonClicked);
	}
	if (Button_Store)
	{
		Button_Store->OnClicked.RemoveAll(this);
		Button_Store->OnClicked.AddDynamic(this, &UFrontierLobbyWidget::HandleStoreButtonClicked);
	}
	if (Button_SkillTree)
	{
		Button_SkillTree->OnClicked.RemoveAll(this);
		Button_SkillTree->OnClicked.AddDynamic(this, &UFrontierLobbyWidget::HandleSkillTreeButtonClicked);
	}
	if (Button_Upgrade)
	{
		Button_Upgrade->OnClicked.RemoveAll(this);
		Button_Upgrade->OnClicked.AddDynamic(this, &UFrontierLobbyWidget::HandleUpgradeButtonClicked);
	}
}

void UFrontierLobbyWidget::UnbindContentButtons()
{
	if (Button_Lobby)
	{
		Button_Lobby->OnClicked.RemoveAll(this);
	}
	if (Button_Storage)
	{
		Button_Storage->OnClicked.RemoveAll(this);
	}
	if (Button_Store)
	{
		Button_Store->OnClicked.RemoveAll(this);
	}
	if (Button_SkillTree)
	{
		Button_SkillTree->OnClicked.RemoveAll(this);
	}
	if (Button_Upgrade)
	{
		Button_Upgrade->OnClicked.RemoveAll(this);
	}
}
