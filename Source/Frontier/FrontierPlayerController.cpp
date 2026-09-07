// Copyright Epic Games, Inc. All Rights Reserved.


#include "FrontierPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/OverlapResult.h"
#include "Frontier.h"
#include "Interaction/FrontierInteractInterface.h"
#include "Interaction/FrontierInteractableActor.h"
#include "Interaction/FrontierInteractionPolicy.h"
#include "Interaction/FrontierRaidDeployActor.h"
#include "Interaction/FrontierTutorialPosterActor.h"
#include "Game/FrontierGameMode.h"
#include "Game/FrontierGameState.h"
#include "Game/FrontierPlayerState.h"
#include "Loot/FrontierDroppedItemActor.h"
#include "Loot/FrontierLootContainerActor.h"
#include "UI/FrontierFocusedItemWidget.h"
#include "Kismet/GameplayStatics.h"
#include "UI/FrontierPlayerStatusWidget.h"
#include "UI/FrontierSpectatorStatusWidget.h"
#include "UI/FrontierInventoryWidget.h"
#include "UI/FrontierInGameInventoryWrapperWidget.h"
#include "UI/FrontierLootWidget.h"
#include "UI/FrontierDropLootWidget.h"
#include "UI/FrontierLoadingScreenSubsystem.h"
#include "UI/FrontierTeamStatusWidget.h"
#include "UI/FrontierTutorialPosterWidget.h"
#include "UI/FrontierSettingsWidget.h"
#include "UI/FrontierUISettings.h"
#include "Components/MeshComponent.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "EngineUtils.h"
#include "Components/FrontierInventoryComponent.h"
#include "Components/FrontierInventoryCommandService.h"
#include "Components/FrontierBackendInventoryReadyComponent.h"
#include "Components/FrontierSkillTreePersistenceComponent.h"
#include "Components/FrontierBackendProtocolComponent.h"
#include "Components/FrontierMinimapComponent.h"
#include "Components/FrontierCameraFeedbackComponent.h"
#include "Combat/FrontierNumberPopComponent_NiagaraText.h"
#include "Components/FrontierStorageComponent.h"
#include "Components/FrontierLoadoutComponent.h"
#include "Components/FrontierRaidInventoryComponent.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Character/FrontierBaseCharacter.h"
#include "Character/FrontierCharacterSelectionSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Extraction/FrontierExtractionZoneActor.h"
#include "Components/PrimitiveComponent.h"
#include "Warning/AttackWarningSubsystem.h"
#include "Warning/FrontierSkillTrajectorySubsystem.h"
#include "Progression/FrontierRaidExperienceSubsystem.h"
#include "Progression/FrontierRaidSessionSubsystem.h"
#include "NiagaraSystem.h"
#include "Weapons/FrontierWeaponBase.h"
#include "Sound/SoundBase.h"

AFrontierPlayerController::AFrontierPlayerController()
{
	InventoryCommandService = CreateDefaultSubobject<UFrontierInventoryCommandService>(TEXT("InventoryCommandService"));
	BackendInventoryReadyComponent = CreateDefaultSubobject<UFrontierBackendInventoryReadyComponent>(TEXT("BackendInventoryReadyComponent"));
	NumberPopComponent = CreateDefaultSubobject<UFrontierNumberPopComponent_NiagaraText>(TEXT("NumberPopComponent"));
	SkillTreePersistenceComponent = CreateDefaultSubobject<UFrontierSkillTreePersistenceComponent>(TEXT("SkillTreePersistenceComponent"));
	BackendProtocolComponent = CreateDefaultSubobject<UFrontierBackendProtocolComponent>(TEXT("BackendProtocolComponent"));
	MinimapComponent = CreateDefaultSubobject<UFrontierMinimapComponent>(TEXT("MinimapComponent"));
	CameraFeedbackComponent = CreateDefaultSubobject<UFrontierCameraFeedbackComponent>(TEXT("CameraFeedbackComponent"));
}

void AFrontierPlayerController::ClientRevealDamagedTarget_Implementation(
	AFrontierBaseCharacter* DamagedTarget,
	const float HealthPercent)
{
	if (IsValid(DamagedTarget))
	{
		DamagedTarget->RevealCombatOverheadLocally(FMath::Clamp(HealthPercent, 0.0f, 1.0f));
	}
}

void AFrontierPlayerController::ClientPlayHeavyHitFeedback_Implementation(const FVector& HitLocation)
{
	if (IsLocalController() && CameraFeedbackComponent)
	{
		CameraFeedbackComponent->PlayHeavyHitFeedback(HitLocation);
	}
}

void AFrontierPlayerController::ClientBeginInteractionChannel_Implementation(
	const float Duration,
	const FVector TargetWorldLocation)
{
	if (!IsLocalController() || !GetWorld())
	{
		return;
	}

	bLocalInteractionChannelActive = true;
	LocalInteractionStartLocation = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
	LocalInteractionStartTime = GetWorld()->GetTimeSeconds();
	LocalInteractionDuration = FMath::Max(Duration, 0.1f);
	if (AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->BeginInteractionFacing(TargetWorldLocation);
	}
	BeginLocalInteractionPresentation();
	if (PlayerStatusWidget)
	{
		PlayerStatusWidget->HideInteractionPrompt();
		PlayerStatusWidget->ShowInteractionProgress();
	}
}

void AFrontierPlayerController::ClientEndInteractionChannel_Implementation(const bool bCompleted)
{
	EndLocalInteractionChannelLocally();
}

void AFrontierPlayerController::EndLocalInteractionChannelLocally()
{
	EndLocalInteractionPresentation();
	bLocalInteractionChannelActive = false;
	LocalInteractionStartLocation = FVector::ZeroVector;
	LocalInteractionStartTime = 0.0f;
	LocalInteractionDuration = 0.0f;
	if (PlayerStatusWidget)
	{
		PlayerStatusWidget->HideInteractionProgress();
	}
}

void AFrontierPlayerController::BeginLocalInteractionPresentation()
{
	AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(GetPawn());
	if (!PlayerCharacter)
	{
		return;
	}

	if (AFrontierWeaponBase* CurrentWeapon = PlayerCharacter->GetCurrentWeapon())
	{
		if (USkeletalMeshComponent* WeaponMesh = CurrentWeapon->GetWeaponMesh())
		{
			InteractionHiddenWeapon = CurrentWeapon;
			bInteractionWeaponWasVisible = WeaponMesh->IsVisible();
			WeaponMesh->SetVisibility(false, true);
		}
	}

	if (InteractionMontage)
	{
		bInteractionMontageStarted = PlayerCharacter->PlayAnimMontage(InteractionMontage) > 0.0f;
	}
}

void AFrontierPlayerController::EndLocalInteractionPresentation()
{
	if (AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->EndInteractionFacing();
		if (InteractionMontage && bInteractionMontageStarted)
		{
			PlayerCharacter->StopAnimMontage(InteractionMontage);
		}
	}
	bInteractionMontageStarted = false;

	if (InteractionHiddenWeapon)
	{
		if (USkeletalMeshComponent* WeaponMesh = InteractionHiddenWeapon->GetWeaponMesh())
		{
			WeaponMesh->SetVisibility(bInteractionWeaponWasVisible, true);
		}
	}
	InteractionHiddenWeapon = nullptr;
	bInteractionWeaponWasVisible = true;
}

void AFrontierPlayerController::AcknowledgePossession(APawn* NewPawn)
{
	Super::AcknowledgePossession(NewPawn);
	ApplyLocalCharacterAppearance(NewPawn);
	RefreshPlayerStatusWidgetBinding();
	TryNotifyClientGameplayReady();
}

void AFrontierPlayerController::BeginPlay()
{
	Super::BeginPlay();
	CacheUIAudioSettings();

	FRONTIER_LOG(Log, TEXT("FrontierPlayerController BeginPlay. Controller=%s IsLocal=%d"), *GetNameSafe(this), IsLocalController() ? 1 : 0);
	FRONTIER_LOG(Log, TEXT("UI class bindings. ControllerClass=%s InventoryWrapperWidgetClass=%s InventoryWidgetClass=%s"),
		*GetClass()->GetName(),
		*GetNameSafe(InventoryWrapperWidgetClass),
		*GetNameSafe(InventoryWidgetClass));

	ApplyLocalCharacterAppearance(GetPawn());

	if (IsLocalController() && !PlayerStatusWidget)
	{
		TSubclassOf<UFrontierPlayerStatusWidget> WidgetClass = PlayerStatusWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UFrontierPlayerStatusWidget::StaticClass();
		}
		PlayerStatusWidget = CreateWidget<UFrontierPlayerStatusWidget>(this, WidgetClass);
		if (PlayerStatusWidget)
		{
			PlayerStatusWidget->SetOwningPlayer(this);
			PlayerStatusWidget->AddToViewport(0);
			PlayerStatusWidget->SetVisibility(ESlateVisibility::Visible);
			FRONTIER_LOG(Log, TEXT("Player status widget created successfully. Widget=%s"), *GetNameSafe(PlayerStatusWidget));
		}
		else
		{
			FRONTIER_LOG(Warning, TEXT("Player status widget creation failed."));
		}
	}

	if (IsLocalController())
	{
		if (SkillTreePersistenceComponent)
		{
			SkillTreePersistenceComponent->StartSkillTreeSync();
		}

		if (IsInLobbyLevel())
		{
			bSpectatorInputModeApplied = false;
			bRaidLoadingScreenActive = false;

			if (UGameInstance* GameInstance = GetGameInstance())
			{
				if (UFrontierLoadingScreenSubsystem* LoadingScreen =
					GameInstance->GetSubsystem<UFrontierLoadingScreenSubsystem>())
				{
					LoadingScreen->StopRaidLoadingScreen();
				}
			}

			if (CurrentLootSideWidget)
			{
				CurrentLootSideWidget->RemoveFromParent();
				CurrentLootSideWidget = nullptr;
			}

			if (CurrentLobbySideWidget)
			{
				CurrentLobbySideWidget->RemoveFromParent();
				CurrentLobbySideWidget = nullptr;
			}

			bIsLootUIOpened = false;
			CurrentOpenedLootContainer = nullptr;
			bShowMouseCursor = false;
			ApplyModalGameplayInputBlock(false);
			SetInputMode(FInputModeGameOnly());
		}

		EnsureTeamStatusWidget();
		if (BackendInventoryReadyComponent)
		{
			BackendInventoryReadyComponent->StartObservation(IsInLobbyLevel());
		}
		EnsureSpectatorStatusWidget();
		RefreshSpectatorStatusWidgetVisibility();

		if (!IsInLobbyLevel())
		{
			SetRaidLoadingScreenState(true);
			if (UGameInstance* GameInstance = GetGameInstance())
			{
				if (UFrontierLoadingScreenSubsystem* LoadingScreen =
					GameInstance->GetSubsystem<UFrontierLoadingScreenSubsystem>())
				{
					LoadingScreen->SetRaidLoadingProgress(
						0.8f,
						NSLOCTEXT("FrontierLoading", "RaidMapInitializing", "레이드 맵 초기화 중..."));
				}
			}
			TryNotifyClientGameplayReady();
		}
	}
}

void AFrontierPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopNearbyDroppedLootTracking();
	EndLocalInteractionPresentation();
	if (SettingsWidget)
	{
		SettingsWidget->RemoveFromParent();
		SettingsWidget = nullptr;
	}
	if (HasAuthority())
	{
		ClearInteractionChannelServer();
	}
	Super::EndPlay(EndPlayReason);
}

void AFrontierPlayerController::SetWeaponAttackCharge(const float ChargeProgress)
{
	if (IsLocalController() && PlayerStatusWidget)
	{
		PlayerStatusWidget->SetWeaponAttackCharge(ChargeProgress);
	}
}

void AFrontierPlayerController::ApplyLocalCharacterAppearance(APawn* TargetPawn)
{
	// Local-only bridge. Backend selection should later be replicated per player and applied in OnRep.
	if (!IsLocalController())
	{
		return;
	}

	AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(TargetPawn);
	UGameInstance* GameInstance = GetGameInstance();
	UFrontierCharacterSelectionSubsystem* SelectionSubsystem = GameInstance
		? GameInstance->GetSubsystem<UFrontierCharacterSelectionSubsystem>()
		: nullptr;
	if (!PlayerCharacter || !SelectionSubsystem)
	{
		return;
	}

	SelectionSubsystem->InitializeCharacterSelection();
	const EFrontierCharacterType CharacterType =
		SelectionSubsystem->GetSelectedCharacterType();
	PlayerCharacter->RequestCharacterType(CharacterType);
	FRONTIER_LOG(
		Log,
		TEXT("Requested possessed character appearance. CharacterType=%s"),
		*UEnum::GetValueAsString(CharacterType));
}

void AFrontierPlayerController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		UpdateInteractionChannelServer();
	}

	if (IsLocalController())
	{
		UpdateLocalInteractionProgress();
		EnsureFocusedItemWidget();
		EnsureSpectatorStatusWidget();
		RefreshSpectatorStatusWidgetVisibility();
		UpdateFocusedInteractable();
		UpdateFocusedDroppedItem();
	}
}

void AFrontierPlayerController::RefreshPlayerStatusWidgetBinding()
{
	

	if (PlayerStatusWidget)
	{
		PlayerStatusWidget->RebindToOwningPawnAttributes();
	}
}

void AFrontierPlayerController::TryNotifyClientGameplayReady()
{
	if (!IsLocalController()
		|| IsInLobbyLevel()
		|| !bRaidLoadingScreenActive
		|| bClientGameplayReadySent)
	{
		return;
	}

	AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(GetPawn());
	AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>();
	UFrontierAbilitySystemComponent* AbilitySystemComponent =
		FrontierPlayerState ? FrontierPlayerState->GetFrontierAbilitySystemComponent() : nullptr;
	if (!PlayerCharacter
		|| !FrontierPlayerState
		|| !AbilitySystemComponent
		|| !FrontierPlayerState->GetFrontierAttributeSet()
		|| AbilitySystemComponent->GetOwnerActor() != FrontierPlayerState
		|| AbilitySystemComponent->GetAvatarActor() != PlayerCharacter)
	{
		return;
	}

	bClientGameplayReadySent = true;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFrontierLoadingScreenSubsystem* LoadingScreen =
			GameInstance->GetSubsystem<UFrontierLoadingScreenSubsystem>())
		{
			LoadingScreen->SetRaidLoadingProgress(
				1.0f,
				NSLOCTEXT("FrontierLoading", "RaidReady", "레이드 준비 완료"));
			LoadingScreen->StopRaidLoadingScreen();
		}
	}
	FRONTIER_LOG(
		Log,
		TEXT("Client gameplay readiness confirmed after ASC initialization. Controller=%s PlayerState=%s Pawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(FrontierPlayerState),
		*GetNameSafe(PlayerCharacter));

	if (HasAuthority())
	{
		if (AFrontierGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AFrontierGameMode>() : nullptr)
		{
			GameMode->NotifyRaidClientGameplayReady(this);
		}
		return;
	}

	ServerNotifyClientGameplayReady();
}

void AFrontierPlayerController::ServerNotifyClientGameplayReady_Implementation()
{
	if (IsInLobbyLevel())
	{
		return;
	}

	AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(GetPawn());
	AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>();
	UFrontierAbilitySystemComponent* AbilitySystemComponent =
		FrontierPlayerState ? FrontierPlayerState->GetFrontierAbilitySystemComponent() : nullptr;
	if (!PlayerCharacter
		|| !FrontierPlayerState
		|| !AbilitySystemComponent
		|| !FrontierPlayerState->GetFrontierAttributeSet()
		|| AbilitySystemComponent->GetOwnerActor() != FrontierPlayerState
		|| AbilitySystemComponent->GetAvatarActor() != PlayerCharacter)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Server rejected client gameplay readiness because authoritative ASC state is incomplete. Controller=%s PlayerState=%s Pawn=%s"),
			*GetNameSafe(this),
			*GetNameSafe(FrontierPlayerState),
			*GetNameSafe(PlayerCharacter));
		return;
	}

	if (AFrontierGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AFrontierGameMode>() : nullptr)
	{
		GameMode->NotifyRaidClientGameplayReady(this);
	}
}

void AFrontierPlayerController::RefreshTeamStatusWidget()
{
	
	EnsureTeamStatusWidget();
	if (TeamStatusWidget)
	{
		TeamStatusWidget->RefreshTeamMembers();
	}
}

bool AFrontierPlayerController::IsInLobbyContext() const
{
	return IsInLobbyLevel();
}

bool AFrontierPlayerController::TryHandleInventoryDragDropOnUI(const FVector2D& ScreenSpacePosition, UFrontierInventoryDragDropOperation* DragOperation) const
{
	if (!DragOperation || !InventoryWrapperWidget || !InventoryWrapperWidget->IsInViewport())
	{
		return false;
	}

	return const_cast<UFrontierInGameInventoryWrapperWidget*>(InventoryWrapperWidget.Get())->TryHandlePanelDrop(ScreenSpacePosition, DragOperation);
}


void AFrontierPlayerController::ToggleInventoryUI()
{
	

	if (!IsLocalController())
	{
		return;
	}

	if (bIsLootUIOpened)
	{
		CloseLootUI();
		return;
	}

	if (InventoryWrapperWidget && InventoryWrapperWidget->IsInViewport())
	{
		CloseInventoryWidgetLocal();
		return;
	}

	UFrontierInGameInventoryWrapperWidget* WrapperWidget = EnsureInventoryWrapperWidget();
	UFrontierInventoryWidget* PlayerInventoryWidget = WrapperWidget ? WrapperWidget->GetFixedInventoryWidget() : nullptr;
	if (!PlayerInventoryWidget)
	{
		PlayerInventoryWidget = EnsureInventoryWidget();
	}
	if (!PlayerInventoryWidget)
	{
		return;
	}

	PlayerInventoryWidget->SetObservedPlayerState(GetPlayerState<AFrontierPlayerState>());
	PlayerInventoryWidget->SetInventoryWidgetMode(EInventoryWidgetMode::PlayerInventory);

	if (WrapperWidget)
	{
		WrapperWidget->SetObservedPlayerState(GetPlayerState<AFrontierPlayerState>());
		WrapperWidget->ClearPanelWidgets();
		WrapperWidget->AddToViewport(10);
		WrapperWidget->SetVisibility(ESlateVisibility::Visible);
		StartNearbyDroppedLootTracking(WrapperWidget);
	}
	else if (!PlayerInventoryWidget->IsInViewport())
	{
		PlayerInventoryWidget->AddToViewport(10);
		PlayerInventoryWidget->SetVisibility(ESlateVisibility::Visible);
	}

	RefreshModalInputState();
	PlayLocalUIOpenSound(LoadedInventoryOpenSound);
}

void AFrontierPlayerController::OpenLobbyStorageUI()
{
	if (!IsLocalController())
	{
		return;
	}

	OpenLobbyStorageUILocal();
}

void AFrontierPlayerController::ClientShowLocalAttackWarning_Implementation(const FGuid WarningId, const FAttackWarningData& WarningData)
{
	if (!IsLocalController() || !GetLocalPlayer())
	{
		return;
	}

	if (UAttackWarningSubsystem* WarningSubsystem = GetLocalPlayer()->GetSubsystem<UAttackWarningSubsystem>())
	{
		WarningSubsystem->ShowWarning(WarningId, WarningData);
	}
}

void AFrontierPlayerController::ClientHideLocalAttackWarning_Implementation(const FGuid WarningId)
{
	if (!IsLocalController() || !GetLocalPlayer())
	{
		return;
	}

	if (UAttackWarningSubsystem* WarningSubsystem = GetLocalPlayer()->GetSubsystem<UAttackWarningSubsystem>())
	{
		WarningSubsystem->HideWarning(WarningId);
	}
}

void AFrontierPlayerController::ClientShowLocalSkillTrajectory_Implementation(
	const FGuid TrajectoryId,
	UNiagaraSystem* TrajectoryNiagaraSystem,
	const TArray<FVector>& TrajectoryPoints,
	const float Duration)
{
	if (!IsLocalController() || !TrajectoryId.IsValid() || !TrajectoryNiagaraSystem || TrajectoryPoints.Num() < 2)
	{
		return;
	}

	if (GetLocalPlayer())
	{
		if (UFrontierSkillTrajectorySubsystem* TrajectorySubsystem = GetLocalPlayer()->GetSubsystem<UFrontierSkillTrajectorySubsystem>())
		{
			TrajectorySubsystem->ShowTrajectory(TrajectoryId, TrajectoryNiagaraSystem, TrajectoryPoints, Duration);
		}
	}
}

void AFrontierPlayerController::ClientHideLocalSkillTrajectory_Implementation(const FGuid TrajectoryId)
{
	if (!IsLocalController() || !TrajectoryId.IsValid() || !GetLocalPlayer())
	{
		return;
	}

	if (UFrontierSkillTrajectorySubsystem* TrajectorySubsystem = GetLocalPlayer()->GetSubsystem<UFrontierSkillTrajectorySubsystem>())
	{
		TrajectorySubsystem->HideTrajectory(TrajectoryId);
	}
}

void AFrontierPlayerController::ClientOpenLobbyStorageUI_Implementation()
{
	OpenLobbyStorageUI();
}

bool AFrontierPlayerController::IsInventoryUIOpen() const
{
	return (InventoryWrapperWidget && InventoryWrapperWidget->IsInViewport())
		|| (InventoryWidget && InventoryWidget->IsInViewport());
}

bool AFrontierPlayerController::IsLootUIOpen() const
{
	return bIsLootUIOpened;
}

UFrontierInventoryCommandService* AFrontierPlayerController::GetInventoryCommandService() const
{
	return InventoryCommandService;
}

void AFrontierPlayerController::RequestInteraction()
{
	if (IsLocalController() && bIsLootUIOpened)
	{
		CloseLootUI();
		return;
	}

	if (bLocalInteractionChannelActive)
	{
		return;
	}

	if (IsInteractionBlockedByExtractionZone())
	{
		FRONTIER_LOG(VeryVerbose, TEXT("Interaction ignored while the player is inside an extraction zone. Controller=%s"),
			*GetNameSafe(this));
		return;
	}

	if (FocusedDroppedItemActor)
	{
		if (HasAuthority())
		{
			ServerRequestFocusedInteraction_Implementation(FocusedDroppedItemActor);
		}
		else
		{
			ServerRequestFocusedInteraction(FocusedDroppedItemActor);
		}
		return;
	}

	if (IsLocalController())
	{
		if (const AFrontierRaidDeployActor* RaidDeployActor = Cast<AFrontierRaidDeployActor>(FocusedInteractableActor))
		{
			const AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>();
			if (RaidDeployActor && FrontierPlayerState && FrontierPlayerState->GetTeamId() <= 0)
			{
				ShowLocalPlayerStatusWarning(TEXT("숫자키를 눌러 팀을 먼저 정하세요."));
				return;
			}
		}
	}

	if (FocusedInteractableActor)
	{
		if (HasAuthority())
		{
			ServerRequestFocusedInteraction_Implementation(FocusedInteractableActor);
		}
		else
		{
			ServerRequestFocusedInteraction(FocusedInteractableActor);
		}
		return;
	}

	if (HasAuthority())
	{
		ServerRequestInteraction_Implementation();
		return;
	}

	if (!HasAuthority())
	{
		ServerRequestInteraction();	
	}
}

void AFrontierPlayerController::CancelInteractionChannelForLocalAction()
{
	if (IsLocalController())
	{
		EndLocalInteractionChannelLocally();
	}

	if (HasAuthority())
	{
		CancelInteractionChannelServer(TEXT("player attack input"));
	}
	else
	{
		ServerCancelInteractionChannel();
	}
}

void AFrontierPlayerController::ServerRequestInteraction_Implementation()
{
	if (IsInteractionBlockedByExtractionZone())
	{
		return;
	}

	AActor* InteractableActor = FindNearestInteractableActor();
	if (InteractableActor)
	{
		BeginInteractionChannelServer(InteractableActor);
	}
	else
	{
		FRONTIER_LOG(Log, TEXT("No interactable actor was found near the player."));
	}
}


AActor* AFrontierPlayerController::FindNearestInteractableActor() const
{
	const APawn* PlayerPawn = GetPawn();
	if (!PlayerPawn || !GetWorld() || IsInteractionBlockedByExtractionZone())
	{
		return nullptr;
	}

	AActor* NearestInteractableActor = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AFrontierInteractableActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		AFrontierInteractableActor* Candidate = *Iterator;
		if (!Candidate || Candidate->IsA<AFrontierExtractionZoneActor>())
		{
			continue;
		}

		IFrontierInteractInterface* Interactable = Cast<IFrontierInteractInterface>(Candidate);
		if (!Interactable || !Interactable->CanInteract(this))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(PlayerPawn->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestInteractableActor = Candidate;
			NearestDistanceSquared = DistanceSquared;
		}
	}

	return NearestInteractableActor;
}

const AFrontierExtractionZoneActor* AFrontierPlayerController::FindContainingExtractionZone() const
{
	const APawn* PlayerPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!PlayerPawn || !World)
	{
		return nullptr;
	}

	for (TActorIterator<AFrontierExtractionZoneActor> Iterator(World); Iterator; ++Iterator)
	{
		const AFrontierExtractionZoneActor* ExtractionZone = *Iterator;
		if (ExtractionZone && ExtractionZone->IsActorInsideExtractionZone(PlayerPawn))
		{
			return ExtractionZone;
		}
	}

	return nullptr;
}

bool AFrontierPlayerController::IsInteractionBlockedByExtractionZone() const
{
	return FindContainingExtractionZone() != nullptr;
}


void AFrontierPlayerController::ServerEquipRaidInventoryItemToLoadout_Implementation(const int32 SourceSlotIndex, const EFrontierEquipmentSlot SlotType)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->EquipRaidInventoryItemToLoadout(SourceSlotIndex, SlotType);
	}
}

void AFrontierPlayerController::ServerUnequipLoadoutItemToRaidInventory_Implementation(const EFrontierEquipmentSlot SlotType)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->UnequipLoadoutItemToRaidInventory(SlotType);
	}
}

void AFrontierPlayerController::ServerUnequipLoadoutItemToRaidInventorySlot_Implementation(const EFrontierEquipmentSlot SlotType, const int32 TargetSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->UnequipLoadoutItemToRaidInventorySlot(SlotType, TargetSlotIndex);
	}
}

void AFrontierPlayerController::ServerSwapRaidInventorySlots_Implementation(const int32 SourceSlotIndex, const int32 TargetSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->SwapRaidInventorySlots(SourceSlotIndex, TargetSlotIndex);
	}
}

void AFrontierPlayerController::ServerSwapStorageSlots_Implementation(const int32 SourceSlotIndex, const int32 TargetSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->SwapStorageSlots(SourceSlotIndex, TargetSlotIndex);
	}
}

void AFrontierPlayerController::ServerUseRaidInventoryItem_Implementation(const int32 SourceSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->UseRaidInventoryItem(SourceSlotIndex);
	}
}

void AFrontierPlayerController::ServerRegisterQuickSlotFromInventory_Implementation(
	const int32 QuickSlotIndex,
	const int32 InventorySlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->RegisterQuickSlotFromRaidInventory(QuickSlotIndex, InventorySlotIndex);
	}
}

void AFrontierPlayerController::ServerClearQuickSlot_Implementation(const int32 QuickSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->ClearQuickSlot(QuickSlotIndex);
	}
}

void AFrontierPlayerController::ServerMoveQuickSlot_Implementation(
	const int32 SourceQuickSlotIndex,
	const int32 TargetQuickSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->MoveOrSwapQuickSlots(SourceQuickSlotIndex, TargetQuickSlotIndex);
	}
}

void AFrontierPlayerController::ServerUseQuickSlot_Implementation(const int32 QuickSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->UseQuickSlot(QuickSlotIndex);
	}
}

void AFrontierPlayerController::ServerStoreRaidItemInStorage_Implementation(const int32 SourceRaidSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->StoreRaidItemInStorage(SourceRaidSlotIndex);
	}
}

void AFrontierPlayerController::ServerStoreRaidItemInStorageSlot_Implementation(const int32 SourceRaidSlotIndex, const int32 TargetLobbySlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->StoreRaidItemInStorageSlot(SourceRaidSlotIndex, TargetLobbySlotIndex);
	}
}

void AFrontierPlayerController::ServerWithdrawLobbyItemToRaidInventory_Implementation(const int32 SourceLobbySlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->WithdrawLobbyItemToRaidInventory(SourceLobbySlotIndex);
	}
}

void AFrontierPlayerController::ServerWithdrawLobbyItemToRaidInventorySlot_Implementation(const int32 SourceLobbySlotIndex, const int32 TargetRaidSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->WithdrawLobbyItemToRaidInventorySlot(SourceLobbySlotIndex, TargetRaidSlotIndex);
	}
}

void AFrontierPlayerController::ServerRequestFocusedInteraction_Implementation(AActor* FocusedActor)
{
	if (IsInteractionBlockedByExtractionZone() || PendingInteractionActor)
	{
		return;
	}

	const bool bFocusedTargetStarted = BeginInteractionChannelServer(FocusedActor);
	if (FFrontierInteractionPolicy::ShouldRetryWithAuthoritativeTarget(
		bFocusedTargetStarted,
		PendingInteractionActor != nullptr,
		IsInteractionBlockedByExtractionZone()))
	{
		// Client focus can be stale because movement and replicated actor state are
		// evaluated independently. Re-resolve the nearest valid target on authority.
		if (AActor* FallbackActor = FindNearestInteractableActor())
		{
			BeginInteractionChannelServer(FallbackActor);
		}
	}
}

void AFrontierPlayerController::ServerCancelInteractionChannel_Implementation()
{
	CancelInteractionChannelServer(TEXT("player attack input"));
}

bool AFrontierPlayerController::BeginInteractionChannelServer(AActor* InteractableActor)
{
	IFrontierInteractInterface* Interactable = IsValid(InteractableActor)
		? Cast<IFrontierInteractInterface>(InteractableActor)
		: nullptr;
	AFrontierBaseCharacter* PlayerCharacter = Cast<AFrontierBaseCharacter>(GetPawn());
	FFrontierInteractionChannelContext Context;
	Context.bHasAuthority = HasAuthority();
	Context.bHasPendingInteraction = PendingInteractionActor != nullptr;
	Context.bTargetValid = IsValid(InteractableActor) && Interactable != nullptr;
	Context.bTargetIsExtractionZone = IsValid(InteractableActor)
		&& InteractableActor->IsA<AFrontierExtractionZoneActor>();
	Context.bPlayerValid = PlayerCharacter != nullptr;
	Context.bPlayerDead = PlayerCharacter && PlayerCharacter->IsDead();
	Context.bTargetCanInteract = Interactable && Interactable->CanInteract(this);
	Context.bPlayerInsideExtractionZone = IsInteractionBlockedByExtractionZone();
	if (!FFrontierInteractionPolicy::CanBeginChannel(Context))
	{
		return false;
	}

	if (Interactable->IsInstantInteraction(this))
	{
		const FString InteractableName = GetNameSafe(InteractableActor);
		Interactable->Interacted(this);
		FRONTIER_LOG(VeryVerbose, TEXT("Instant interaction completed. Controller=%s Actor=%s"),
			*GetNameSafe(this), *InteractableName);
		return true;
	}

	PendingInteractionActor = InteractableActor;
	PendingInteractionStartLocation = PlayerCharacter->GetActorLocation();
	PendingInteractionStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	PendingInteractionStartDamageTime = PlayerCharacter->GetLastDamageReceivedTime();
	const float ActorDuration = Interactable->GetInteractionDuration(this);
	PendingInteractionDuration = FMath::Max(
		ActorDuration > 0.0f ? ActorDuration : InteractionChannelDuration,
		0.1f);
	const FVector InteractionLocation = Interactable->GetInteractionWorldLocation();
	const FVector TargetWorldLocation = InteractionLocation.IsNearlyZero()
		? InteractableActor->GetActorLocation()
		: InteractionLocation;
	if (AFrontierPlayerCharacter* FrontierPlayerCharacter = Cast<AFrontierPlayerCharacter>(PlayerCharacter))
	{
		FrontierPlayerCharacter->BeginInteractionFacing(TargetWorldLocation);
	}
	ClientBeginInteractionChannel(PendingInteractionDuration, TargetWorldLocation);

	FRONTIER_LOG(VeryVerbose, TEXT("Interaction channel started. Controller=%s Actor=%s Duration=%.2f"),
		*GetNameSafe(this), *GetNameSafe(InteractableActor), PendingInteractionDuration);
	return true;
}

void AFrontierPlayerController::UpdateInteractionChannelServer()
{
	if (!PendingInteractionActor)
	{
		return;
	}

	AFrontierBaseCharacter* PlayerCharacter = Cast<AFrontierBaseCharacter>(GetPawn());
	IFrontierInteractInterface* Interactable = Cast<IFrontierInteractInterface>(PendingInteractionActor);
	if (!PlayerCharacter || PlayerCharacter->IsDead() || !IsValid(PendingInteractionActor) || !Interactable)
	{
		CancelInteractionChannelServer(TEXT("invalid player or target"));
		return;
	}

	if (!Interactable->CanInteract(this))
	{
		CancelInteractionChannelServer(TEXT("target no longer interactable"));
		return;
	}

	if (IsInteractionBlockedByExtractionZone())
	{
		CancelInteractionChannelServer(TEXT("player entered extraction zone"));
		return;
	}

	if (FFrontierInteractionPolicy::HasExceededMovementTolerance(
		PendingInteractionStartLocation,
		PlayerCharacter->GetActorLocation(),
		InteractionMovementCancelDistance))
	{
		CancelInteractionChannelServer(TEXT("player moved"));
		return;
	}

	if (PlayerCharacter->GetLastDamageReceivedTime() > PendingInteractionStartDamageTime + KINDA_SMALL_NUMBER)
	{
		CancelInteractionChannelServer(TEXT("player damaged"));
		return;
	}

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : PendingInteractionStartTime;
	if ((CurrentTime - PendingInteractionStartTime) >= PendingInteractionDuration)
	{
		CompleteInteractionChannelServer();
	}
}

void AFrontierPlayerController::CancelInteractionChannelServer(const TCHAR* Reason)
{
	if (!PendingInteractionActor)
	{
		return;
	}

	FRONTIER_LOG(VeryVerbose, TEXT("Interaction channel cancelled. Controller=%s Actor=%s Reason=%s"),
		*GetNameSafe(this), *GetNameSafe(PendingInteractionActor), Reason ? Reason : TEXT("unknown"));
	ClearInteractionChannelServer();
	ClientEndInteractionChannel(false);
}

void AFrontierPlayerController::CompleteInteractionChannelServer()
{
	AActor* CompletedActor = PendingInteractionActor.Get();
	IFrontierInteractInterface* Interactable = Cast<IFrontierInteractInterface>(CompletedActor);
	ClearInteractionChannelServer();
	ClientEndInteractionChannel(true);

	if (Interactable && IsValid(CompletedActor) && Interactable->CanInteract(this))
	{
		Interactable->Interacted(this);
		FRONTIER_LOG(VeryVerbose, TEXT("Interaction channel completed. Controller=%s Actor=%s"),
			*GetNameSafe(this), *GetNameSafe(CompletedActor));
	}
}

void AFrontierPlayerController::ClearInteractionChannelServer()
{
	if (AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->EndInteractionFacing();
	}
	PendingInteractionActor = nullptr;
	PendingInteractionStartLocation = FVector::ZeroVector;
	PendingInteractionStartTime = 0.0f;
	PendingInteractionStartDamageTime = -1000.0f;
	PendingInteractionDuration = 0.0f;
}

void AFrontierPlayerController::UpdateLocalInteractionProgress()
{
	if (!bLocalInteractionChannelActive || !GetWorld())
	{
		return;
	}

	if (const APawn* ControlledPawn = GetPawn())
	{
		if (FVector::DistSquared(ControlledPawn->GetActorLocation(), LocalInteractionStartLocation)
			> FMath::Square(InteractionMovementCancelDistance))
		{
			CancelInteractionChannelForLocalAction();
			return;
		}
	}

	const float ElapsedTime = GetWorld()->GetTimeSeconds() - LocalInteractionStartTime;
	const float Progress = LocalInteractionDuration > 0.0f
		? FMath::Clamp(ElapsedTime / LocalInteractionDuration, 0.0f, 1.0f)
		: 1.0f;
	if (PlayerStatusWidget)
	{
		PlayerStatusWidget->SetInteractionProgress(Progress);
	}
}

void AFrontierPlayerController::ServerDropRaidInventoryItem_Implementation(const int32 SourceSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->DropRaidInventoryItem(SourceSlotIndex);
	}
}

void AFrontierPlayerController::ServerDropLoadoutItem_Implementation(const EFrontierEquipmentSlot SlotType)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->DropLoadoutItem(SlotType);
	}
}

void AFrontierPlayerController::ServerPickupDroppedItem_Implementation(AFrontierDroppedItemActor* DroppedItem)
{
	if (!IsValid(DroppedItem) || IsInteractionBlockedByExtractionZone() || PendingInteractionActor)
	{
		FRONTIER_LOG(VeryVerbose, TEXT("Dropped-item pickup rejected before interaction validation. Controller=%s Item=%s"),
			*GetNameSafe(this), *GetNameSafe(DroppedItem));
		return;
	}

	if (!BeginInteractionChannelServer(DroppedItem))
	{
		FRONTIER_LOG(VeryVerbose, TEXT("Dropped-item pickup failed interaction validation. Controller=%s Item=%s"),
			*GetNameSafe(this), *GetNameSafe(DroppedItem));
	}
}

void AFrontierPlayerController::ServerPickupDroppedItemToRaidSlot_Implementation(
	AFrontierDroppedItemActor* DroppedItem,
	const int32 TargetSlotIndex)
{
	const AFrontierBaseCharacter* PlayerCharacter = Cast<AFrontierBaseCharacter>(GetPawn());
	if (!IsValid(DroppedItem)
		|| TargetSlotIndex < 0
		|| !PlayerCharacter
		|| PlayerCharacter->IsDead()
		|| IsInteractionBlockedByExtractionZone()
		|| PendingInteractionActor)
	{
		FRONTIER_LOG(VeryVerbose, TEXT("Dropped-item slot pickup rejected before validation. Controller=%s Item=%s Slot=%d"),
			*GetNameSafe(this), *GetNameSafe(DroppedItem), TargetSlotIndex);
		return;
	}

	if (!DroppedItem->TryPickupToRaidInventorySlot(this, TargetSlotIndex))
	{
		FRONTIER_LOG(VeryVerbose, TEXT("Dropped-item slot pickup failed validation. Controller=%s Item=%s Slot=%d"),
			*GetNameSafe(this), *GetNameSafe(DroppedItem), TargetSlotIndex);
	}
}

void AFrontierPlayerController::ServerLootContainerItem_Implementation(AFrontierLootContainerActor* LootContainer, const int32 SourceSlotIndex)
{
	if (InventoryCommandService && InventoryCommandService->LootContainerItem(LootContainer, SourceSlotIndex))
	{
		if (UFrontierRaidExperienceSubsystem* RaidExperience = GetWorld()->GetSubsystem<UFrontierRaidExperienceSubsystem>())
		{
			RaidExperience->RecordChestSearchCompleted(GetPlayerState<AFrontierPlayerState>(), LootContainer);
		}
	}
}

void AFrontierPlayerController::ServerLootContainerItemToRaidSlot_Implementation(AFrontierLootContainerActor* LootContainer, const int32 SourceLootSlotIndex, const int32 TargetRaidSlotIndex)
{
	if (InventoryCommandService && InventoryCommandService->LootContainerItemToRaidSlot(LootContainer, SourceLootSlotIndex, TargetRaidSlotIndex))
	{
		if (UFrontierRaidExperienceSubsystem* RaidExperience = GetWorld()->GetSubsystem<UFrontierRaidExperienceSubsystem>())
		{
			RaidExperience->RecordChestSearchCompleted(GetPlayerState<AFrontierPlayerState>(), LootContainer);
		}
	}
}

void AFrontierPlayerController::ServerLootContainerLoadoutItem_Implementation(AFrontierLootContainerActor* LootContainer, const EFrontierEquipmentSlot SlotType)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->LootContainerLoadoutItem(LootContainer, SlotType);
	}
}

void AFrontierPlayerController::ServerStoreRaidItemInLootContainer_Implementation(AFrontierLootContainerActor* LootContainer, const int32 SourceRaidSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->StoreRaidItemInLootContainer(LootContainer, SourceRaidSlotIndex);
	}
}

void AFrontierPlayerController::ServerStoreRaidItemInLootContainerSlot_Implementation(AFrontierLootContainerActor* LootContainer, const int32 SourceRaidSlotIndex, const int32 TargetLootSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->StoreRaidItemInLootContainerSlot(LootContainer, SourceRaidSlotIndex, TargetLootSlotIndex);
	}
}

void AFrontierPlayerController::ServerSwapLootContainerSlots_Implementation(AFrontierLootContainerActor* LootContainer, const int32 SourceLootSlotIndex, const int32 TargetLootSlotIndex)
{
	if (InventoryCommandService)
	{
		InventoryCommandService->SwapLootContainerSlots(LootContainer, SourceLootSlotIndex, TargetLootSlotIndex);
	}
}

void AFrontierPlayerController::ClientOpenLootContainer_Implementation(AFrontierLootContainerActor* LootContainer, const EFrontierLootContainerSourceType SourceType)
{
	if (!IsLocalController() || !LootContainer)
	{
		return;
	}

	OpenLootUI(LootContainer, SourceType);
}

void AFrontierPlayerController::ClientOpenTutorialPoster_Implementation(AFrontierTutorialPosterActor* TutorialPoster)
{
	if (!IsLocalController() || !TutorialPoster)
	{
		return;
	}

	if (TutorialPosterWidget)
	{
		TutorialPosterWidget->RemoveFromParent();
		TutorialPosterWidget = nullptr;
	}

	TSubclassOf<UFrontierTutorialPosterWidget> WidgetClass = TutorialPoster->GetTutorialPosterWidgetClass();
	if (!WidgetClass)
	{
		WidgetClass = UFrontierTutorialPosterWidget::StaticClass();
		FRONTIER_LOG(Warning, TEXT("Tutorial poster widget class is not assigned. Falling back to native widget class."));
	}

	TutorialPosterWidget = CreateWidget<UFrontierTutorialPosterWidget>(this, WidgetClass);
	if (!TutorialPosterWidget)
	{
		return;
	}

	TutorialPosterWidget->SetPosterMaterial(TutorialPoster->GetPosterMaterial());
	TutorialPosterWidget->AddToViewport(60);
	TutorialPosterWidget->SetKeyboardFocus();

	FRONTIER_LOG(Log, TEXT("Opened tutorial poster widget. Controller=%s Poster=%s Material=%s"),
		*GetNameSafe(this),
		*GetNameSafe(TutorialPoster),
		*GetNameSafe(TutorialPoster->GetPosterMaterial()));
}

UFrontierInGameInventoryWrapperWidget* AFrontierPlayerController::EnsureInventoryWrapperWidget()
{
	if (InventoryWrapperWidget)
	{
		return InventoryWrapperWidget;
	}

	TSubclassOf<UFrontierInGameInventoryWrapperWidget> WidgetClass = InventoryWrapperWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UFrontierInGameInventoryWrapperWidget::StaticClass();
		FRONTIER_LOG(Warning, TEXT("Inventory wrapper widget class is not assigned. Falling back to the native wrapper widget class."));
	}

	InventoryWrapperWidget = CreateWidget<UFrontierInGameInventoryWrapperWidget>(this, WidgetClass);
	if (InventoryWrapperWidget
		&& !InventoryWrapperWidget->HasPanelHost()
		&& !InventoryWrapperWidget->GetFixedInventoryWidget())
	{
		FRONTIER_LOG(Warning, TEXT("Inventory wrapper widget has no fixed inventory widget or panel host. Falling back to direct inventory widget mode."));
		InventoryWrapperWidget = nullptr;
	}

	return InventoryWrapperWidget;
}

UFrontierInventoryWidget* AFrontierPlayerController::EnsureInventoryWidget()
{
	if (InventoryWrapperWidget && InventoryWrapperWidget->GetFixedInventoryWidget())
	{
		InventoryWidget = InventoryWrapperWidget->GetFixedInventoryWidget();
		return InventoryWidget;
	}

	if (InventoryWidget)
	{
		return InventoryWidget;
	}

	TSubclassOf<UFrontierInventoryWidget> WidgetClass = InventoryWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UFrontierInventoryWidget::StaticClass();
		FRONTIER_LOG(Warning, TEXT("Inventory widget class is not assigned. Falling back to the native inventory widget class."));
	}

	InventoryWidget = CreateWidget<UFrontierInventoryWidget>(this, WidgetClass);
	return InventoryWidget;
}

UFrontierDropLootWidget* AFrontierPlayerController::EnsureDropLootWidget()
{
	if (DropLootWidget)
	{
		return DropLootWidget;
	}

	if (!DropLootWidgetClass)
	{
		FRONTIER_LOG(Error, TEXT("DropLootWidgetClass is not configured. Assign a Widget Blueprint on the PlayerController defaults. Controller=%s"), *GetNameSafe(this));
		return nullptr;
	}

	DropLootWidget = CreateWidget<UFrontierDropLootWidget>(this, DropLootWidgetClass);
	if (!DropLootWidget)
	{
		FRONTIER_LOG(Warning, TEXT("Failed to create the nearby dropped-loot widget. Controller=%s"), *GetNameSafe(this));
	}
	return DropLootWidget;
}

void AFrontierPlayerController::StartNearbyDroppedLootTracking(UFrontierInGameInventoryWrapperWidget* WrapperWidget)
{
	StopNearbyDroppedLootTracking();
	if (!WrapperWidget || !WrapperWidget->HasPanelHost() || !CanTrackNearbyDroppedLoot())
	{
		return;
	}

	UFrontierDropLootWidget* NearbyLootWidget = EnsureDropLootWidget();
	if (!NearbyLootWidget || !WrapperWidget->AddPanelWidget(NearbyLootWidget))
	{
		return;
	}
	NearbyLootWidget->SetOwningInventoryWidget(WrapperWidget->GetFixedInventoryWidget());

	RefreshNearbyDroppedLoot();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			NearbyDroppedLootRefreshTimerHandle,
			this,
			&AFrontierPlayerController::RefreshNearbyDroppedLoot,
			FMath::Max(0.05f, NearbyDroppedLootRefreshInterval),
			true);
	}

	FRONTIER_LOG(VeryVerbose, TEXT("Started nearby dropped-loot tracking. Controller=%s Radius=%.1f Interval=%.2f"),
		*GetNameSafe(this), NearbyDroppedLootQueryRadius, NearbyDroppedLootRefreshInterval);
}

void AFrontierPlayerController::StopNearbyDroppedLootTracking()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NearbyDroppedLootRefreshTimerHandle);
	}

	if (DropLootWidget)
	{
		DropLootWidget->ClearObservedDroppedItems();
	}
}

void AFrontierPlayerController::RefreshNearbyDroppedLoot()
{
	if (!CanTrackNearbyDroppedLoot())
	{
		StopNearbyDroppedLootTracking();
		return;
	}

	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	if (!World || !ControlledPawn || !DropLootWidget)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel5); // LootOrb
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FrontierNearbyDroppedLoot), false, ControlledPawn);

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		ControlledPawn->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(FMath::Max(1.0f, NearbyDroppedLootQueryRadius)),
		QueryParams);

	TSet<TWeakObjectPtr<AFrontierDroppedItemActor>> UniqueDroppedItems;
	TArray<AFrontierDroppedItemActor*> NearbyDroppedItems;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AFrontierDroppedItemActor* DroppedItem = Cast<AFrontierDroppedItemActor>(OverlapResult.GetActor());
		const TWeakObjectPtr<AFrontierDroppedItemActor> DroppedItemKey(DroppedItem);
		if (!IsValid(DroppedItem)
			|| UniqueDroppedItems.Contains(DroppedItemKey)
			|| !DroppedItem->GetItemInstance().IsValid()
			|| !DroppedItem->CanInteract(this))
		{
			continue;
		}

		UniqueDroppedItems.Add(DroppedItemKey);
		NearbyDroppedItems.Add(DroppedItem);
	}

	DropLootWidget->SetObservedDroppedItems(NearbyDroppedItems);
}

bool AFrontierPlayerController::CanTrackNearbyDroppedLoot() const
{
	if (!IsLocalController()
		|| IsInLobbyLevel()
		|| bRaidLoadingScreenActive
		|| bIsLootUIOpened
		|| !InventoryWrapperWidget
		|| !InventoryWrapperWidget->IsInViewport())
	{
		return false;
	}

	const AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>();
	if (FrontierPlayerState && FrontierPlayerState->IsOnlyASpectator())
	{
		return false;
	}

	const AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(GetPawn());
	return GetPawn() && (!PlayerCharacter || !PlayerCharacter->IsDead());
}

void AFrontierPlayerController::OpenLootUI(AFrontierLootContainerActor* LootContainer, const EFrontierLootContainerSourceType SourceType)
{
	if (!IsLocalController() || !LootContainer)
	{
		return;
	}

	StopNearbyDroppedLootTracking();

	if (bIsLootUIOpened && CurrentOpenedLootContainer == LootContainer)
	{
		CloseLootUI();
		return;
	}

	UFrontierInGameInventoryWrapperWidget* WrapperWidget = EnsureInventoryWrapperWidget();
	UFrontierInventoryWidget* PlayerInventoryWidget = EnsureInventoryWidget();
	if (!PlayerInventoryWidget)
	{
		return;
	}

	if (bIsLootUIOpened)
	{
		CloseLootUI();
	}

	PlayerInventoryWidget->SetObservedPlayerState(GetPlayerState<AFrontierPlayerState>());
	PlayerInventoryWidget->SetInventoryWidgetMode(EInventoryWidgetMode::PlayerInventory);

	if (WrapperWidget)
	{
		WrapperWidget->ClearPanelWidgets();
		WrapperWidget->SetObservedPlayerState(GetPlayerState<AFrontierPlayerState>());
		if (UFrontierInventoryWidget* FixedInventoryWidget = WrapperWidget->GetFixedInventoryWidget())
		{
			InventoryWidget = FixedInventoryWidget;
			FixedInventoryWidget->SetObservedPlayerState(GetPlayerState<AFrontierPlayerState>());
			FixedInventoryWidget->SetInventoryWidgetMode(EInventoryWidgetMode::PlayerInventory);
		}
	}

	if (WrapperWidget && !WrapperWidget->IsInViewport())
	{
		WrapperWidget->AddToViewport(10);
		WrapperWidget->SetVisibility(ESlateVisibility::Visible);
	}
	else if (!WrapperWidget && !PlayerInventoryWidget->IsInViewport())
	{
		PlayerInventoryWidget->AddToViewport(10);
		PlayerInventoryWidget->SetVisibility(ESlateVisibility::Visible);
	}

	if (SourceType == EFrontierLootContainerSourceType::PlayerDeath)
	{
		UFrontierInventoryWidget* DeadPlayerLootWidget = CreateWidget<UFrontierInventoryWidget>(this, InventoryWidgetClass);
		if (DeadPlayerLootWidget)
		{
			DeadPlayerLootWidget->SetInventoryWidgetMode(EInventoryWidgetMode::DeadPlayerLootView);
			DeadPlayerLootWidget->SetObservedLootContainer(LootContainer);
			CurrentLootSideWidget = DeadPlayerLootWidget;
			if (WrapperWidget)
			{
				WrapperWidget->AddPanelWidget(DeadPlayerLootWidget);
			}
			else
			{
				DeadPlayerLootWidget->AddToViewport(11);
			}

			DeadPlayerLootWidget->RefreshFromPlayerState();
			DeadPlayerLootWidget->ForceLayoutPrepass();
		}
	}
	else
	{
		TSubclassOf<UFrontierLootWidget> WidgetClass = LootWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UFrontierLootWidget::StaticClass();
		}

		LootWidget = CreateWidget<UFrontierLootWidget>(this, WidgetClass);
		if (LootWidget)
		{
			LootWidget->SetOwningInventoryWidget(PlayerInventoryWidget);
			LootWidget->SetObservedPlayerState(GetPlayerState<AFrontierPlayerState>());
			LootWidget->SetObservedLootContainer(LootContainer);
			CurrentLootSideWidget = LootWidget;
			if (WrapperWidget)
			{
				WrapperWidget->AddPanelWidget(LootWidget);
			}
			else
			{
				LootWidget->AddToViewport(11);
			}

			LootWidget->ForceLayoutPrepass();
		}
	}

	PlayerInventoryWidget->RefreshFromPlayerState();
	PlayerInventoryWidget->ForceLayoutPrepass();
	if (WrapperWidget)
	{
		WrapperWidget->ForceLayoutPrepass();
	}

	CurrentOpenedLootContainer = LootContainer;
	bIsLootUIOpened = true;
	RefreshModalInputState();
	PlayLocalUIOpenSound(LoadedLootContainerOpenSound);
}

void AFrontierPlayerController::CloseLootUI()
{
	

	if (InventoryWidget)
	{
		InventoryWidget->HideItemTooltip(nullptr);
	}

	if (InventoryWrapperWidget)
	{
		InventoryWrapperWidget->ClearPanelWidgets();
	}

	if (CurrentLootSideWidget)
	{
		CurrentLootSideWidget->RemoveFromParent();
		CurrentLootSideWidget = nullptr;
	}

	if (CurrentLobbySideWidget)
	{
		CurrentLobbySideWidget->RemoveFromParent();
		CurrentLobbySideWidget = nullptr;
	}

	LootWidget = nullptr;
	CurrentOpenedLootContainer = nullptr;
	bIsLootUIOpened = false;

	if (InventoryWrapperWidget && InventoryWrapperWidget->IsInViewport())
	{
		if (InventoryWidget)
		{
			InventoryWidget->HideItemTooltip(nullptr);
		}
		InventoryWrapperWidget->RemoveFromParent();
	}
	else if (InventoryWidget && InventoryWidget->IsInViewport())
	{
		InventoryWidget->HideItemTooltip(nullptr);
		InventoryWidget->RemoveFromParent();
	}

	if (LootWidget && LootWidget->IsInViewport())
	{
		LootWidget->RemoveFromParent();
	}

	RefreshModalInputState();
}

void AFrontierPlayerController::CloseInventoryWidgetLocal()
{
	

	if (bIsLootUIOpened)
	{
		CloseLootUI();
		return;
	}

	StopNearbyDroppedLootTracking();

	if (InventoryWidget)
	{
		InventoryWidget->HideItemTooltip(nullptr);
	}

	if (InventoryWrapperWidget)
	{
		InventoryWrapperWidget->ClearPanelWidgets();
		InventoryWrapperWidget->RemoveFromParent();
	}
	else if (InventoryWidget)
	{
		InventoryWidget->RemoveFromParent();
	}

	if (CurrentLobbySideWidget)
	{
		CurrentLobbySideWidget->RemoveFromParent();
		CurrentLobbySideWidget = nullptr;
	}

	RefreshModalInputState();
}

void AFrontierPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}
			
		}
	}

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// Numeric keys 1/2/3 are reserved for the three quick slots.
	}

	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AFrontierPlayerController::Input_UseQuickSlot1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AFrontierPlayerController::Input_UseQuickSlot2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AFrontierPlayerController::Input_UseQuickSlot3);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AFrontierPlayerController::ToggleSettingsWidget);
}

void AFrontierPlayerController::ToggleSettingsWidget()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!SettingsWidget)
	{
		TSubclassOf<UFrontierSettingsWidget> WidgetClass = SettingsWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UFrontierSettingsWidget::StaticClass();
		}

		SettingsWidget = CreateWidget<UFrontierSettingsWidget>(this, WidgetClass);
		if (!SettingsWidget)
		{
			return;
		}

		SettingsWidget->SetOwningPlayer(this);
	}

	if (SettingsWidget->IsInViewport() && SettingsWidget->IsVisible())
	{
		SettingsWidget->CloseSettings();
		return;
	}

	if (!SettingsWidget->IsInViewport())
	{
		SettingsWidget->AddToViewport(100);
	}

	SettingsWidget->OpenSettings();
	RefreshModalInputState();
}

void AFrontierPlayerController::HandleSettingsWidgetClosed()
{
	if (IsLocalController())
	{
		RefreshModalInputState();
	}
}

void AFrontierPlayerController::RefreshSkillBarWidget()
{
	if (PlayerStatusWidget)
	{
		PlayerStatusWidget->RefreshSkillBarWidget();
	}
}

void AFrontierPlayerController::CloseLootWidgetLocal()
{
	CloseLootUI();
}

void AFrontierPlayerController::RefreshModalInputState()
{
	if (!IsLocalController())
	{
		return;
	}

	UUserWidget* FocusWidget = nullptr;
	bool bUseUIOnlyInputMode = false;
	if (bRaidLoadingScreenActive)
	{
		bUseUIOnlyInputMode = true;
	}
	else if (bSpectatorInputModeApplied)
	{
		FocusWidget = SpectatorStatusWidget;
		bUseUIOnlyInputMode = true;
	}
	else if (InventoryWrapperWidget && InventoryWrapperWidget->IsInViewport())
	{
		FocusWidget = InventoryWrapperWidget;
	}
	else if (InventoryWidget && InventoryWidget->IsInViewport())
	{
		FocusWidget = InventoryWidget;
	}
	else if (SettingsWidget && SettingsWidget->IsInViewport() && SettingsWidget->IsVisible())
	{
		FocusWidget = SettingsWidget;
	}

	const bool bBlocked = bRaidLoadingScreenActive
		|| bSpectatorInputModeApplied
		|| FocusWidget != nullptr;
	if (bBlocked && PlayerStatusWidget)
	{
		PlayerStatusWidget->HideInteractionPrompt();
	}

	bShowMouseCursor = bBlocked;
	ApplyModalGameplayInputBlock(bBlocked);

	if (bUseUIOnlyInputMode)
	{
		FInputModeUIOnly InputMode;
		UWidget* PreferredFocus = FocusWidget;
		if (FocusWidget == SpectatorStatusWidget && SpectatorStatusWidget)
		{
			PreferredFocus = SpectatorStatusWidget->GetPreferredFocusTarget();
		}

		if (PreferredFocus)
		{
			InputMode.SetWidgetToFocus(PreferredFocus->TakeWidget());
		}
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		return;
	}

	if (FocusWidget)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(FocusWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		return;
	}

	SetInputMode(FInputModeGameOnly());
}

void AFrontierPlayerController::ApplyModalGameplayInputBlock(const bool bShouldBlock)
{
	if (bModalGameplayInputBlocked == bShouldBlock)
	{
		return;
	}

	bModalGameplayInputBlocked = bShouldBlock;
	SetIgnoreMoveInput(bShouldBlock);
	SetIgnoreLookInput(bShouldBlock);
}

void AFrontierPlayerController::UpdateFocusedInteractable()
{
	if (IsModalUIBlockingGameplay())
	{
		if (FocusedInteractableActor)
		{
			ApplyInteractionOutline(nullptr);
			FocusedInteractableActor = nullptr;
		}
		if (PlayerStatusWidget)
		{
			PlayerStatusWidget->HideInteractionPrompt();
			PlayerStatusWidget->SetExtractionProgress(0.0f);
		}
		return;
	}

	const AFrontierExtractionZoneActor* ContainingExtractionZone = FindContainingExtractionZone();
	if (ContainingExtractionZone)
	{
		if (FocusedInteractableActor)
		{
			ApplyInteractionOutline(nullptr);
			FocusedInteractableActor = nullptr;
		}
		if (PlayerStatusWidget)
		{
			PlayerStatusWidget->HideInteractionPrompt();
			PlayerStatusWidget->SetExtractionProgress(
				ContainingExtractionZone->GetLocalProgressForCharacter(
					Cast<AFrontierPlayerCharacter>(GetPawn())));
		}
		return;
	}

	AActor* NewFocusedInteractable = nullptr;
	const APawn* ControlledPawn = GetPawn();
	if (ControlledPawn && GetWorld())
	{
		const FVector PawnLocation = ControlledPawn->GetActorLocation();
		float ClosestDistanceSquared = FMath::Square(FocusedInteractableRange);

		for (TActorIterator<AFrontierInteractableActor> Iterator(GetWorld()); Iterator; ++Iterator)
		{
			AFrontierInteractableActor* Candidate = *Iterator;
			if (!Candidate || Candidate->IsA<AFrontierExtractionZoneActor>())
			{
				continue;
			}

			IFrontierInteractInterface* Interactable = Cast<IFrontierInteractInterface>(Candidate);
			if (!Interactable || !Interactable->CanInteract(this))
			{
				continue;
			}

			const FVector InteractionLocation = Interactable->GetInteractionWorldLocation();
			const float DistanceSquared = FVector::DistSquared(PawnLocation, InteractionLocation.IsNearlyZero() ? Candidate->GetActorLocation() : InteractionLocation);
			if (DistanceSquared <= ClosestDistanceSquared)
			{
				ClosestDistanceSquared = DistanceSquared;
				NewFocusedInteractable = Candidate;
			}
		}
	}

	if (FocusedInteractableActor != NewFocusedInteractable)
	{
		ApplyInteractionOutline(NewFocusedInteractable);
		FocusedInteractableActor = NewFocusedInteractable;
	}

	if (PlayerStatusWidget)
	{
		if (!bLocalInteractionChannelActive)
		{
			if (IFrontierInteractInterface* Interactable = Cast<IFrontierInteractInterface>(FocusedInteractableActor))
			{
				PlayerStatusWidget->ShowInteractionPrompt(
					Interactable->GetInteractionDisplayName(this),
					Interactable->GetInteractionActionText(this));
			}
			else
			{
				PlayerStatusWidget->HideInteractionPrompt();
			}
		}
		else
		{
			PlayerStatusWidget->HideInteractionPrompt();
		}
		PlayerStatusWidget->SetExtractionProgress(0.0f);
	}
}

void AFrontierPlayerController::ApplyInteractionOutline(AActor* InFocusedActor)
{
	for (UPrimitiveComponent* PrimitiveComponent : FocusedInteractionOutlineComponents)
	{
		if (UMeshComponent* MeshComponent = Cast<UMeshComponent>(PrimitiveComponent))
		{
			MeshComponent->SetOverlayMaterial(nullptr);
		}
	}

	FocusedInteractionOutlineComponents.Reset();

	IFrontierInteractInterface* Interactable = Cast<IFrontierInteractInterface>(InFocusedActor);
	if (!Interactable)
	{
		return;
	}

	TArray<UPrimitiveComponent*> HighlightComponents;
	Interactable->GetInteractionHighlightComponents(HighlightComponents);
	for (UPrimitiveComponent* PrimitiveComponent : HighlightComponents)
	{
		UMeshComponent* MeshComponent = Cast<UMeshComponent>(PrimitiveComponent);
		if (!MeshComponent)
		{
			continue;
		}

		MeshComponent->SetOverlayMaterial(InteractionOverlayMaterial);
		FocusedInteractionOutlineComponents.Add(MeshComponent);
	}
}

void AFrontierPlayerController::UpdateFocusedDroppedItem()
{
	AFrontierDroppedItemActor* NewFocusedItem = Cast<AFrontierDroppedItemActor>(FocusedInteractableActor);

	if (FocusedDroppedItemActor == NewFocusedItem)
	{
		return;
	}

	FocusedDroppedItemActor = NewFocusedItem;
	if (FocusedItemWidget)
	{
		FocusedItemWidget->SetFocusedItemActor(FocusedDroppedItemActor);
	}
}

void AFrontierPlayerController::EnsureFocusedItemWidget()
{
	if (FocusedItemWidget)
	{
		return;
	}

	TSubclassOf<UFrontierFocusedItemWidget> WidgetClass = FocusedItemWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UFrontierFocusedItemWidget::StaticClass();
	}

	FocusedItemWidget = CreateWidget<UFrontierFocusedItemWidget>(this, WidgetClass);
	if (!FocusedItemWidget)
	{
		return;
	}

	FocusedItemWidget->AddToViewport(1);
	FocusedItemWidget->SetVisibility(ESlateVisibility::Collapsed);
	FocusedItemWidget->SetFocusedItemActor(nullptr);
}

bool AFrontierPlayerController::ShouldAcceptLookInput() const
{
	if (!IsLocalController())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const UGameViewportClient* GameViewportClient = World ? World->GetGameViewport() : nullptr;
	if (!GameViewportClient || !GameViewportClient->Viewport)
	{
		return true;
	}

	return GameViewportClient->Viewport->HasFocus();
}

void AFrontierPlayerController::EnsureSpectatorStatusWidget()
{
	if (!IsLocalController() || SpectatorStatusWidget)
	{
		return;
	}

	TSubclassOf<UFrontierSpectatorStatusWidget> WidgetClass = SpectatorStatusWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UFrontierSpectatorStatusWidget::StaticClass();
	}

	SpectatorStatusWidget = CreateWidget<UFrontierSpectatorStatusWidget>(this, WidgetClass);
	if (!SpectatorStatusWidget)
	{
		return;
	}

	SpectatorStatusWidget->SetOwningPlayer(this);
	SpectatorStatusWidget->AddToViewport(3);
	SpectatorStatusWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void AFrontierPlayerController::RefreshSpectatorStatusWidgetVisibility()
{
	if (!IsLocalController())
	{
		return;
	}

	AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>();
	const bool bShouldShowSpectatorWidget = FrontierPlayerState && FrontierPlayerState->IsOnlyASpectator() && !IsInLobbyLevel();

	if (SpectatorStatusWidget)
	{
		SpectatorStatusWidget->SetVisibility(bShouldShowSpectatorWidget ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bShouldShowSpectatorWidget)
		{
			SpectatorStatusWidget->RefreshSpectatorState();
		}
	}

	if (PlayerStatusWidget)
	{
		PlayerStatusWidget->SetVisibility(bShouldShowSpectatorWidget ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (bShouldShowSpectatorWidget && !bSpectatorInputModeApplied)
	{
		bSpectatorInputModeApplied = true;
		RefreshModalInputState();
		return;
	}

	if (!bShouldShowSpectatorWidget && bSpectatorInputModeApplied)
	{
		bSpectatorInputModeApplied = false;
		RefreshModalInputState();
	}
}

void AFrontierPlayerController::BuildAvailableSpectatorTargets(TArray<AActor*>& OutTargets) const
{
	OutTargets.Reset();

	const UWorld* World = GetWorld();
	const AFrontierPlayerState* LocalPlayerState = GetPlayerState<AFrontierPlayerState>();
	if (!World || !LocalPlayerState || !LocalPlayerState->IsOnlyASpectator())
	{
		return;
	}

	TArray<AActor*> AliveTeammates;
	for (TActorIterator<AFrontierPlayerCharacter> Iterator(World); Iterator; ++Iterator)
	{
		AFrontierPlayerCharacter* CandidateCharacter = *Iterator;
		const AFrontierPlayerState* CandidatePlayerState = CandidateCharacter ? CandidateCharacter->GetPlayerState<AFrontierPlayerState>() : nullptr;
		if (!CandidateCharacter || CandidateCharacter == GetPawn() || !CandidatePlayerState || CandidatePlayerState == LocalPlayerState || CandidatePlayerState->IsOnlyASpectator() || CandidateCharacter->IsDead())
		{
			continue;
		}

		if (LocalPlayerState->GetTeamId() > 0 && CandidatePlayerState->GetTeamId() == LocalPlayerState->GetTeamId())
		{
			AliveTeammates.Add(CandidateCharacter);
		}
	}

	OutTargets = MoveTemp(AliveTeammates);
}

void AFrontierPlayerController::CycleSpectatorTarget(const int32 Direction)
{
	if (!IsLocalController() || Direction == 0)
	{
		return;
	}

	TArray<AActor*> AvailableTargets;
	BuildAvailableSpectatorTargets(AvailableTargets);
	if (AvailableTargets.Num() <= 0)
	{
		return;
	}

	const AActor* CurrentTarget = GetViewTarget();
	int32 CurrentIndex = AvailableTargets.IndexOfByKey(CurrentTarget);
	if (CurrentIndex == INDEX_NONE)
	{
		CurrentIndex = 0;
	}
	else
	{
		CurrentIndex = (CurrentIndex + Direction + AvailableTargets.Num()) % AvailableTargets.Num();
	}

	SetViewTargetWithBlend(AvailableTargets[CurrentIndex], 0.2f);
}

void AFrontierPlayerController::RequestEndSpectating()
{
	if (!IsLocalController())
	{
		return;
	}

	const AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>();
	if (!FrontierPlayerState || !FrontierPlayerState->IsOnlyASpectator())
	{
		return;
	}

	ServerRequestEndSpectating();
}

void AFrontierPlayerController::ServerRequestEndSpectating_Implementation()
{
	if (AFrontierGameMode* FrontierGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<AFrontierGameMode>()
		: nullptr)
	{
		FrontierGameMode->HandleSpectatorEndRequested(this);
	}
}

void AFrontierPlayerController::HandleRaidSettlementReadyFromBackend(
	const FString& RaidSessionId,
	const EFrontierRaidOutcome Outcome,
	const int64 AwardedExperience)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AFrontierGameMode* FrontierGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<AFrontierGameMode>()
		: nullptr)
	{
		FrontierGameMode->HandleBackendSettlementReady(
			this,
			RaidSessionId,
			Outcome,
			AwardedExperience);
	}
}

bool AFrontierPlayerController::CanCycleSpectatorTargets() const
{
	TArray<AActor*> AvailableTargets;
	BuildAvailableSpectatorTargets(AvailableTargets);
	return AvailableTargets.Num() > 0;
}

FText AFrontierPlayerController::GetCurrentSpectatorTargetDisplayText() const
{
	const AFrontierPlayerState* LocalPlayerState = GetPlayerState<AFrontierPlayerState>();
	if (!LocalPlayerState || !LocalPlayerState->IsOnlyASpectator())
	{
		return FText::GetEmpty();
	}

	TArray<AActor*> AvailableTargets;
	BuildAvailableSpectatorTargets(AvailableTargets);
	if (AvailableTargets.Num() <= 0)
	{
		return FText::FromString(TEXT("Spectating : none"));
	}

	AActor* ViewTarget = GetViewTarget();
	if (!AvailableTargets.Contains(ViewTarget))
	{
		ViewTarget = AvailableTargets[0];
	}

	const APawn* TargetPawn = Cast<APawn>(ViewTarget);
	const AFrontierPlayerState* TargetPlayerState = TargetPawn ? TargetPawn->GetPlayerState<AFrontierPlayerState>() : nullptr;
	const FString TargetName = TargetPlayerState ? TargetPlayerState->GetPlayerName() : GetNameSafe(ViewTarget);
	return FText::FromString(FString::Printf(TEXT("Spectating : %s"), *TargetName));
}

void AFrontierPlayerController::ClientNotifyRaidReadyState_Implementation(const bool bNewRaidReady)
{
	if (AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>())
	{
		FrontierPlayerState->ApplyPredictedRaidReady(bNewRaidReady);
	}
}

void AFrontierPlayerController::ClientShowPlayerStatusWarning_Implementation(const FString& WarningMessage)
{
	ShowLocalPlayerStatusWarning(WarningMessage);
}

void AFrontierPlayerController::ClientAddNumberPop_Implementation(
	const FFrontierNumberPopRequest& NumberPopRequest)
{
	if (!IsLocalController())
	{
		FRONTIER_LOG(Warning, TEXT("[NumberPop] RPC reached a non-local controller. Controller=%s"), *GetNameSafe(this));
		return;
	}

	if (!NumberPopComponent)
	{
		FRONTIER_LOG(Warning, TEXT("[NumberPop] RPC received without a NumberPopComponent. Controller=%s"), *GetNameSafe(this));
		return;
	}

	FRONTIER_LOG(Log, TEXT("[NumberPop] RPC received. Controller=%s Damage=%d Location=%s"),
		*GetNameSafe(this),
		NumberPopRequest.NumberToDisplay,
		*NumberPopRequest.WorldLocation.ToString());
	NumberPopComponent->AddNumberPop(NumberPopRequest);
}

void AFrontierPlayerController::SetRaidLoadingScreenState(const bool bShowLoadingScreen)
{
	if (!IsLocalController())
	{
		return;
	}

	if (bShowLoadingScreen)
	{
		StopNearbyDroppedLootTracking();
		bClientGameplayReadySent = false;

		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UFrontierLoadingScreenSubsystem* LoadingScreen =
				GameInstance->GetSubsystem<UFrontierLoadingScreenSubsystem>())
			{
				LoadingScreen->RecreateRaidLoadingScreen();
			}
		}

		bRaidLoadingScreenActive = true;
		RefreshModalInputState();
		return;
	}

	bRaidLoadingScreenActive = false;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFrontierLoadingScreenSubsystem* LoadingScreen =
			GameInstance->GetSubsystem<UFrontierLoadingScreenSubsystem>())
		{
			LoadingScreen->StopRaidLoadingScreen();
		}
	}
	RefreshModalInputState();
}

void AFrontierPlayerController::ClientSetRaidLoadingScreen_Implementation(const bool bShowLoadingScreen)
{
	SetRaidLoadingScreenState(bShowLoadingScreen);
}

void AFrontierPlayerController::ClientReceiveRaidExperienceResult_Implementation(
	const FFrontierRaidExperienceResult& Result)
{
	LastRaidExperienceResult = Result;
	OnRaidExperienceResultReceived.Broadcast(LastRaidExperienceResult);
}

void AFrontierPlayerController::ClientReceiveRaidActivated_Implementation(const FString& RaidSessionId)
{
	if (UFrontierRaidSessionSubsystem* RaidSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr)
	{
		RaidSession->MarkClientRaidActive(RaidSessionId);
	}
}

void AFrontierPlayerController::ClientReceiveRaidSettlementState_Implementation(
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

void AFrontierPlayerController::ClientCompleteRaidSettlement_Implementation(
	const FString& RaidSessionId,
	const EFrontierRaidOutcome Outcome,
	const int64 AwardedExperience,
	const FString& LobbyDestination)
{
	if (UFrontierRaidSessionSubsystem* RaidSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr)
	{
		RaidSession->MarkClientSettlementCommitted(RaidSessionId, Outcome, AwardedExperience);
	}

	if (LobbyDestination.IsEmpty())
	{
		ClientReceiveRaidFlowFailed_Implementation(TEXT("로비 이동 주소가 설정되지 않았습니다."), false);
		return;
	}

	ClientTravel(LobbyDestination, TRAVEL_Absolute);
}

void AFrontierPlayerController::ClientReceiveRaidFlowFailed_Implementation(
	const FString& Error,
	const bool bRetryable)
{
	FRONTIER_LOG(
		Error,
		TEXT("Raid server flow failed. Controller=%s Retryable=%d Error=%s"),
		*GetNameSafe(this),
		bRetryable ? 1 : 0,
		Error.IsEmpty() ? TEXT("<empty>") : *Error);

	if (UFrontierRaidSessionSubsystem* RaidSession = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierRaidSessionSubsystem>()
		: nullptr)
	{
		RaidSession->FailClientFlow(Error, bRetryable);
	}
	ShowLocalPlayerStatusWarning(Error.IsEmpty() ? TEXT("레이드 정산에 실패했습니다.") : Error);
}

void AFrontierPlayerController::OpenLobbyStorageUILocal()
{
	if (!IsLocalController())
	{
		return;
	}

	StopNearbyDroppedLootTracking();

	UFrontierInGameInventoryWrapperWidget* WrapperWidget = EnsureInventoryWrapperWidget();
	UFrontierInventoryWidget* PlayerInventoryWidget = EnsureInventoryWidget();
	if (!PlayerInventoryWidget)
	{
		return;
	}

	if (WrapperWidget && WrapperWidget->IsInViewport() && CurrentLobbySideWidget)
	{
		CloseInventoryWidgetLocal();
		return;
	}

	if (WrapperWidget)
	{
		WrapperWidget->ClearPanelWidgets();
		WrapperWidget->SetObservedPlayerState(GetPlayerState<AFrontierPlayerState>());
		if (UFrontierInventoryWidget* FixedInventoryWidget = WrapperWidget->GetFixedInventoryWidget())
		{
			InventoryWidget = FixedInventoryWidget;
			FixedInventoryWidget->SetObservedPlayerState(GetPlayerState<AFrontierPlayerState>());
			FixedInventoryWidget->SetInventoryWidgetMode(EInventoryWidgetMode::PlayerInventory);
		}
	}
	else
	{
		PlayerInventoryWidget->SetObservedPlayerState(GetPlayerState<AFrontierPlayerState>());
		PlayerInventoryWidget->SetInventoryWidgetMode(EInventoryWidgetMode::PlayerInventory);
	}

	if (WrapperWidget && !WrapperWidget->IsInViewport())
	{
		WrapperWidget->AddToViewport(10);
		WrapperWidget->SetVisibility(ESlateVisibility::Visible);
	}
	else if (!WrapperWidget && !PlayerInventoryWidget->IsInViewport())
	{
		PlayerInventoryWidget->AddToViewport(10);
		PlayerInventoryWidget->SetVisibility(ESlateVisibility::Visible);
	}

	TSubclassOf<UFrontierInventoryWidget> LobbyWidgetClass = StorageWidgetClass ? StorageWidgetClass : InventoryWidgetClass;
	UFrontierInventoryWidget* StorageWidget = CreateWidget<UFrontierInventoryWidget>(this, LobbyWidgetClass);
	if (StorageWidget)
	{
		StorageWidget->SetObservedPlayerState(GetPlayerState<AFrontierPlayerState>());
		StorageWidget->SetInventoryWidgetMode(EInventoryWidgetMode::StorageView);
		CurrentLobbySideWidget = StorageWidget;

		if (WrapperWidget && WrapperWidget->HasPanelHost())
		{
			WrapperWidget->AddPanelWidget(StorageWidget);
		}
		else
		{
			StorageWidget->AddToViewport(11);
			StorageWidget->SetPositionInViewport(FVector2D(980.0f, 80.0f), false);
		}
	}

	if (PlayerInventoryWidget)
	{
		PlayerInventoryWidget->RefreshFromPlayerState();
		PlayerInventoryWidget->ForceLayoutPrepass();
	}

	if (StorageWidget)
	{
		StorageWidget->RefreshFromPlayerState();
		StorageWidget->ForceLayoutPrepass();
	}

	if (WrapperWidget)
	{
		WrapperWidget->ForceLayoutPrepass();
	}

	RefreshModalInputState();
	PlayLocalUIOpenSound(LoadedInventoryOpenSound);
}

void AFrontierPlayerController::CacheUIAudioSettings()
{
	LoadedInventoryOpenSound = nullptr;
	LoadedLootContainerOpenSound = nullptr;
	UIOpenSoundVolumeMultiplier = 1.0f;
	UIOpenSoundPitchMultiplier = 1.0f;

	if (!IsLocalController())
	{
		return;
	}

	const UFrontierUISettings* UISettings = GetDefault<UFrontierUISettings>();
	if (!UISettings)
	{
		return;
	}

	LoadedInventoryOpenSound = UISettings->InventoryOpenSound.LoadSynchronous();
	LoadedLootContainerOpenSound = UISettings->LootContainerOpenSound.LoadSynchronous();
	UIOpenSoundVolumeMultiplier = FMath::Max(0.0f, UISettings->UIOpenSoundVolumeMultiplier);
	UIOpenSoundPitchMultiplier = FMath::Max(0.01f, UISettings->UIOpenSoundPitchMultiplier);
}

void AFrontierPlayerController::PlayLocalUIOpenSound(USoundBase* Sound) const
{
	if (IsLocalController() && Sound)
	{
		UGameplayStatics::PlaySound2D(
			this,
			Sound,
			UIOpenSoundVolumeMultiplier,
			UIOpenSoundPitchMultiplier);
	}
}

void AFrontierPlayerController::Input_UseQuickSlot1()
{
	RequestActivateQuickSlot(0);
}

void AFrontierPlayerController::Input_UseQuickSlot2()
{
	RequestActivateQuickSlot(1);
}

void AFrontierPlayerController::Input_UseQuickSlot3()
{
	RequestActivateQuickSlot(2);
}

void AFrontierPlayerController::RequestActivateQuickSlot(const int32 QuickSlotIndex)
{
	if (QuickSlotIndex < 0 || QuickSlotIndex >= 3 || IsInventoryUIOpen() || IsLootUIOpen())
	{
		return;
	}

	if (HasAuthority())
	{
		ServerUseQuickSlot_Implementation(QuickSlotIndex);
	}
	else
	{
		ServerUseQuickSlot(QuickSlotIndex);
	}
}

void AFrontierPlayerController::RequestSetTeam(const int32 RequestedTeamId)
{
	FRONTIER_LOG(Log, TEXT("Requesting team change. Controller=%s TeamId=%d"), *GetNameSafe(this), RequestedTeamId);

	if (!IsInLobbyLevel())
	{
		FRONTIER_LOG(Warning, TEXT("Rejected team change because the controller is not in the lobby. Controller=%s"), *GetNameSafe(this));
		return;
	}

	if (HasAuthority())
	{
		ServerRequestSetTeam_Implementation(RequestedTeamId);
		return;
	}

	ServerRequestSetTeam(RequestedTeamId);
}

void AFrontierPlayerController::ServerRequestSetTeam_Implementation(const int32 RequestedTeamId)
{
	FRONTIER_LOG(Log, TEXT("Server handling team change request. Controller=%s TeamId=%d"), *GetNameSafe(this), RequestedTeamId);

	if (!IsInLobbyLevel())
	{
		FRONTIER_LOG(Warning, TEXT("Rejected team change on server because the controller is not in the lobby. Controller=%s"), *GetNameSafe(this));
		return;
	}

	if (RequestedTeamId != 1 && RequestedTeamId != 2 && RequestedTeamId != 3)
	{
		FRONTIER_LOG(Warning, TEXT("Rejected invalid team id %d."), RequestedTeamId);
		return;
	}

	if (AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>())
	{
		FrontierPlayerState->SetTeamId(RequestedTeamId);
	}
}

void AFrontierPlayerController::EnsureTeamStatusWidget()
{
	if (!IsLocalController() || TeamStatusWidget)
	{
		return;
	}

	TSubclassOf<UFrontierTeamStatusWidget> WidgetClass = TeamStatusWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UFrontierTeamStatusWidget::StaticClass();
	}

	TeamStatusWidget = CreateWidget<UFrontierTeamStatusWidget>(this, WidgetClass);
	if (!TeamStatusWidget)
	{
		return;
	}

	TeamStatusWidget->AddToViewport(2);
	TeamStatusWidget->SetPositionInViewport(FVector2D(24.0f, 90.0f), false);
	TeamStatusWidget->SetVisibility(ESlateVisibility::Visible);
	TeamStatusWidget->RefreshTeamMembers();
}

AFrontierLootContainerActor* AFrontierPlayerController::GetCurrentOpenedLootContainer() const
{
	return CurrentOpenedLootContainer;
}

bool AFrontierPlayerController::IsLobbyStorageUIOpen() const
{
	return CurrentLobbySideWidget != nullptr && InventoryWrapperWidget && InventoryWrapperWidget->IsInViewport();
}

bool AFrontierPlayerController::IsInLobbyLevel() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FString CurrentWorldPackageName = World->GetOutermost()->GetName();
	const FString LobbyLevelPackageName = ResolveLobbyLevelPackageName();

	if (!LobbyLevelPackageName.IsEmpty())
	{
		if (CurrentWorldPackageName.Equals(LobbyLevelPackageName, ESearchCase::CaseSensitive)
			|| CurrentWorldPackageName.EndsWith(LobbyLevelPackageName, ESearchCase::CaseSensitive))
		{
			return true;
		}
	}

	return CurrentWorldPackageName.Contains(TEXT("Lobby"), ESearchCase::IgnoreCase);
}

FString AFrontierPlayerController::ResolveLobbyLevelPackageName() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return FString();
	}

	if (const AFrontierGameMode* FrontierGameMode = World->GetAuthGameMode<AFrontierGameMode>())
	{
		return FrontierGameMode->GetLobbyLevelPackageName();
	}

	UClass* GameModeClass = World->GetWorldSettings() ? World->GetWorldSettings()->DefaultGameMode : nullptr;
	if (!GameModeClass)
	{
		return FString();
	}

	const AFrontierGameMode* FrontierGameModeCDO = Cast<AFrontierGameMode>(GameModeClass->GetDefaultObject());
	return FrontierGameModeCDO ? FrontierGameModeCDO->GetLobbyLevelPackageName() : FString();
}

void AFrontierPlayerController::ShowLocalPlayerStatusWarning(const FString& WarningMessage) const
{
	if (!IsLocalController() || !PlayerStatusWidget)
	{
		return;
	}

	PlayerStatusWidget->ShowTransientWarning(FText::FromString(WarningMessage));
}

bool AFrontierPlayerController::IsModalUIBlockingGameplay() const
{
	return bRaidLoadingScreenActive
		|| bSpectatorInputModeApplied
		|| (TutorialPosterWidget && TutorialPosterWidget->IsInViewport())
		|| bIsLootUIOpened
		|| (InventoryWrapperWidget && InventoryWrapperWidget->IsInViewport())
		|| (InventoryWidget && InventoryWidget->IsInViewport());
}
