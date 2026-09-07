#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "AbilitySystem/FrontierAttributeSet.h"
#include "Components/FrontierInventoryCommandService.h"
#include "Components/FrontierInventoryComponent.h"
#include "Components/FrontierLoadoutComponent.h"
#include "Components/FrontierStorageComponent.h"
#include "Components/FrontierLootInventoryComponent.h"
#include "Components/FrontierRaidInventoryComponent.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierGameMode.h"
#include "Game/FrontierPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "Loot/FrontierLootContainerActor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealEdGlobals.h"

namespace FrontierInventoryNetworkPIETest
{
	constexpr int32 PrivateSlotIndex = 7;
	constexpr int32 RaidSlotIndex = 3;
	constexpr int32 ForwardedRaidSlotIndex = 4;
	constexpr int32 InitialLootSlotIndex = 5;
	constexpr int32 UpdatedLootSlotIndex = 6;
	constexpr int32 InitialLootMarker = 90001;
	constexpr int32 UpdatedLootMarker = 90002;
	constexpr int32 InitialPlayerMarkerBase = 10000;
	constexpr int32 UpdatedPlayerMarkerBase = 60000;
	constexpr float InitialHealthMarkerBase = 1000.0f;
	constexpr float InitialStaminaMarkerBase = 500.0f;
	constexpr float UpdatedStaminaMarkerBase = 700.0f;
	constexpr double PhaseTimeoutSeconds = 30.0;
	constexpr double MaxVitalsReplicationLatencySeconds = 0.75;

	struct FNetworkPIETestState
	{
		explicit FNetworkPIETestState(FAutomationTestBase* InTest)
			: Test(InTest)
		{
		}

		void Fail(const FString& Message)
		{
			if (!bFailed && Test)
			{
				Test->AddError(Message);
			}
			bFailed = true;
		}

		FAutomationTestBase* Test = nullptr;
		bool bFailed = false;
		double VitalsMutationStartedAt = 0.0;
		double VitalsReplicationDelaySeconds = 0.0;
	};

	using FNetworkPIETestStateRef = TSharedRef<FNetworkPIETestState>;

	TArray<UWorld*> GetPIEWorlds(const ENetMode NetMode)
	{
		TArray<UWorld*> Worlds;
		if (!GEngine)
		{
			return Worlds;
		}

		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			UWorld* World = WorldContext.World();
			if (WorldContext.WorldType == EWorldType::PIE && World && World->GetNetMode() == NetMode)
			{
				Worlds.Add(World);
			}
		}
		return Worlds;
	}

	UWorld* GetDedicatedServerWorld()
	{
		const TArray<UWorld*> ServerWorlds = GetPIEWorlds(NM_DedicatedServer);
		return ServerWorlds.Num() == 1 ? ServerWorlds[0] : nullptr;
	}

	bool HasConnectedPlayers(UWorld* World, const int32 ExpectedCount)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		return GameState && GameState->PlayerArray.Num() == ExpectedCount;
	}

	const FFrontierInventorySlot* FindInventorySlot(const UFrontierInventoryComponent* Inventory, const int32 SlotIndex)
	{
		if (!Inventory || !Inventory->GetSlots().IsValidIndex(SlotIndex))
		{
			return nullptr;
		}
		return &Inventory->GetSlots()[SlotIndex];
	}

	bool HasInventoryMarker(const UFrontierInventoryComponent* Inventory, const int32 SlotIndex, const int32 Marker)
	{
		const FFrontierInventorySlot* Slot = FindInventorySlot(Inventory, SlotIndex);
		return Slot && Slot->bOccupied && Slot->ItemInstance.IsValid() && Slot->ItemInstance.EnhancementLevel == Marker;
	}

	bool IsInventorySlotEmpty(const UFrontierInventoryComponent* Inventory, const int32 SlotIndex)
	{
		const FFrontierInventorySlot* Slot = FindInventorySlot(Inventory, SlotIndex);
		return Slot && !Slot->bOccupied;
	}

	bool HasLoadoutMarker(const UFrontierLoadoutComponent* Loadout, const int32 Marker)
	{
		if (!Loadout)
		{
			return false;
		}

		for (const FFrontierLoadoutSlot& Slot : Loadout->GetLoadoutSlots())
		{
			if (Slot.bOccupied && Slot.ItemInstance.IsValid() && Slot.ItemInstance.EnhancementLevel == Marker)
			{
				return true;
			}
		}
		return false;
	}

	bool IsLoadoutSlotEmpty(const UFrontierLoadoutComponent* Loadout)
	{
		if (!Loadout)
		{
			return false;
		}

		for (const FFrontierLoadoutSlot& Slot : Loadout->GetLoadoutSlots())
		{
			if (Slot.bOccupied)
			{
				return false;
			}
		}
		return true;
	}

	AFrontierLootContainerActor* FindTestLootActor(UWorld* World, const int32 SlotIndex, const int32 Marker)
	{
		if (!World)
		{
			return nullptr;
		}

		for (TActorIterator<AFrontierLootContainerActor> It(World); It; ++It)
		{
			AFrontierLootContainerActor* LootActor = *It;
			if (LootActor && HasInventoryMarker(LootActor->GetLootInventoryComponent(), SlotIndex, Marker))
			{
				return LootActor;
			}
		}
		return nullptr;
	}

	bool IsNetworkReady()
	{
		UWorld* ServerWorld = GetDedicatedServerWorld();
		const TArray<UWorld*> ClientWorlds = GetPIEWorlds(NM_Client);
		if (!ServerWorld || ClientWorlds.Num() != 2 || !HasConnectedPlayers(ServerWorld, 2))
		{
			return false;
		}

		for (UWorld* ClientWorld : ClientWorlds)
		{
			APlayerController* LocalController = ClientWorld ? ClientWorld->GetFirstPlayerController() : nullptr;
			if (!LocalController || !LocalController->GetPlayerState<AFrontierPlayerState>() || !HasConnectedPlayers(ClientWorld, 2))
			{
				return false;
			}
		}
		return true;
	}

	bool IsForwardedRaidSwapReady(const int32 OccupiedSlotIndex, const int32 EmptySlotIndex)
	{
		const TArray<UWorld*> ClientWorlds = GetPIEWorlds(NM_Client);
		if (ClientWorlds.Num() != 2)
		{
			return false;
		}

		for (UWorld* ClientWorld : ClientWorlds)
		{
			AFrontierPlayerController* LocalController = Cast<AFrontierPlayerController>(
				ClientWorld ? ClientWorld->GetFirstPlayerController() : nullptr);
			AFrontierPlayerState* LocalPlayerState = LocalController
				? LocalController->GetPlayerState<AFrontierPlayerState>()
				: nullptr;
			if (!LocalController || !LocalController->GetInventoryCommandService() || !LocalPlayerState)
			{
				return false;
			}

			const int32 ExpectedMarker = InitialPlayerMarkerBase + LocalPlayerState->GetPlayerId();
			if (!HasInventoryMarker(LocalPlayerState->GetRaidInventoryComponent(), OccupiedSlotIndex, ExpectedMarker)
				|| !IsInventorySlotEmpty(LocalPlayerState->GetRaidInventoryComponent(), EmptySlotIndex))
			{
				return false;
			}
		}
		return true;
	}

	bool HasPublicVitals(
		const AFrontierPlayerState* PlayerState,
		const float ExpectedHealth,
		const float ExpectedStamina,
		const bool bExpectedDead)
	{
		if (!PlayerState)
		{
			return false;
		}

		const FFrontierPublicVitalsSnapshot Snapshot = PlayerState->GetPublicVitalsSnapshot();
		return FMath::IsNearlyEqual(Snapshot.Health, ExpectedHealth)
			&& FMath::IsNearlyEqual(Snapshot.MaxHealth, 2000.0f + PlayerState->GetPlayerId())
			&& FMath::IsNearlyEqual(Snapshot.Stamina, ExpectedStamina)
			&& FMath::IsNearlyEqual(Snapshot.MaxStamina, ExpectedStamina)
			&& Snapshot.bIsDead == bExpectedDead
			&& FMath::IsNearlyEqual(PlayerState->GetDisplayHealth(), ExpectedHealth)
			&& FMath::IsNearlyEqual(PlayerState->GetDisplayStamina(), ExpectedStamina)
			&& PlayerState->IsDisplayDead() == bExpectedDead;
	}

	bool ArePublicVitalsReady(UWorld* ClientWorld, const bool bUpdated)
	{
		const AGameStateBase* GameState = ClientWorld ? ClientWorld->GetGameState() : nullptr;
		if (!GameState)
		{
			return false;
		}

		for (APlayerState* BasePlayerState : GameState->PlayerArray)
		{
			const AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(BasePlayerState);
			if (!PlayerState)
			{
				return false;
			}

			const float ExpectedHealth = bUpdated ? 0.0f : InitialHealthMarkerBase + PlayerState->GetPlayerId();
			const float ExpectedStaminaBase = bUpdated ? UpdatedStaminaMarkerBase : InitialStaminaMarkerBase;
			if (!HasPublicVitals(
				PlayerState,
				ExpectedHealth,
				ExpectedStaminaBase + PlayerState->GetPlayerId(),
				bUpdated))
			{
				return false;
			}
		}
		return true;
	}

	FString DescribeInitialReplicationState()
	{
		FString Description;
		for (UWorld* ClientWorld : GetPIEWorlds(NM_Client))
		{
			APlayerController* LocalController = ClientWorld ? ClientWorld->GetFirstPlayerController() : nullptr;
			AFrontierPlayerState* LocalPlayerState = LocalController ? LocalController->GetPlayerState<AFrontierPlayerState>() : nullptr;
			const int32 ExpectedMarker = LocalPlayerState ? InitialPlayerMarkerBase + LocalPlayerState->GetPlayerId() : INDEX_NONE;
			Description += FString::Printf(
				TEXT(" ClientPIE=%d LocalPS=%d Lobby=%d Raid=%d Loadout=%d Loot=%d Vitals=["),
				ClientWorld ? ClientWorld->GetPackage()->GetPIEInstanceID() : INDEX_NONE,
				LocalPlayerState ? LocalPlayerState->GetPlayerId() : INDEX_NONE,
				LocalPlayerState && HasInventoryMarker(LocalPlayerState->GetStorageComponent(), PrivateSlotIndex, ExpectedMarker),
				LocalPlayerState && HasInventoryMarker(LocalPlayerState->GetRaidInventoryComponent(), RaidSlotIndex, ExpectedMarker),
				LocalPlayerState && HasLoadoutMarker(LocalPlayerState->GetLoadoutComponent(), ExpectedMarker),
				FindTestLootActor(ClientWorld, InitialLootSlotIndex, InitialLootMarker) != nullptr);

			const AGameStateBase* GameState = ClientWorld ? ClientWorld->GetGameState() : nullptr;
			if (GameState)
			{
				for (APlayerState* BasePlayerState : GameState->PlayerArray)
				{
					const AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(BasePlayerState);
					if (!PlayerState)
					{
						Description += TEXT("NonFrontierPS ");
						continue;
					}

					const FFrontierPublicVitalsSnapshot Snapshot = PlayerState->GetPublicVitalsSnapshot();
					Description += FString::Printf(
						TEXT("PS%d H=%.1f/%.1f S=%.1f/%.1f Dead=%d "),
						PlayerState->GetPlayerId(),
						Snapshot.Health,
						Snapshot.MaxHealth,
						Snapshot.Stamina,
						Snapshot.MaxStamina,
						Snapshot.bIsDead);
				}
			}
			Description += TEXT("]");
		}
		return Description;
	}

	bool IsInitialReplicationReady()
	{
		const TArray<UWorld*> ClientWorlds = GetPIEWorlds(NM_Client);
		if (ClientWorlds.Num() != 2)
		{
			return false;
		}

		for (UWorld* ClientWorld : ClientWorlds)
		{
			APlayerController* LocalController = ClientWorld->GetFirstPlayerController();
			AFrontierPlayerState* LocalPlayerState = LocalController ? LocalController->GetPlayerState<AFrontierPlayerState>() : nullptr;
			if (!LocalPlayerState)
			{
				return false;
			}
			if (!ArePublicVitalsReady(ClientWorld, false))
			{
				return false;
			}

			const int32 ExpectedMarker = InitialPlayerMarkerBase + LocalPlayerState->GetPlayerId();
			if (!HasInventoryMarker(LocalPlayerState->GetStorageComponent(), PrivateSlotIndex, ExpectedMarker)
				|| !HasInventoryMarker(LocalPlayerState->GetRaidInventoryComponent(), RaidSlotIndex, ExpectedMarker)
				|| !HasLoadoutMarker(LocalPlayerState->GetLoadoutComponent(), ExpectedMarker))
			{
				return false;
			}

			AFrontierLootContainerActor* LootActor = FindTestLootActor(ClientWorld, InitialLootSlotIndex, InitialLootMarker);
			if (!LootActor || !HasInventoryMarker(LootActor->GetLootInventoryComponent(), InitialLootSlotIndex, InitialLootMarker))
			{
				return false;
			}

			const TArray<FFrontierInventorySlot>& CompatibilityMirror = LootActor->GetDeadPlayerInventory();
			if (!CompatibilityMirror.IsValidIndex(InitialLootSlotIndex)
				|| !CompatibilityMirror[InitialLootSlotIndex].bOccupied
				|| CompatibilityMirror[InitialLootSlotIndex].ItemInstance.EnhancementLevel != InitialLootMarker)
			{
				return false;
			}
		}
		return true;
	}

	bool IsUpdatedReplicationReady()
	{
		const TArray<UWorld*> ClientWorlds = GetPIEWorlds(NM_Client);
		if (ClientWorlds.Num() != 2)
		{
			return false;
		}

		for (UWorld* ClientWorld : ClientWorlds)
		{
			APlayerController* LocalController = ClientWorld->GetFirstPlayerController();
			AFrontierPlayerState* LocalPlayerState = LocalController ? LocalController->GetPlayerState<AFrontierPlayerState>() : nullptr;
			if (!LocalPlayerState)
			{
				return false;
			}
			if (!ArePublicVitalsReady(ClientWorld, true))
			{
				return false;
			}

			const int32 ExpectedMarker = UpdatedPlayerMarkerBase + LocalPlayerState->GetPlayerId();
			if (!HasInventoryMarker(LocalPlayerState->GetStorageComponent(), PrivateSlotIndex, ExpectedMarker)
				|| !IsInventorySlotEmpty(LocalPlayerState->GetRaidInventoryComponent(), RaidSlotIndex)
				|| !IsLoadoutSlotEmpty(LocalPlayerState->GetLoadoutComponent()))
			{
				return false;
			}

			AFrontierLootContainerActor* LootActor = FindTestLootActor(ClientWorld, UpdatedLootSlotIndex, UpdatedLootMarker);
			if (!LootActor
				|| !IsInventorySlotEmpty(LootActor->GetLootInventoryComponent(), InitialLootSlotIndex)
				|| !HasInventoryMarker(LootActor->GetLootInventoryComponent(), UpdatedLootSlotIndex, UpdatedLootMarker))
			{
				return false;
			}

			const TArray<FFrontierInventorySlot>& CompatibilityMirror = LootActor->GetDeadPlayerInventory();
			if (!CompatibilityMirror.IsValidIndex(UpdatedLootSlotIndex)
				|| CompatibilityMirror[InitialLootSlotIndex].bOccupied
				|| !CompatibilityMirror[UpdatedLootSlotIndex].bOccupied
				|| CompatibilityMirror[UpdatedLootSlotIndex].ItemInstance.EnhancementLevel != UpdatedLootMarker)
			{
				return false;
			}
		}
		return true;
	}

	void ValidateRemotePrivacy(const FNetworkPIETestStateRef& State)
	{
		for (UWorld* ClientWorld : GetPIEWorlds(NM_Client))
		{
			APlayerController* LocalController = ClientWorld->GetFirstPlayerController();
			AFrontierPlayerState* LocalPlayerState = LocalController ? LocalController->GetPlayerState<AFrontierPlayerState>() : nullptr;
			AGameStateBase* GameState = ClientWorld->GetGameState();
			if (!LocalPlayerState || !GameState)
			{
				State->Fail(TEXT("Client world lost its local PlayerState or GameState during privacy validation."));
				return;
			}

			const UFrontierAttributeSet* LocalAttributes = LocalPlayerState->GetFrontierAttributeSet();
			const float ExpectedLocalMaxHealth = 2000.0f + LocalPlayerState->GetPlayerId();
			if (!LocalAttributes || !FMath::IsNearlyEqual(LocalAttributes->GetMaxHealth(), ExpectedLocalMaxHealth))
			{
				State->Fail(FString::Printf(
					TEXT("Owner-only attributes did not reach local PlayerState %d in client PIE instance %d."),
					LocalPlayerState->GetPlayerId(),
					ClientWorld->GetPackage()->GetPIEInstanceID()));
				return;
			}

			for (APlayerState* BasePlayerState : GameState->PlayerArray)
			{
				AFrontierPlayerState* RemotePlayerState = Cast<AFrontierPlayerState>(BasePlayerState);
				if (!RemotePlayerState || RemotePlayerState == LocalPlayerState)
				{
					continue;
				}

				if (!IsInventorySlotEmpty(RemotePlayerState->GetStorageComponent(), PrivateSlotIndex)
					|| !IsInventorySlotEmpty(RemotePlayerState->GetRaidInventoryComponent(), RaidSlotIndex)
					|| !IsLoadoutSlotEmpty(RemotePlayerState->GetLoadoutComponent()))
				{
					State->Fail(FString::Printf(
						TEXT("Owner-only privacy failed in client PIE instance %d: remote PlayerState %d received private inventory or loadout data."),
						ClientWorld->GetPackage()->GetPIEInstanceID(),
						RemotePlayerState->GetPlayerId()));
					return;
				}

				const UFrontierAttributeSet* RemoteAttributes = RemotePlayerState->GetFrontierAttributeSet();
				const float PrivateMaxHealthMarker = 2000.0f + RemotePlayerState->GetPlayerId();
				if (RemoteAttributes && FMath::IsNearlyEqual(RemoteAttributes->GetMaxHealth(), PrivateMaxHealthMarker))
				{
					State->Fail(FString::Printf(
						TEXT("Owner-only attribute privacy failed in client PIE instance %d: remote PlayerState %d received private GAS attributes."),
						ClientWorld->GetPackage()->GetPIEInstanceID(),
						RemotePlayerState->GetPlayerId()));
					return;
				}
			}
		}
	}
}

class FStartFrontierNetworkPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FStartFrontierNetworkPIECommand(const FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef& InState)
		: State(InState)
	{
	}

	virtual bool Update() override
	{
		if (!GUnrealEd)
		{
			State->Fail(TEXT("UnrealEd is unavailable; cannot start multiplayer PIE."));
			return true;
		}

		ULevelEditorPlaySettings* TestSettings = NewObject<ULevelEditorPlaySettings>();
		TestSettings->SetPlayNetMode(PIE_Client);
		TestSettings->SetRunUnderOneProcess(true);
		TestSettings->SetPlayNumberOfClients(2);
		TestSettings->bLaunchSeparateServer = false;

		FRequestPlaySessionParams RequestParams;
		RequestParams.EditorPlaySettings = TestSettings;
		RequestParams.GlobalMapOverride = TEXT("/Game/ThirdPerson/Lvl_ThirdPerson");
		UClass* ProjectGameModeClass = LoadClass<AFrontierGameMode>(
			nullptr,
			TEXT("/Game/SY/BP/BP_FrontierGameMode.BP_FrontierGameMode_C"));
		if (!ProjectGameModeClass)
		{
			State->Fail(TEXT("Could not load the project Frontier GameMode for Controller RPC testing."));
			return true;
		}
		RequestParams.GameModeOverride = ProjectGameModeClass;
		RequestParams.bAllowOnlineSubsystem = false;
		GUnrealEd->RequestPlaySession(RequestParams);
		return true;
	}

private:
	FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef State;
};

class FWaitForFrontierNetworkPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FWaitForFrontierNetworkPIECommand(const FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef& InState)
		: State(InState)
		, Deadline(FPlatformTime::Seconds() + FrontierInventoryNetworkPIETest::PhaseTimeoutSeconds)
	{
	}

	virtual bool Update() override
	{
		if (State->bFailed || FrontierInventoryNetworkPIETest::IsNetworkReady())
		{
			return true;
		}

		if (FPlatformTime::Seconds() >= Deadline)
		{
			State->Fail(TEXT("Timed out waiting for one dedicated server and two connected PIE clients."));
			return true;
		}
		return false;
	}

private:
	FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef State;
	double Deadline;
};

class FSeedFrontierNetworkPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FSeedFrontierNetworkPIECommand(const FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef& InState)
		: State(InState)
	{
	}

	virtual bool Update() override
	{
		using namespace FrontierInventoryNetworkPIETest;
		if (State->bFailed)
		{
			return true;
		}

		UWorld* ServerWorld = GetDedicatedServerWorld();
		AGameStateBase* GameState = ServerWorld ? ServerWorld->GetGameState() : nullptr;
		UFrontierItemDataAsset* ItemData = LoadObject<UFrontierItemDataAsset>(
			nullptr,
			TEXT("/Game/SY/DA/ArmorItem/DA_Amor_HelmetItem_Basic.DA_Amor_HelmetItem_Basic"));
		if (!ServerWorld || !GameState || !ItemData)
		{
			State->Fail(TEXT("Could not load the network test server world, GameState, or weapon item data asset."));
			return true;
		}

		for (APlayerState* BasePlayerState : GameState->PlayerArray)
		{
			AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(BasePlayerState);
			if (!PlayerState)
			{
				State->Fail(TEXT("Dedicated server created a non-Frontier PlayerState."));
				return true;
			}

			UFrontierStorageComponent* Storage = PlayerState->GetStorageComponent();
			UFrontierRaidInventoryComponent* RaidInventory = PlayerState->GetRaidInventoryComponent();
			UFrontierLoadoutComponent* Loadout = PlayerState->GetLoadoutComponent();
			UFrontierAbilitySystemComponent* AbilitySystem = PlayerState->GetFrontierAbilitySystemComponent();
			if (!Storage || !RaidInventory || !Loadout || !AbilitySystem)
			{
				State->Fail(TEXT("A Frontier PlayerState is missing an inventory, loadout, or ability system component."));
				return true;
			}

			AbilitySystem->SetNumericAttributeBase(UFrontierAttributeSet::GetMaxHealthAttribute(), 2000.0f + PlayerState->GetPlayerId());
			AbilitySystem->SetNumericAttributeBase(UFrontierAttributeSet::GetHealthAttribute(), InitialHealthMarkerBase + PlayerState->GetPlayerId());
			const float InitialStaminaMarker = InitialStaminaMarkerBase + PlayerState->GetPlayerId();
			AbilitySystem->SetNumericAttributeBase(UFrontierAttributeSet::GetMaxStaminaAttribute(), InitialStaminaMarker);
			AbilitySystem->SetNumericAttributeBase(UFrontierAttributeSet::GetStaminaAttribute(), InitialStaminaMarker);

			Storage->SetSlotsFromSnapshot({});
			RaidInventory->SetSlotsFromSnapshot({});
			Loadout->SetLoadoutSlotsFromSnapshot({});

			const int32 Marker = InitialPlayerMarkerBase + PlayerState->GetPlayerId();
			FFrontierItemInstance LobbyItem = FFrontierItemInstance::CreateFromItemData(ItemData);
			LobbyItem.EnhancementLevel = Marker;
			FFrontierItemInstance RaidItem = FFrontierItemInstance::CreateFromItemData(ItemData);
			RaidItem.EnhancementLevel = Marker;
			FFrontierItemInstance LoadoutItem = FFrontierItemInstance::CreateFromItemData(ItemData);
			LoadoutItem.EnhancementLevel = Marker;
			EFrontierEquipmentSlot LoadoutSlot = EFrontierEquipmentSlot::None;

			if (!Storage->SetItemAtSlot(PrivateSlotIndex, LobbyItem)
				|| !RaidInventory->SetItemAtSlot(RaidSlotIndex, RaidItem)
				|| !Loadout->ResolveEquipSlotForItem(LoadoutItem, LoadoutSlot)
				|| !Loadout->EquipItem(LoadoutSlot, LoadoutItem))
			{
				State->Fail(FString::Printf(TEXT("Failed to seed private data for PlayerState %d."), PlayerState->GetPlayerId()));
				return true;
			}
		}

		AFrontierLootContainerActor* LootActor = ServerWorld->SpawnActor<AFrontierLootContainerActor>();
		if (!LootActor)
		{
			State->Fail(TEXT("Failed to spawn the public world loot actor."));
			return true;
		}

		TArray<FFrontierInventorySlot> LootSlots;
		LootSlots.SetNum(20);
		for (int32 SlotIndex = 0; SlotIndex < LootSlots.Num(); ++SlotIndex)
		{
			LootSlots[SlotIndex].SlotIndex = SlotIndex;
		}
		FFrontierInventorySlot& LootSlot = LootSlots[InitialLootSlotIndex];
		LootSlot.bOccupied = true;
		LootSlot.ItemInstance = FFrontierItemInstance::CreateFromItemData(ItemData);
		LootSlot.ItemInstance.EnhancementLevel = InitialLootMarker;
		LootActor->InitializeFromLootSlots(LootSlots, EFrontierLootContainerSourceType::PlayerDeath);
		return true;
	}

private:
	FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef State;
};

class FWaitForInitialFrontierReplicationCommand final : public IAutomationLatentCommand
{
public:
	explicit FWaitForInitialFrontierReplicationCommand(const FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef& InState)
		: State(InState)
		, Deadline(FPlatformTime::Seconds() + FrontierInventoryNetworkPIETest::PhaseTimeoutSeconds)
	{
	}

	virtual bool Update() override
	{
		if (State->bFailed)
		{
			return true;
		}
		if (FrontierInventoryNetworkPIETest::IsInitialReplicationReady())
		{
			FrontierInventoryNetworkPIETest::ValidateRemotePrivacy(State);
			return true;
		}
		if (FPlatformTime::Seconds() >= Deadline)
		{
			State->Fail(FString::Printf(
				TEXT("Timed out waiting for initial inventory, loot, and public vitals replication.%s"),
				*FrontierInventoryNetworkPIETest::DescribeInitialReplicationState()));
			return true;
		}
		return false;
	}

private:
	FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef State;
	double Deadline;
};

class FRequestFrontierRaidSlotSwapPIECommand final : public IAutomationLatentCommand
{
public:
	FRequestFrontierRaidSlotSwapPIECommand(
		const FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef& InState,
		const int32 InSourceSlotIndex,
		const int32 InTargetSlotIndex)
		: State(InState)
		, SourceSlotIndex(InSourceSlotIndex)
		, TargetSlotIndex(InTargetSlotIndex)
	{
	}

	virtual bool Update() override
	{
		if (State->bFailed)
		{
			return true;
		}

		for (UWorld* ClientWorld : FrontierInventoryNetworkPIETest::GetPIEWorlds(NM_Client))
		{
			AFrontierPlayerController* LocalController = Cast<AFrontierPlayerController>(
				ClientWorld ? ClientWorld->GetFirstPlayerController() : nullptr);
			if (!LocalController || !LocalController->GetInventoryCommandService())
			{
				State->Fail(TEXT("A PIE client is not using FrontierPlayerController or is missing its command service."));
				return true;
			}

			LocalController->ServerSwapRaidInventorySlots(SourceSlotIndex, TargetSlotIndex);
		}
		return true;
	}

private:
	FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef State;
	int32 SourceSlotIndex;
	int32 TargetSlotIndex;
};

class FWaitForFrontierRaidSlotSwapPIECommand final : public IAutomationLatentCommand
{
public:
	FWaitForFrontierRaidSlotSwapPIECommand(
		const FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef& InState,
		const int32 InOccupiedSlotIndex,
		const int32 InEmptySlotIndex)
		: State(InState)
		, OccupiedSlotIndex(InOccupiedSlotIndex)
		, EmptySlotIndex(InEmptySlotIndex)
	{
	}

	virtual bool Update() override
	{
		if (State->bFailed)
		{
			return true;
		}

		if (FrontierInventoryNetworkPIETest::IsForwardedRaidSwapReady(OccupiedSlotIndex, EmptySlotIndex))
		{
			FrontierInventoryNetworkPIETest::ValidateRemotePrivacy(State);
			return true;
		}

		const double CurrentTime = FPlatformTime::Seconds();
		if (Deadline == 0.0)
		{
			Deadline = CurrentTime + FrontierInventoryNetworkPIETest::PhaseTimeoutSeconds;
		}
		else if (CurrentTime >= Deadline)
		{
			State->Fail(FString::Printf(
				TEXT("Timed out waiting for Controller RPC slot swap. Occupied=%d Empty=%d."),
				OccupiedSlotIndex,
				EmptySlotIndex));
			return true;
		}
		return false;
	}

private:
	FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef State;
	int32 OccupiedSlotIndex;
	int32 EmptySlotIndex;
	double Deadline = 0.0;
};

class FMutateFrontierVitalsPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FMutateFrontierVitalsPIECommand(const FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef& InState)
		: State(InState)
	{
	}

	virtual bool Update() override
	{
		using namespace FrontierInventoryNetworkPIETest;
		if (State->bFailed)
		{
			return true;
		}

		UWorld* ServerWorld = GetDedicatedServerWorld();
		AGameStateBase* GameState = ServerWorld ? ServerWorld->GetGameState() : nullptr;
		if (!GameState)
		{
			State->Fail(TEXT("Dedicated server disappeared before the vitals latency phase."));
			return true;
		}

		State->VitalsMutationStartedAt = FPlatformTime::Seconds();
		for (APlayerState* BasePlayerState : GameState->PlayerArray)
		{
			AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(BasePlayerState);
			UFrontierAbilitySystemComponent* AbilitySystem = PlayerState ? PlayerState->GetFrontierAbilitySystemComponent() : nullptr;
			if (!PlayerState || !AbilitySystem)
			{
				State->Fail(TEXT("A Frontier PlayerState is missing its ability system during the vitals latency phase."));
				return true;
			}

			AbilitySystem->SetNumericAttributeBase(UFrontierAttributeSet::GetHealthAttribute(), 0.0f);
			AbilitySystem->SetNumericAttributeBase(
				UFrontierAttributeSet::GetMaxStaminaAttribute(),
				UpdatedStaminaMarkerBase + PlayerState->GetPlayerId());
			AbilitySystem->SetNumericAttributeBase(
				UFrontierAttributeSet::GetStaminaAttribute(),
				UpdatedStaminaMarkerBase + PlayerState->GetPlayerId());
		}
		return true;
	}

private:
	FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef State;
};

class FWaitForUpdatedFrontierVitalsPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FWaitForUpdatedFrontierVitalsPIECommand(const FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef& InState)
		: State(InState)
	{
	}

	virtual bool Update() override
	{
		using namespace FrontierInventoryNetworkPIETest;
		if (State->bFailed)
		{
			return true;
		}

		const TArray<UWorld*> ClientWorlds = GetPIEWorlds(NM_Client);
		bool bAllClientsReady = ClientWorlds.Num() == 2;
		for (UWorld* ClientWorld : ClientWorlds)
		{
			bAllClientsReady &= ArePublicVitalsReady(ClientWorld, true);
		}

		const double ElapsedSeconds = FPlatformTime::Seconds() - State->VitalsMutationStartedAt;
		if (bAllClientsReady)
		{
			State->VitalsReplicationDelaySeconds = ElapsedSeconds;
			return true;
		}

		if (ElapsedSeconds >= MaxVitalsReplicationLatencySeconds)
		{
			State->Fail(FString::Printf(
				TEXT("Public vitals exceeded the %.0f ms replication budget (elapsed %.1f ms)."),
				MaxVitalsReplicationLatencySeconds * 1000.0,
				ElapsedSeconds * 1000.0));
			return true;
		}
		return false;
	}

private:
	FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef State;
};

class FMutateFrontierNetworkPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FMutateFrontierNetworkPIECommand(const FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef& InState)
		: State(InState)
	{
	}

	virtual bool Update() override
	{
		using namespace FrontierInventoryNetworkPIETest;
		if (State->bFailed)
		{
			return true;
		}

		UWorld* ServerWorld = GetDedicatedServerWorld();
		AGameStateBase* GameState = ServerWorld ? ServerWorld->GetGameState() : nullptr;
		if (!ServerWorld || !GameState)
		{
			State->Fail(TEXT("Dedicated server disappeared before the mutation phase."));
			return true;
		}

		for (APlayerState* BasePlayerState : GameState->PlayerArray)
		{
			AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(BasePlayerState);
			FFrontierInventorySlot LobbySlot;
			if (!PlayerState
				|| !PlayerState->GetStorageComponent()->GetSlot(PrivateSlotIndex, LobbySlot)
				|| !LobbySlot.bOccupied)
			{
				State->Fail(TEXT("Private inventory data was unavailable for the mutation phase."));
				return true;
			}

			LobbySlot.ItemInstance.EnhancementLevel = UpdatedPlayerMarkerBase + PlayerState->GetPlayerId();
			EFrontierEquipmentSlot EquippedSlot = EFrontierEquipmentSlot::None;
			for (const FFrontierLoadoutSlot& LoadoutSlot : PlayerState->GetLoadoutComponent()->GetLoadoutSlots())
			{
				if (LoadoutSlot.bOccupied)
				{
					EquippedSlot = LoadoutSlot.SlotType;
					break;
				}
			}
			if (!PlayerState->GetStorageComponent()->SetItemAtSlot(PrivateSlotIndex, LobbySlot.ItemInstance)
				|| !PlayerState->GetRaidInventoryComponent()->ClearSlot(RaidSlotIndex)
				|| EquippedSlot == EFrontierEquipmentSlot::None
				|| !PlayerState->GetLoadoutComponent()->UnequipItem(EquippedSlot))
			{
				State->Fail(FString::Printf(TEXT("Failed to mutate private data for PlayerState %d."), PlayerState->GetPlayerId()));
				return true;
			}
		}

		AFrontierLootContainerActor* LootActor = FindTestLootActor(ServerWorld, InitialLootSlotIndex, InitialLootMarker);
		UFrontierLootInventoryComponent* LootInventory = LootActor ? LootActor->GetLootInventoryComponent() : nullptr;
		FFrontierInventorySlot InitialLootSlot;
		if (!LootActor || !LootInventory || !LootInventory->GetSlot(InitialLootSlotIndex, InitialLootSlot))
		{
			State->Fail(TEXT("Public loot data was unavailable for the mutation phase."));
			return true;
		}

		InitialLootSlot.ItemInstance.EnhancementLevel = UpdatedLootMarker;
		if (!LootInventory->SetItemAtSlot(UpdatedLootSlotIndex, InitialLootSlot.ItemInstance)
			|| !LootInventory->ClearSlot(InitialLootSlotIndex))
		{
			State->Fail(TEXT("Failed to mutate the public loot Fast Array."));
			return true;
		}
		return true;
	}

private:
	FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef State;
};

class FWaitForUpdatedFrontierReplicationCommand final : public IAutomationLatentCommand
{
public:
	explicit FWaitForUpdatedFrontierReplicationCommand(const FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef& InState)
		: State(InState)
		, Deadline(FPlatformTime::Seconds() + FrontierInventoryNetworkPIETest::PhaseTimeoutSeconds)
	{
	}

	virtual bool Update() override
	{
		if (State->bFailed)
		{
			return true;
		}
		if (FrontierInventoryNetworkPIETest::IsUpdatedReplicationReady())
		{
			FrontierInventoryNetworkPIETest::ValidateRemotePrivacy(State);
			if (!State->bFailed && State->Test)
			{
				State->Test->AddInfo(FString::Printf(
					TEXT("Two-client PIE verified transactional replication without test-side ForceNetUpdate and public vitals latency of %.1f ms."),
					State->VitalsReplicationDelaySeconds * 1000.0));
			}
			return true;
		}
		if (FPlatformTime::Seconds() >= Deadline)
		{
			State->Fail(TEXT("Timed out waiting for changed and removed Fast Array entries to replicate to both clients."));
			return true;
		}
		return false;
	}

private:
	FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef State;
	double Deadline;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierPlayerControllerForwardingContractTest,
	"Frontier.Architecture.PlayerController.InventoryCommandForwarding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierPlayerControllerForwardingContractTest::RunTest(const FString& Parameters)
{
	const AFrontierPlayerController* NativeControllerCDO = GetDefault<AFrontierPlayerController>();
	TestNotNull(TEXT("Native Frontier PlayerController CDO exists"), NativeControllerCDO);
	if (NativeControllerCDO)
	{
		TestNotNull(TEXT("Native Controller owns the inventory command service"), NativeControllerCDO->GetInventoryCommandService());
		TestFalse(
			TEXT("Inventory command service does not replicate or own RPCs"),
			NativeControllerCDO->GetInventoryCommandService()->GetIsReplicated());
	}

	const FName InventoryRpcNames[] = {
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerEquipRaidInventoryItemToLoadout),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerUnequipLoadoutItemToRaidInventory),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerUnequipLoadoutItemToRaidInventorySlot),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerSwapRaidInventorySlots),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerSwapStorageSlots),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerUseRaidInventoryItem),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerStoreRaidItemInStorage),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerStoreRaidItemInStorageSlot),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerWithdrawLobbyItemToRaidInventory),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerWithdrawLobbyItemToRaidInventorySlot),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerDropRaidInventoryItem),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerDropLoadoutItem),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerPickupDroppedItem),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerPickupDroppedItemToRaidSlot),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerLootContainerItem),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerLootContainerItemToRaidSlot),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerLootContainerLoadoutItem),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerStoreRaidItemInLootContainer),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerStoreRaidItemInLootContainerSlot),
		GET_FUNCTION_NAME_CHECKED(AFrontierPlayerController, ServerSwapLootContainerSlots)
	};

	for (const FName RpcName : InventoryRpcNames)
	{
		const UFunction* RpcFunction = AFrontierPlayerController::StaticClass()->FindFunctionByName(RpcName);
		TestNotNull(*FString::Printf(TEXT("%s remains available"), *RpcName.ToString()), RpcFunction);
		if (RpcFunction)
		{
			const EFunctionFlags RequiredFlags = FUNC_Net | FUNC_NetServer | FUNC_NetReliable;
			TestTrue(
				*FString::Printf(TEXT("%s remains a reliable server RPC"), *RpcName.ToString()),
				RpcFunction->HasAllFunctionFlags(RequiredFlags));
		}
	}

	UClass* ProjectControllerClass = LoadClass<AFrontierPlayerController>(
		nullptr,
		TEXT("/Game/SY/BP/BP_FrontierPlayerController.BP_FrontierPlayerController_C"));
	TestNotNull(TEXT("Project Frontier PlayerController Blueprint loads"), ProjectControllerClass);
	if (ProjectControllerClass)
	{
		const AFrontierPlayerController* ProjectControllerCDO = ProjectControllerClass->GetDefaultObject<AFrontierPlayerController>();
		TestNotNull(TEXT("Project Controller Blueprint inherits the inventory command service"),
			ProjectControllerCDO ? ProjectControllerCDO->GetInventoryCommandService() : nullptr);
	}

	UClass* ProjectGameModeClass = LoadClass<AFrontierGameMode>(
		nullptr,
		TEXT("/Game/SY/BP/BP_FrontierGameMode.BP_FrontierGameMode_C"));
	TestNotNull(TEXT("Project Frontier GameMode Blueprint loads"), ProjectGameModeClass);
	if (ProjectGameModeClass)
	{
		const AGameModeBase* ProjectGameModeCDO = ProjectGameModeClass->GetDefaultObject<AGameModeBase>();
		TestTrue(
			TEXT("Project GameMode uses a Frontier PlayerController"),
			ProjectGameModeCDO
				&& ProjectGameModeCDO->PlayerControllerClass
				&& ProjectGameModeCDO->PlayerControllerClass->IsChildOf(AFrontierPlayerController::StaticClass()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierPlayerStateNetUpdatePolicyTest,
	"Frontier.Network.PlayerState.UpdatePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierPlayerStateNetUpdatePolicyTest::RunTest(const FString& Parameters)
{
	const AFrontierPlayerState* PlayerStateCDO = GetDefault<AFrontierPlayerState>();
	TestNotNull(TEXT("Frontier PlayerState CDO exists"), PlayerStateCDO);
	if (PlayerStateCDO)
	{
		TestEqual(TEXT("PlayerState maximum update frequency"), PlayerStateCDO->GetNetUpdateFrequency(), 30.0f);
		TestEqual(TEXT("PlayerState adaptive minimum update frequency"), PlayerStateCDO->GetMinNetUpdateFrequency(), 10.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierInventoryTwoClientPIETest,
	"Frontier.Inventory.Replication.TwoClientPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierInventoryTwoClientPIETest::RunTest(const FString& Parameters)
{
	const FrontierInventoryNetworkPIETest::FNetworkPIETestStateRef State =
		MakeShared<FrontierInventoryNetworkPIETest::FNetworkPIETestState>(this);

	ADD_LATENT_AUTOMATION_COMMAND(FStartFrontierNetworkPIECommand(State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForFrontierNetworkPIECommand(State));
	ADD_LATENT_AUTOMATION_COMMAND(FSeedFrontierNetworkPIECommand(State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForInitialFrontierReplicationCommand(State));
	ADD_LATENT_AUTOMATION_COMMAND(FRequestFrontierRaidSlotSwapPIECommand(
		State,
		FrontierInventoryNetworkPIETest::RaidSlotIndex,
		FrontierInventoryNetworkPIETest::ForwardedRaidSlotIndex));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForFrontierRaidSlotSwapPIECommand(
		State,
		FrontierInventoryNetworkPIETest::ForwardedRaidSlotIndex,
		FrontierInventoryNetworkPIETest::RaidSlotIndex));
	ADD_LATENT_AUTOMATION_COMMAND(FRequestFrontierRaidSlotSwapPIECommand(
		State,
		FrontierInventoryNetworkPIETest::ForwardedRaidSlotIndex,
		FrontierInventoryNetworkPIETest::RaidSlotIndex));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForFrontierRaidSlotSwapPIECommand(
		State,
		FrontierInventoryNetworkPIETest::RaidSlotIndex,
		FrontierInventoryNetworkPIETest::ForwardedRaidSlotIndex));
	ADD_LATENT_AUTOMATION_COMMAND(FMutateFrontierVitalsPIECommand(State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForUpdatedFrontierVitalsPIECommand(State));
	ADD_LATENT_AUTOMATION_COMMAND(FMutateFrontierNetworkPIECommand(State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForUpdatedFrontierReplicationCommand(State));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
