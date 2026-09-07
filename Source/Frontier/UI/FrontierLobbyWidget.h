#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/FrontierBackendProtocolComponent.h"
#include "Components/FrontierLoadoutComponent.h"
#include "Game/FrontierSteamPartySubsystem.h"
#include "Progression/FrontierRaidSessionSubsystem.h"
#include "FrontierLobbyWidget.generated.h"

class UButton;
class UEditableTextBox;
class UBorder;
class UImage;
class UProgressBar;
class AFrontierLobbyPlayerController;
class UFrontierSteamSubsystem;
class UFrontierSteamPartySubsystem;
class UFrontierLobbyMapIconWidget;
class UFrontierRaidSessionSubsystem;
class UFrontierRaidSettlementWidget;
class UFrontierChangeNicknameWidget;
class UFrontierInventoryComponent;
class UFrontierRaidEntryWarningWidget;
class UFrontierStorageWrapper;
class UTextBlock;
class UUpgradePanel;
class UWidgetSwitcher;
class UDataTable;

UENUM(BlueprintType)
enum class EFrontierLobbyPanel : uint8
{
	Start = 0,
	Status = 1,
	Lobby = 2
};

UENUM(BlueprintType)
enum class EFrontierLobbyInitState : uint8
{
	None,
	CheckingSteam,
	RequestingSteamTicket,
	AuthenticatingBackend,
	Completed,
	Failed
};

UENUM(BlueprintType)
enum class EFrontierLobbyContentPanel : uint8
{
	Lobby = 0,
	Inventory = 1,
	Store = 2,
	SkillTree = 3,
	Upgrade = 4
};

UCLASS()
class FRONTIER_API UFrontierLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFrontierLobbyWidget(const FObjectInitializer& ObjectInitializer);
	void ShowChangeNicknameWidget(bool bCanCancel);

	/** Clears matchmaking UI when a controller-side raid flow failure bypasses the status delegate. */
	void HandleRaidFlowFailure(const FString& ErrorMessage);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleStartButtonClicked();

	UFUNCTION()
	void HandleRetryButtonClicked();

	UFUNCTION()
	void HandleQuitButtonClicked();

	UFUNCTION()
	void HandleSettingsButtonClicked();

	UFUNCTION()
	void HandleLobbyButtonClicked();

	UFUNCTION()
	void HandleStorageButtonClicked();

	UFUNCTION()
	void HandleStoreButtonClicked();

	UFUNCTION()
	void HandleSkillTreeButtonClicked();

	UFUNCTION()
	void HandleUpgradeButtonClicked();

	UFUNCTION()
	void HandleMapSelectButtonClicked();

	UFUNCTION()
	void HandleDungeonMapButtonClicked();

	UFUNCTION()
	void HandleForestMapButtonClicked();

	UFUNCTION()
	void HandleRaidEntryButtonClicked();

	UFUNCTION()
	void HandleMapSelectCloseButtonClicked();

	UFUNCTION()
	void HandleRaidFlowStateChanged(EFrontierRaidFlowState State, const FString& Message);

	UFUNCTION()
	void HandleSteamTicketReceived(const FString& SteamTicket);

	UFUNCTION()
	void HandleSteamTicketFailed(const FString& ErrorMessage);

	UFUNCTION()
	void HandleBackendSteamLoginSucceeded(const FFrontierBackendSteamLoginResult& Result);

	UFUNCTION()
	void HandleBackendCurrenciesChanged();

	UFUNCTION()
	void HandleUpgradeStoneInventoryChanged(const TArray<FFrontierInventorySlot>& Slots);

	UFUNCTION()
	void HandleLoadoutChanged(const TArray<FFrontierLoadoutSlot>& Slots);

	UFUNCTION()
	void HandleBackendRequestFailed(const FString& ErrorMessage);

	UFUNCTION()
	void HandleNicknameUpdateSucceeded(const FString& Nickname);

	UFUNCTION()
	void HandleNicknameUpdateFailed(const FString& ErrorMessage);

	UFUNCTION()
	void HandleFallbackNicknameConfirmClicked();

	UFUNCTION()
	void HandleFallbackNicknameCancelClicked();

	UFUNCTION()
	void HandleMatchmakingStatusChanged(const FString& Status);

	UFUNCTION()
	void HandleCancelMatchMakingButtonClicked();

	UFUNCTION()
	void HandleLobbyLevelReady(
		const FFrontierPlayerLevelSnapshot& Level,
		const FFrontierTemporarySkillPointResult& SkillPointResult);

	UFUNCTION()
	void HandleSettlementReturnToLobbyRequested();

	UFUNCTION()
	void HandleInviteFriend1Clicked();

	UFUNCTION()
	void HandleInviteFriend2Clicked();

	UFUNCTION()
	void HandleSteamPartyMembersChanged(const TArray<FFrontierSteamPartyMember>& Members);

	void StartLobbyInitialization();
	void ResumeLobbyInitialization();
	void CompleteLobbyInitialization(const FFrontierBackendSteamLoginResult& LoginResult);
	void HandleLobbyProcessFailed(const FString& ErrorMessage);
	void SetStatusText(const FString& NewStatus);
	void SetActiveLobbyPanel(EFrontierLobbyPanel Panel);
	void SetActiveContentPanel(EFrontierLobbyContentPanel Panel);
	void SetLobbyInitState(EFrontierLobbyInitState NewState, const FString& StatusMessage);
	void RefreshLobbyPlayerInfo(const FFrontierBackendSteamLoginResult& LoginResult);
	void BindFallbackChangeNicknameWidget();
	void UnbindFallbackChangeNicknameWidget();
	void BindSteamSubsystem();
	void UnbindSteamSubsystem();
	void BindSteamPartySubsystem();
	void UnbindSteamPartySubsystem();
	void RefreshSteamPartyDisplay();
	void BindBackendProtocolComponent();
	void UnbindBackendProtocolComponent();
	void BindContentButtons();
	void UnbindContentButtons();
	void RefreshStoragePanel();
	void RefreshCurrencies();
	void RefreshLevelDisplay();
	void BindRaidIntegration();
	void UnbindRaidIntegration();
	void RefreshRaidControls();
	void RefreshMatchmakingControls();
	void ScheduleRaidLoadingScreen();
	void ShowMatchmakingFailurePopup(const FString& ErrorMessage = FString());
	void BindMapSelectButtons();
	void UnbindMapSelectButtons();
	void SelectRaidMap(FName MapName);
	void SetMapSelectPanelVisible(bool bVisible);
	void RefreshSelectedMapInfo();
	void RefreshRaidEntryButtonState();
	float GetTotalEquipmentScore() const;
	void CollectLimitedEquipmentSlots(float LimitEquipmentScore, TArray<FFrontierInventorySlot>& OutSlots) const;
	bool ShowRaidEntryWarningIfNeeded();
	void HideRaidEntryWarning();
	UButton* ResolveRaidEntryButton();
	void RefreshUpgradeStoneDisplay();
	void BindUpgradeStoneInventoryEvents();
	void UnbindUpgradeStoneInventoryEvents();
	void BindLoadoutEvents();
	void UnbindLoadoutEvents();
	int32 CountUpgradeStoneQuantity(const UFrontierInventoryComponent* InventoryComponent) const;
	bool IsUpgradeStoneTemplateId(FName ItemTemplateId) const;
	void RefreshCurrentEquipmentScoreDisplay();
	float GetEquipmentScoreForItem(const FFrontierItemInstance& ItemInstance) const;
	UTextBlock* ResolveUpgradeStoneText();

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UWidgetSwitcher> WidgetSwitcher_Lobby;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UWidgetSwitcher> WidgetSwitcher_Contents;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Start;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_lobbySettings;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_startSettings;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> WBP_SettingsWidget;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> WBP_ChangeNickname;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_MapSelect;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> MapSelectPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> MapInfoPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierLobbyMapIconWidget> Button_L_Dungeon;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierLobbyMapIconWidget> Button_L_Forest;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_RaidEntry;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_RaidButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_MapSelectClose;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Box_MatchMaking;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MatchMakingStatus;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_CancelMatchMaking;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SelectedMapName;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> Image_MapImage;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MinTotalEquipmentScore;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MaxTotalEquipmentScore;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_LimitEquipmentScore;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CurrentEquimentScore;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_PlayerUpgradeStone;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Retry;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Quit;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_PlayerName;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_PlayerLevel;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CurrentLevelExperience;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_NextLevelExperience;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TemporarySkillPoints;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar_LevelExperience;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> Image_MyProfileImage;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_InviteFriend1;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> Image_TeamProfileImage1;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_InviteFriend2;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> Image_TeamProfileImage2;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierRaidSettlementWidget> WBP_RaidSettlement;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Lobby;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Storage;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Store;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_SkillTree;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Upgrade;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UUpgradePanel> WBP_UpgradePanel;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UOutGameInventoryWrapperWidget> WBP_LobbyInventoryWrapper;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UFrontierUserCurrencySlot> WBP_GoldCurrencySlot;

	UPROPERTY()
	TObjectPtr<UFrontierSteamSubsystem> SteamSubsystem;

	UPROPERTY()
	TObjectPtr<UFrontierSteamPartySubsystem> SteamPartySubsystem;

	UPROPERTY()
	TObjectPtr<UFrontierBackendProtocolComponent> BackendProtocolComponent;

	UPROPERTY()
	TObjectPtr<UFrontierRaidSessionSubsystem> RaidSessionSubsystem;

	bool bAuthenticationInProgress = false;
	bool bLobbyInitialized = false;
	EFrontierLobbyInitState CurrentInitState = EFrontierLobbyInitState::None;
	FFrontierPlayerLevelSnapshot CachedPlayerLevel;
	FFrontierTemporarySkillPointResult CachedSkillPointResult;
	bool bHasCachedPlayerLevel = false;
	bool bInitialNicknamePromptShown = false;
	bool bFallbackNicknameRequestInProgress = false;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> FallbackNicknameTextBox;

	UPROPERTY()
	TObjectPtr<UButton> FallbackNicknameConfirmButton;

	UPROPERTY()
	TObjectPtr<UButton> FallbackNicknameCancelButton;
	bool bMatchmakingActive = false;
	bool bMatchmakingCancelInProgress = false;
	bool bMatchmakingServerReady = false;
	bool bRaidEntryWarningCreatedAtRuntime = false;
	FString PendingMatchmakingFailureMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid|Map Selection", meta=(AllowPrivateAccess="true"))
	FName MedievalDungeonID = TEXT("Map_MedivalDungeon");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid|Map Selection", meta=(AllowPrivateAccess="true"))
	FName DarkForestID = TEXT("Map_DarkForest");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid|Map Selection", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UDataTable> MapInfoDataTable;

	/** Optional warning WBP class. Used when the warning is not embedded in the lobby WBP. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid|Map Selection|Warning", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UFrontierRaidEntryWarningWidget> RaidEntryWarningWidgetClass;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierRaidEntryWarningWidget> WBP_RaidEntryWarning;

	/** Backend/catalog template ID used for the upgrade material counter in map selection. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Upgrade", meta=(AllowPrivateAccess="true"))
	FName UpgradeStoneItemTemplateId = TEXT("Item_Miscellaneous_UpgradeStone_NormalUpgradeStone");

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Raid|Map Selection", meta=(AllowPrivateAccess="true"))
	FName SelectedRaidMapName = NAME_None;
	
	
};
