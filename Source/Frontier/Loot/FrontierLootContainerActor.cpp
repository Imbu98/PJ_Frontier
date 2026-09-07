#include "Loot/FrontierLootContainerActor.h"

#include "Components/FrontierLootInventoryComponent.h"
#include "Components/FrontierLootComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Frontier.h"
#include "FrontierPlayerController.h"
#include "Components/FrontierRaidInventoryComponent.h"
#include "Game/FrontierPlayerState.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "Net/UnrealNetwork.h"

AFrontierLootContainerActor::AFrontierLootContainerActor()
{
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	if (SceneRoot)
	{
		SceneRoot->SetupAttachment(Root);
	}
	if (MeshComponent)
	{
		MeshComponent->SetupAttachment(Root);
		MeshComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	}

	LootInventoryComponent = CreateDefaultSubobject<UFrontierLootInventoryComponent>(TEXT("LootInventoryComponent"));
	LootComponent = CreateDefaultSubobject<UFrontierLootComponent>(TEXT("LootComponent"));
}

void AFrontierLootContainerActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFrontierLootContainerActor, SourceType);
	DOREPLIFETIME(AFrontierLootContainerActor, LootActorDisplayName);
	DOREPLIFETIME(AFrontierLootContainerActor, DeadPlayerLoadoutItems);
}

bool AFrontierLootContainerActor::CanInteract(const AFrontierPlayerController* InteractingController) const
{
	if (!InteractingController)
	{
		return false;
	}

	const APawn* InteractingPawn = InteractingController->GetPawn();
	return InteractingPawn && FVector::DistSquared(InteractingPawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(InteractionRange);
}

void AFrontierLootContainerActor::Interacted(AFrontierPlayerController* InteractingController)
{
	if (!HasAuthority() || !InteractingController)
	{
		return;
	}

	InteractingController->ClientOpenLootContainer(this, SourceType);
}

FText AFrontierLootContainerActor::GetInteractionDisplayName(const AFrontierPlayerController* InteractingController) const
{
	return GetLootActorDisplayName();
}

FText AFrontierLootContainerActor::GetInteractionActionText(const AFrontierPlayerController* InteractingController) const
{
	return NSLOCTEXT("FrontierInteraction", "SearchLootAction", "수색");
}

FText AFrontierLootContainerActor::GetInteractionPromptText(const AFrontierPlayerController* InteractingController) const
{
	switch (SourceType)
	{
	case EFrontierLootContainerSourceType::PlayerDeath:
		return FText::FromString(TEXT("Press F to search corpse"));
	case EFrontierLootContainerSourceType::EnemyDeath:
	case EFrontierLootContainerSourceType::BossDeath:
		return FText::FromString(TEXT("Press F to loot"));
	default:
		return FText::FromString(TEXT("Press F to search"));
	}
}

FVector AFrontierLootContainerActor::GetInteractionWorldLocation() const
{
	return MeshComponent ? MeshComponent->GetComponentLocation() : GetActorLocation();
}

void AFrontierLootContainerActor::GetInteractionHighlightComponents(TArray<UPrimitiveComponent*>& OutComponents) const
{
	OutComponents.Reset();
	if (MeshComponent)
	{
		OutComponents.Add(MeshComponent);
	}
}

UFrontierLootInventoryComponent* AFrontierLootContainerActor::GetLootInventoryComponent() const
{
	return LootInventoryComponent;
}

EFrontierLootContainerSourceType AFrontierLootContainerActor::GetSourceType() const
{
	return SourceType;
}

FText AFrontierLootContainerActor::GetLootActorDisplayName() const
{
	return LootActorDisplayName.IsEmpty() ? FText::FromString(TEXT("Loot")) : LootActorDisplayName;
}

const TArray<FFrontierInventorySlot>& AFrontierLootContainerActor::GetDeadPlayerInventory() const
{
	return DeadPlayerInventory;
}

const TArray<FFrontierLoadoutSlot>& AFrontierLootContainerActor::GetDeadPlayerLoadoutItems() const
{
	return DeadPlayerLoadoutItems;
}

void AFrontierLootContainerActor::InitializeFromLootSlots(
	const TArray<FFrontierInventorySlot>& InitialLootSlots,
	const EFrontierLootContainerSourceType InSourceType)
{
	if (!HasAuthority())
	{
		return;
	}

	bGenerateLootOnBeginPlay = false;
	bInitialLootGenerated = true;
	SourceType = InSourceType;

	if (LootInventoryComponent)
	{
		LootInventoryComponent->InitializeLootSlots(InitialLootSlots);
	}
}

void AFrontierLootContainerActor::InitializeDeadPlayerLootData(
	const TArray<FFrontierInventorySlot>& InDeadPlayerInventory,
	const TArray<FFrontierLoadoutSlot>& InDeadPlayerLoadoutItems)
{
	if (!HasAuthority())
	{
		return;
	}

	DeadPlayerInventory = InDeadPlayerInventory;
	for (int32 SlotIndex = 0; SlotIndex < DeadPlayerInventory.Num(); ++SlotIndex)
	{
		DeadPlayerInventory[SlotIndex].SlotIndex = SlotIndex;
	}

	DeadPlayerLoadoutItems = InDeadPlayerLoadoutItems;

	if (SourceType == EFrontierLootContainerSourceType::PlayerDeath && LootInventoryComponent)
	{
		LootInventoryComponent->InitializeLootSlots(DeadPlayerInventory);
	}

	BroadcastDeadPlayerInventoryChanged();
	BroadcastDeadPlayerLoadoutChanged();
}

bool AFrontierLootContainerActor::LootItemToPlayer(AFrontierPlayerController* InteractingController, const int32 LootSlotIndex)
{
	return LootItemToPlayerSlot(InteractingController, LootSlotIndex, INDEX_NONE);
}

bool AFrontierLootContainerActor::LootItemToPlayerSlot(AFrontierPlayerController* InteractingController, const int32 LootSlotIndex, const int32 TargetRaidSlotIndex)
{
	if (!HasAuthority() || !InteractingController || !LootInventoryComponent)
	{
		return false;
	}

	AFrontierPlayerState* FrontierPlayerState = InteractingController->GetPlayerState<AFrontierPlayerState>();
	if (!FrontierPlayerState || !CanInteract(InteractingController))
	{
		return false;
	}

	UFrontierRaidInventoryComponent* RaidInventory = FrontierPlayerState->GetRaidInventoryComponent();
	if (!RaidInventory)
	{
		return false;
	}

	if (TargetRaidSlotIndex != INDEX_NONE)
	{
		return LootInventoryComponent->MoveItemToSlot(LootSlotIndex, RaidInventory, TargetRaidSlotIndex);
	}

	return LootInventoryComponent->MoveItemTo(RaidInventory, LootSlotIndex);
}

bool AFrontierLootContainerActor::LootDeadPlayerLoadoutItemToPlayer(AFrontierPlayerController* InteractingController, const EFrontierEquipmentSlot SlotType)
{
	if (!HasAuthority() || !InteractingController || SourceType != EFrontierLootContainerSourceType::PlayerDeath)
	{
		return false;
	}

	AFrontierPlayerState* FrontierPlayerState = InteractingController->GetPlayerState<AFrontierPlayerState>();
	if (!FrontierPlayerState || !CanInteract(InteractingController))
	{
		return false;
	}

	UFrontierRaidInventoryComponent* RaidInventory = FrontierPlayerState->GetRaidInventoryComponent();
	if (!RaidInventory)
	{
		return false;
	}

	for (FFrontierLoadoutSlot& LoadoutSlot : DeadPlayerLoadoutItems)
	{
		if (LoadoutSlot.SlotType != SlotType || !LoadoutSlot.bOccupied || !LoadoutSlot.ItemInstance.IsValid())
		{
			continue;
		}

		if (!RaidInventory->AddItem(LoadoutSlot.ItemInstance))
		{
			return false;
		}

		LoadoutSlot.bOccupied = false;
		LoadoutSlot.ItemInstance = FFrontierItemInstance();
		BroadcastDeadPlayerLoadoutChanged();
		return true;
	}

	return false;
}

bool AFrontierLootContainerActor::StorePlayerItem(AFrontierPlayerController* InteractingController, const int32 RaidSlotIndex)
{
	return StorePlayerItemAtSlot(InteractingController, RaidSlotIndex, INDEX_NONE);
}

bool AFrontierLootContainerActor::StorePlayerItemAtSlot(AFrontierPlayerController* InteractingController, const int32 RaidSlotIndex, const int32 TargetLootSlotIndex)
{
	if (!HasAuthority() || !InteractingController || !LootInventoryComponent)
	{
		return false;
	}

	AFrontierPlayerState* FrontierPlayerState = InteractingController->GetPlayerState<AFrontierPlayerState>();
	if (!FrontierPlayerState || !CanInteract(InteractingController))
	{
		return false;
	}

	UFrontierRaidInventoryComponent* RaidInventory = FrontierPlayerState->GetRaidInventoryComponent();
	if (!RaidInventory)
	{
		return false;
	}

	if (TargetLootSlotIndex != INDEX_NONE)
	{
		return RaidInventory->MoveItemToSlot(RaidSlotIndex, LootInventoryComponent, TargetLootSlotIndex);
	}

	return RaidInventory->MoveItemTo(LootInventoryComponent, RaidSlotIndex);
}

bool AFrontierLootContainerActor::SwapLootSlots(const int32 SourceLootSlotIndex, const int32 TargetLootSlotIndex)
{
	return LootInventoryComponent && LootInventoryComponent->SwapSlots(SourceLootSlotIndex, TargetLootSlotIndex);
}

bool AFrontierLootContainerActor::SwapLootSlotsForPlayer(
	AFrontierPlayerController* InteractingController,
	const int32 SourceLootSlotIndex,
	const int32 TargetLootSlotIndex)
{
	if (!HasAuthority() || !InteractingController || !CanInteract(InteractingController))
	{
		return false;
	}

	return SwapLootSlots(SourceLootSlotIndex, TargetLootSlotIndex);
}

void AFrontierLootContainerActor::BeginPlay()
{
	Super::BeginPlay();
	if (LootComponent)
	{
		LootComponent->OnBackendLootPrepared().AddUObject(
			this,
			&AFrontierLootContainerActor::HandleBackendLootPrepared);
	}

	if (LootInventoryComponent)
	{
		LootInventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &AFrontierLootContainerActor::HandleLootInventoryChanged);
		if (SourceType == EFrontierLootContainerSourceType::PlayerDeath)
		{
			DeadPlayerInventory = LootInventoryComponent->GetSlots();
		}
	}

	if (HasAuthority() && bGenerateLootOnBeginPlay)
	{
		GenerateInitialLoot();
	}
}

void AFrontierLootContainerActor::OnRep_SourceType()
{
	if (SourceType == EFrontierLootContainerSourceType::PlayerDeath && LootInventoryComponent)
	{
		DeadPlayerInventory = LootInventoryComponent->GetSlots();
		BroadcastDeadPlayerInventoryChanged();
	}
}

void AFrontierLootContainerActor::OnRep_DeadPlayerLoadoutItems()
{
	BroadcastDeadPlayerLoadoutChanged();
}

void AFrontierLootContainerActor::BroadcastDeadPlayerInventoryChanged()
{
	OnDeadPlayerInventoryChanged.Broadcast(DeadPlayerInventory);
}

void AFrontierLootContainerActor::BroadcastDeadPlayerLoadoutChanged()
{
	OnDeadPlayerLoadoutChanged.Broadcast(DeadPlayerLoadoutItems);
}

void AFrontierLootContainerActor::HandleLootInventoryChanged(const TArray<FFrontierInventorySlot>& UpdatedSlots)
{
	if (SourceType != EFrontierLootContainerSourceType::PlayerDeath)
	{
		return;
	}

	DeadPlayerInventory = UpdatedSlots;
	BroadcastDeadPlayerInventoryChanged();
}

void AFrontierLootContainerActor::HandleBackendLootPrepared()
{
	if (HasAuthority() && bGenerateLootOnBeginPlay && !bInitialLootGenerated)
	{
		GenerateInitialLoot();
	}
}

void AFrontierLootContainerActor::GenerateInitialLoot()
{
	if (!HasAuthority() || !LootInventoryComponent || !LootComponent || bInitialLootGenerated)
	{
		return;
	}
	if (LootComponent->IsBackendLootManaged() && !LootComponent->IsBackendLootPrepared())
	{
		return;
	}

	const TArray<FFrontierInventorySlot> GeneratedLootSlots = LootComponent->GenerateLootSlots();
	LootInventoryComponent->InitializeLootSlots(GeneratedLootSlots);
	bInitialLootGenerated = true;
}

bool AFrontierLootContainerActor::HasAnyLoot() const
{
	if (!LootInventoryComponent)
	{
		return false;
	}

	for (const FFrontierInventorySlot& Slot : LootInventoryComponent->GetSlots())
	{
		if (Slot.bOccupied && Slot.ItemInstance.IsValid())
		{
			return true;
		}
	}

	for (const FFrontierLoadoutSlot& Slot : DeadPlayerLoadoutItems)
	{
		if (Slot.bOccupied && Slot.ItemInstance.IsValid())
		{
			return true;
		}
	}

	return false;
}
