#pragma once

#include "CoreMinimal.h"
#include "Components/FrontierBackendProtocolComponent.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Progression/FrontierRaidExperienceTypes.h"
#include "FrontierLobbyPlayerController.generated.h"

class UFrontierLobbyWidget;
class UFrontierBackendProtocolComponent;
class UFrontierInventoryCommandService;
class UFrontierSkillTreePersistenceComponent;
class UFrontierSkillTreeWidget;

UCLASS()
class FRONTIER_API AFrontierLobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AFrontierLobbyPlayerController();

	UFrontierBackendProtocolComponent* GetBackendProtocolComponent() const { return BackendProtocolComponent; }
	UFrontierInventoryCommandService* GetInventoryCommandService() const { return InventoryCommandService; }
	UFrontierSkillTreePersistenceComponent* GetSkillTreePersistenceComponent() const { return SkillTreePersistenceComponent; }

	void ShowSkillTreeFromLobby();
	void ShowChangeNicknameWidget(bool bCanCancel);
	void RequestStartRaidFromLobby(const FString& MapId, const FString& PartyId = FString());
	void RequestDebugRaidExperience();
	/** Development-only skill-tree test: asks the server to add exactly three points. */
	void RequestDebugSkillPoints();
	void RequestEndSimulatedRaid(EFrontierRaidOutcome Outcome);
	void HandleRaidEntryCreated(const FFrontierOnlineRaidEntryDTO& Entry);
	void ShowRaidLoadingScreenAfterDelay(float DelaySeconds);
	void ShowRaidLoadingScreen();
	void CancelRaidLoadingScreenTransition();

	UFUNCTION(Client, Reliable)
	void ClientReceiveSimulatedRaidActivated(const FString& RaidSessionId);

	UFUNCTION(Client, Reliable)
	void ClientConnectToRaid(
		const FString& RaidSessionId,
		const FString& ServerEndpoint,
		const FString& JoinToken);

	UFUNCTION(Client, Reliable)
	void ClientReceiveRaidSettlementState(EFrontierRaidFlowState State, const FString& Message);

	UFUNCTION(Client, Reliable)
	void ClientReceiveRaidSettlementReady(const FString& RaidSessionId);

	UFUNCTION(Client, Reliable)
	void ClientReceiveRaidFlowFailed(const FString& Error, bool bRetryable);

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
	void ServerStoreRaidItemInStorage(int32 SourceRaidSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerStoreRaidItemInStorageSlot(int32 SourceRaidSlotIndex, int32 TargetLobbySlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerWithdrawLobbyItemToRaidInventory(int32 SourceLobbySlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerWithdrawLobbyItemToRaidInventorySlot(int32 SourceLobbySlotIndex, int32 TargetRaidSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerRequestUpgradeItem(
		FGuid ItemInstanceId,
		int32 ExpectedEnhancementLevel,
		const FString& IdempotencyKey);

	UFUNCTION(Server, Reliable)
	void ServerAwardDebugRaidExperience(const FString& EventId);

	UFUNCTION(Server, Reliable)
	void ServerGrantDebugSkillPoints();

	UFUNCTION(Server, Reliable)
	void ServerEndSimulatedRaid(bool bExtracted);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION()
	void HandleSkillTreeCloseRequested();

	void PerformPendingRaidConnection();
	void ClearPendingRaidConnection();

	UPROPERTY(EditDefaultsOnly, Category="Lobby|UI")
	TSubclassOf<UFrontierLobbyWidget> LobbyWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="Lobby|UI")
	TSoftClassPtr<UFrontierSkillTreeWidget> SkillTreeWidgetClass;

	UPROPERTY()
	TObjectPtr<UFrontierLobbyWidget> LobbyWidget;

	UPROPERTY()
	TObjectPtr<UFrontierSkillTreeWidget> SkillTreeWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Lobby|Backend")
	TObjectPtr<UFrontierBackendProtocolComponent> BackendProtocolComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Lobby|Inventory")
	TObjectPtr<UFrontierInventoryCommandService> InventoryCommandService;
	

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Lobby|Skill Tree|Persistence")
	TObjectPtr<UFrontierSkillTreePersistenceComponent> SkillTreePersistenceComponent;

	FString PendingRaidSessionId;
	FString PendingRaidServerEndpoint;
	FString PendingRaidJoinToken;
	FTimerHandle RaidLoadingScreenTimerHandle;
};
