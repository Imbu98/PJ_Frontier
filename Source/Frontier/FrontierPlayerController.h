// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/FrontierNumberPopComponent.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Loot/FrontierLootContainerActor.h"
#include "Progression/FrontierRaidExperienceTypes.h"
#include "Progression/FrontierRaidSessionSubsystem.h"
#include "Warning/AttackWarningTypes.h"
#include "FrontierPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UFrontierPlayerStatusWidget;
class UFrontierSpectatorStatusWidget;
class UFrontierTeamStatusWidget;
class ALootDropActor;
class ULootWidget;
class UFrontierInventoryWidget;
class UFrontierInGameInventoryWrapperWidget;
class UFrontierFocusedItemWidget;
class UFrontierLootWidget;
class UFrontierDropLootWidget;
class UFrontierInventoryDragDropOperation;
class UWidget;
class AFrontierDroppedItemActor;
class AFrontierExtractionZoneActor;
class AFrontierGameMode;
class AFrontierPlayerState;
class AFrontierTutorialPosterActor;
class AFrontierRaidDeployActor;
class UFrontierInventoryComponent;
class UFrontierInventoryCommandService;
class UFrontierBackendInventoryReadyComponent;
class USoundBase;
class UFrontierSkillTreePersistenceComponent;
class UFrontierBackendProtocolComponent;
class UFrontierMinimapComponent;
class UFrontierTutorialPosterWidget;
class UMaterialInterface;
class UNiagaraSystem;
class UTexture;
class UAnimMontage;
class UPrimitiveComponent;
class AFrontierBaseCharacter;
class UFrontierInteractionProgressWidget;
class UFrontierNumberPopComponent_NiagaraText;
class AFrontierWeaponBase;
class UFrontierRaidLoadingWidget;
class UFrontierCameraFeedbackComponent;
class UFrontierSettingsWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FFrontierRaidExperienceResultSignature,
	const FFrontierRaidExperienceResult&,
	Result);

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class AFrontierPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AFrontierPlayerController();
	virtual void AcknowledgePossession(APawn* NewPawn) override;

	void RefreshPlayerStatusWidgetBinding();
	void SetWeaponAttackCharge(float ChargeProgress);
	void TryNotifyClientGameplayReady();
	void RefreshTeamStatusWidget();
	void RefreshSkillBarWidget();
	bool ShouldAcceptLookInput() const;
	bool IsInLobbyContext() const;
	bool TryHandleInventoryDragDropOnUI(const FVector2D& ScreenSpacePosition, UFrontierInventoryDragDropOperation* DragOperation) const;
	void CycleSpectatorTarget(int32 Direction);
	bool CanCycleSpectatorTargets() const;
	FText GetCurrentSpectatorTargetDisplayText() const;
	void RequestEndSpectating();
	void HandleRaidSettlementReadyFromBackend(
		const FString& RaidSessionId,
		EFrontierRaidOutcome Outcome,
		int64 AwardedExperience);
	void ToggleInventoryUI();
	void RequestActivateQuickSlot(int32 QuickSlotIndex);
	/** Restores gameplay input after the local settings widget is closed. */
	void HandleSettingsWidgetClosed();
	UFUNCTION(BlueprintCallable, Category="UI|Inventory")
	void OpenLobbyStorageUI();
	UFUNCTION(Client, Reliable)
	void ClientOpenLobbyStorageUI();
	UFUNCTION(Client, Reliable)
	void ClientNotifyRaidReadyState(bool bNewRaidReady);
	UFUNCTION(Client, Reliable)
	void ClientShowPlayerStatusWarning(const FString& WarningMessage);
	UFUNCTION(Client, Unreliable)
	void ClientAddNumberPop(const FFrontierNumberPopRequest& NumberPopRequest);

	/** Plays local-only heavy-hit camera feedback for this controller's player. */
	UFUNCTION(Client, Unreliable)
	void ClientPlayHeavyHitFeedback(const FVector& HitLocation);
	UFUNCTION(Client, Reliable)
	void ClientSetRaidLoadingScreen(bool bShowLoadingScreen);

	UFUNCTION(Server, Reliable)
	void ServerNotifyClientGameplayReady();

	UFUNCTION(Client, Reliable)
	void ClientReceiveRaidExperienceResult(const FFrontierRaidExperienceResult& Result);

	UFUNCTION(Client, Reliable)
	void ClientReceiveRaidActivated(const FString& RaidSessionId);

	UFUNCTION(Client, Reliable)
	void ClientReceiveRaidSettlementState(EFrontierRaidFlowState State, const FString& Message);

	UFUNCTION(Client, Reliable)
	void ClientCompleteRaidSettlement(
		const FString& RaidSessionId,
		EFrontierRaidOutcome Outcome,
		int64 AwardedExperience,
		const FString& LobbyDestination);

	UFUNCTION(Client, Reliable)
	void ClientReceiveRaidFlowFailed(const FString& Error, bool bRetryable);

	UPROPERTY(BlueprintAssignable, Category="Frontier|Raid Experience")
	FFrontierRaidExperienceResultSignature OnRaidExperienceResultReceived;

	UFUNCTION(BlueprintPure, Category="Frontier|Raid Experience")
	FFrontierRaidExperienceResult GetLastRaidExperienceResult() const { return LastRaidExperienceResult; }
	bool IsInventoryUIOpen() const;
	bool IsLootUIOpen() const;
	UFrontierInventoryCommandService* GetInventoryCommandService() const;
	UFrontierBackendProtocolComponent* GetBackendProtocolComponent() const { return BackendProtocolComponent; }
	UFrontierSkillTreePersistenceComponent* GetSkillTreePersistenceComponent() const { return SkillTreePersistenceComponent; }
	
	UFUNCTION(Server, Reliable)
	void ServerEquipRaidInventoryItemToLoadout(int32 SourceSlotIndex, EFrontierEquipmentSlot SlotType);

	UFUNCTION(Server, Reliable)
	void ServerUnequipLoadoutItemToRaidInventory(EFrontierEquipmentSlot SlotType);

	UFUNCTION(Server, Reliable)
	void ServerUnequipLoadoutItemToRaidInventorySlot(EFrontierEquipmentSlot SlotType, int32 TargetSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerSwapRaidInventorySlots(int32 SourceSlotIndex, int32 TargetSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerSwapStorageSlots(int32 SourceSlotIndex, int32 TargetSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerUseRaidInventoryItem(int32 SourceSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerRegisterQuickSlotFromInventory(int32 QuickSlotIndex, int32 InventorySlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerClearQuickSlot(int32 QuickSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerMoveQuickSlot(int32 SourceQuickSlotIndex, int32 TargetQuickSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerUseQuickSlot(int32 QuickSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerStoreRaidItemInStorage(int32 SourceRaidSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerStoreRaidItemInStorageSlot(int32 SourceRaidSlotIndex, int32 TargetLobbySlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerWithdrawLobbyItemToRaidInventory(int32 SourceLobbySlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerWithdrawLobbyItemToRaidInventorySlot(int32 SourceLobbySlotIndex, int32 TargetRaidSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerDropRaidInventoryItem(int32 SourceSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerDropLoadoutItem(EFrontierEquipmentSlot SlotType);

	/** Explicit-target pickup used by the nearby dropped-loot panel. */
	UFUNCTION(Server, Reliable)
	void ServerPickupDroppedItem(AFrontierDroppedItemActor* DroppedItem);

	UFUNCTION(Server, Reliable)
	void ServerPickupDroppedItemToRaidSlot(AFrontierDroppedItemActor* DroppedItem, int32 TargetSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerLootContainerItem(AFrontierLootContainerActor* LootContainer, int32 SourceSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerLootContainerItemToRaidSlot(AFrontierLootContainerActor* LootContainer, int32 SourceLootSlotIndex, int32 TargetRaidSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerLootContainerLoadoutItem(AFrontierLootContainerActor* LootContainer, EFrontierEquipmentSlot SlotType);

	UFUNCTION(Server, Reliable)
	void ServerStoreRaidItemInLootContainer(AFrontierLootContainerActor* LootContainer, int32 SourceRaidSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerStoreRaidItemInLootContainerSlot(AFrontierLootContainerActor* LootContainer, int32 SourceRaidSlotIndex, int32 TargetLootSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerSwapLootContainerSlots(AFrontierLootContainerActor* LootContainer, int32 SourceLootSlotIndex, int32 TargetLootSlotIndex);

	void RequestInteraction();
	/** Cancels an active local interaction when the player starts an interrupting action. */
	void CancelInteractionChannelForLocalAction();

	UFUNCTION(Client, Reliable)
	void ClientOpenLootContainer(AFrontierLootContainerActor* LootContainer, EFrontierLootContainerSourceType SourceType);

	UFUNCTION(Client, Reliable)
	void ClientOpenTutorialPoster(AFrontierTutorialPosterActor* TutorialPoster);

	UFUNCTION(Client, Reliable)
	void ClientShowLocalAttackWarning(FGuid WarningId, const FAttackWarningData& WarningData);

	UFUNCTION(Client, Reliable)
	void ClientHideLocalAttackWarning(FGuid WarningId);

	UFUNCTION(Client, Reliable)
	void ClientShowLocalSkillTrajectory(FGuid TrajectoryId, UNiagaraSystem* TrajectoryNiagaraSystem, const TArray<FVector>& TrajectoryPoints, float Duration);

	UFUNCTION(Client, Reliable)
	void ClientHideLocalSkillTrajectory(FGuid TrajectoryId);

	/** Reveals a damaged target only for the client that owns this controller. */
	UFUNCTION(Client, Unreliable)
	void ClientRevealDamagedTarget(AFrontierBaseCharacter* DamagedTarget, float HealthPercent);

	UFUNCTION(Client, Reliable)
	void ClientBeginInteractionChannel(float Duration, FVector TargetWorldLocation);

	UFUNCTION(Client, Reliable)
	void ClientEndInteractionChannel(bool bCompleted);

	UFUNCTION(Server, Reliable)
	void ServerRequestSetTeam(int32 RequestedTeamId);

	UFUNCTION(Server, Reliable)
	void ServerRequestEndSpectating();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Inventory")
	TObjectPtr<UFrontierInventoryCommandService> InventoryCommandService;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Backend")
	TObjectPtr<UFrontierBackendInventoryReadyComponent> BackendInventoryReadyComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Combat|Number Pop")
	TObjectPtr<UFrontierNumberPopComponent_NiagaraText> NumberPopComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Skill Tree|Persistence")
	TObjectPtr<UFrontierSkillTreePersistenceComponent> SkillTreePersistenceComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Backend")
	TObjectPtr<UFrontierBackendProtocolComponent> BackendProtocolComponent;

	/** Native runtime component. Configure it from the Components panel. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UFrontierMinimapComponent> MinimapComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components|Camera Feedback")
	TObjectPtr<UFrontierCameraFeedbackComponent> CameraFeedbackComponent;


	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UFrontierPlayerStatusWidget> PlayerStatusWidgetClass;

	UPROPERTY()
	TObjectPtr<UFrontierPlayerStatusWidget> PlayerStatusWidget;

	UPROPERTY(EditDefaultsOnly, Category="UI|Spectator")
	TSubclassOf<UFrontierSpectatorStatusWidget> SpectatorStatusWidgetClass;

	UPROPERTY()
	TObjectPtr<UFrontierSpectatorStatusWidget> SpectatorStatusWidget;

	UPROPERTY(EditDefaultsOnly, Category="UI|Settings")
	TSubclassOf<UFrontierSettingsWidget> SettingsWidgetClass;

	UPROPERTY()
	TObjectPtr<UFrontierSettingsWidget> SettingsWidget;

	UPROPERTY(EditDefaultsOnly, Category="UI|Team")
	TSubclassOf<UFrontierTeamStatusWidget> TeamStatusWidgetClass;

	UPROPERTY()
	TObjectPtr<UFrontierTeamStatusWidget> TeamStatusWidget;

	UPROPERTY(EditDefaultsOnly, Category="UI|Inventory")
	TSubclassOf<UFrontierInventoryWidget> InventoryWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="UI|Inventory")
	TSubclassOf<UFrontierInGameInventoryWrapperWidget> InventoryWrapperWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="UI|Inventory")
	TSubclassOf<UFrontierInventoryWidget> StorageWidgetClass;

	UPROPERTY()
	TObjectPtr<UFrontierInventoryWidget> InventoryWidget;

	UPROPERTY()
	TObjectPtr<UFrontierInGameInventoryWrapperWidget> InventoryWrapperWidget;

	UPROPERTY(EditDefaultsOnly, Category="UI|Loot")
	TSubclassOf<UFrontierLootWidget> LootWidgetClass;

	UPROPERTY()
	TObjectPtr<UFrontierLootWidget> LootWidget;

	UPROPERTY(EditDefaultsOnly, Category="UI|Loot")
	TSubclassOf<UFrontierDropLootWidget> DropLootWidgetClass;

	UPROPERTY()
	TObjectPtr<UFrontierDropLootWidget> DropLootWidget;

	UPROPERTY()
	TObjectPtr<UFrontierTutorialPosterWidget> TutorialPosterWidget;

	UPROPERTY(EditDefaultsOnly, Category="UI|Focused Item")
	TSubclassOf<UFrontierFocusedItemWidget> FocusedItemWidgetClass;

	UPROPERTY()
	TObjectPtr<UFrontierFocusedItemWidget> FocusedItemWidget;

	UPROPERTY()
	TObjectPtr<AFrontierLootContainerActor> CurrentOpenedLootContainer;

	UPROPERTY()
	TObjectPtr<UWidget> CurrentLootSideWidget;

	UPROPERTY()
	bool bIsLootUIOpened = false;



	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

private:
	void ToggleSettingsWidget();
	void ApplyLocalCharacterAppearance(APawn* TargetPawn);
	void SetRaidLoadingScreenState(bool bShowLoadingScreen);

	void BuildAvailableSpectatorTargets(TArray<AActor*>& OutTargets) const;
	UFUNCTION(Server, Reliable)
	void ServerRequestInteraction();

	UFUNCTION(Server, Reliable)
	void ServerRequestFocusedInteraction(AActor* FocusedActor);

	UFUNCTION(Server, Reliable)
	void ServerCancelInteractionChannel();

	bool BeginInteractionChannelServer(AActor* InteractableActor);
	void UpdateInteractionChannelServer();
	void CancelInteractionChannelServer(const TCHAR* Reason);
	void CompleteInteractionChannelServer();
	void ClearInteractionChannelServer();
	void UpdateLocalInteractionProgress();
	void BeginLocalInteractionPresentation();
	void EndLocalInteractionPresentation();
	void EndLocalInteractionChannelLocally();
	AActor* FindNearestInteractableActor() const;
	const AFrontierExtractionZoneActor* FindContainingExtractionZone() const;
	bool IsInteractionBlockedByExtractionZone() const;
	UFrontierInGameInventoryWrapperWidget* EnsureInventoryWrapperWidget();
	UFrontierInventoryWidget* EnsureInventoryWidget();
	UFrontierDropLootWidget* EnsureDropLootWidget();
	void StartNearbyDroppedLootTracking(UFrontierInGameInventoryWrapperWidget* WrapperWidget);
	void StopNearbyDroppedLootTracking();
	void RefreshNearbyDroppedLoot();
	bool CanTrackNearbyDroppedLoot() const;
	void OpenLootUI(AFrontierLootContainerActor* LootContainer, EFrontierLootContainerSourceType SourceType);
	void OpenLobbyStorageUILocal();
	void CloseLootUI();
	void CloseInventoryWidgetLocal();
	void CloseLootWidgetLocal();
	void RefreshModalInputState();
	void ApplyModalGameplayInputBlock(bool bShouldBlock);
	void UpdateFocusedInteractable();
	void ApplyInteractionOutline(AActor* InFocusedActor);
	void UpdateFocusedDroppedItem();
	void EnsureFocusedItemWidget();
	bool IsInLobbyLevel() const;
	FString ResolveLobbyLevelPackageName() const;
	void EnsureSpectatorStatusWidget();
	void RefreshSpectatorStatusWidgetVisibility();
	void ShowLocalPlayerStatusWarning(const FString& WarningMessage) const;
	bool IsModalUIBlockingGameplay() const;
	void CacheUIAudioSettings();
	void PlayLocalUIOpenSound(USoundBase* Sound) const;

	UPROPERTY(Transient)
	FFrontierRaidExperienceResult LastRaidExperienceResult;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierDroppedItemActor> FocusedDroppedItemActor;

	UPROPERTY(Transient)
	TObjectPtr<AActor> FocusedInteractableActor;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> FocusedInteractionOutlineComponents;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> CurrentLobbySideWidget;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierRaidLoadingWidget> RaidLoadingWidget;

	/** Time required before the server invokes the interaction callback. */
	UPROPERTY(EditDefaultsOnly, Category="UI|Interaction", meta=(ClampMin="0.1"))
	float InteractionChannelDuration = 2.0f;

	/** Optional character montage played locally while the interaction channel is active. */
	UPROPERTY(EditDefaultsOnly, Category="UI|Interaction")
	TObjectPtr<UAnimMontage> InteractionMontage;

	/** Tolerance for network correction while the player remains substantially stationary. */
	UPROPERTY(EditDefaultsOnly, Category="UI|Interaction", meta=(ClampMin="0.0"))
	float InteractionMovementCancelDistance = 25.0f;

	UPROPERTY(Transient)
	TObjectPtr<AActor> PendingInteractionActor;

	FVector PendingInteractionStartLocation = FVector::ZeroVector;
	float PendingInteractionStartTime = 0.0f;
	float PendingInteractionStartDamageTime = -1000.0f;
	float PendingInteractionDuration = 0.0f;

	bool bLocalInteractionChannelActive = false;
	FVector LocalInteractionStartLocation = FVector::ZeroVector;
	float LocalInteractionStartTime = 0.0f;
	float LocalInteractionDuration = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierWeaponBase> InteractionHiddenWeapon;

	bool bInteractionWeaponWasVisible = true;
	bool bInteractionMontageStarted = false;

	bool bSpectatorInputModeApplied = false;
	bool bRaidLoadingScreenActive = false;
	bool bClientGameplayReadySent = false;
	bool bModalGameplayInputBlocked = false;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LoadedInventoryOpenSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LoadedLootContainerOpenSound;

	float UIOpenSoundVolumeMultiplier = 1.0f;
	float UIOpenSoundPitchMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="UI|Loot", meta=(ClampMin="0.05"))
	float NearbyDroppedLootRefreshInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category="UI|Loot", meta=(ClampMin="1.0"))
	float NearbyDroppedLootQueryRadius = 250.0f;

	FTimerHandle NearbyDroppedLootRefreshTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category="UI|Focused Item", meta=(ClampMin="0.0"))
	float FocusedDroppedItemRange = 180.0f;

	UPROPERTY(EditDefaultsOnly, Category="UI|Interaction", meta=(ClampMin="0.0"))
	float FocusedInteractableRange = 250.0f;

	UPROPERTY(EditAnywhere, Category="UI|Interaction")
	TObjectPtr<UMaterialInterface> InteractionOverlayMaterial;

	void Input_UseQuickSlot1();
	void Input_UseQuickSlot2();
	void Input_UseQuickSlot3();
	void RequestSetTeam(int32 RequestedTeamId);
	void EnsureTeamStatusWidget();

public:
	UFUNCTION(BlueprintPure, Category="UI|Loot")
	AFrontierLootContainerActor* GetCurrentOpenedLootContainer() const;

	UFUNCTION(BlueprintPure, Category="UI|Inventory")
	bool IsLobbyStorageUIOpen() const;

};


