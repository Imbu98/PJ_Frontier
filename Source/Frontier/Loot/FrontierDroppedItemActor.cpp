#include "Loot/FrontierDroppedItemActor.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierPlayerState.h"
#include "Components/FrontierRaidInventoryComponent.h"
#include "Engine/StaticMesh.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "Inventory/Items/FrontierWeaponItemDataAsset.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/FrontierWeaponDataAsset.h"

AFrontierDroppedItemActor::AFrontierDroppedItemActor()
{
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = false;

	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	SetRootComponent(StaticMeshComponent);
	if (SceneRoot)
	{
		SceneRoot->SetupAttachment(StaticMeshComponent);
	}
	if (MeshComponent)
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetVisibility(false, true);
		MeshComponent->SetHiddenInGame(true, true);
	}
	InteractionRange = 220.0f;
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	StaticMeshComponent->SetCollisionObjectType(ECC_WorldDynamic);
	StaticMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	StaticMeshComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	StaticMeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	StaticMeshComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	StaticMeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DefaultMesh.Succeeded())
	{
		StaticMeshComponent->SetStaticMesh(DefaultMesh.Object);
	}
	StaticMeshComponent->SetWorldScale3D(FVector(1.0f, 1.0f, 1.0f));
	StaticMeshComponent->SetSimulatePhysics(true);
	StaticMeshComponent->SetEnableGravity(true);

	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent"));
	SkeletalMeshComponent->SetupAttachment(StaticMeshComponent);
	SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalMeshComponent->SetVisibility(false, true);
	SkeletalMeshComponent->SetHiddenInGame(true, true);
	SkeletalMeshComponent->SetWorldScale3D(FVector(1.0f, 1.0f, 1.0f));

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(StaticMeshComponent);
	InteractionSphere->SetSphereRadius(50.0f);
	InteractionSphere->SetCollisionProfileName(TEXT("LootOrb"));
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetGenerateOverlapEvents(false);
}

void AFrontierDroppedItemActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFrontierDroppedItemActor, ItemInstance);
}

void AFrontierDroppedItemActor::OnRep_ItemInstance()
{
	RefreshDroppedItemVisual();
}

bool AFrontierDroppedItemActor::CanInteract(const AFrontierPlayerController* InteractingController) const
{
	if (!InteractingController || !ItemInstance.IsValid())
	{
		return false;
	}

	const APawn* InteractingPawn = InteractingController->GetPawn();
	return InteractingPawn && FVector::DistSquared(InteractingPawn->GetActorLocation(), GetInteractionWorldLocation()) <= FMath::Square(InteractionRange);
}

bool AFrontierDroppedItemActor::IsInstantInteraction(const AFrontierPlayerController* InteractingController) const
{
	return true;
}

void AFrontierDroppedItemActor::Interacted(AFrontierPlayerController* InteractingController)
{
	if (!HasAuthority() || !CanInteract(InteractingController))
	{
		return;
	}

	AFrontierPlayerState* FrontierPlayerState = InteractingController ? InteractingController->GetPlayerState<AFrontierPlayerState>() : nullptr;
	UFrontierRaidInventoryComponent* RaidInventory = FrontierPlayerState ? FrontierPlayerState->GetRaidInventoryComponent() : nullptr;
	if (!RaidInventory)
	{
		return;
	}

	if (RaidInventory->AddItem(ItemInstance))
	{
		Destroy();
	}
}

bool AFrontierDroppedItemActor::TryPickupToRaidInventorySlot(
	AFrontierPlayerController* InteractingController,
	const int32 TargetSlotIndex)
{
	if (!HasAuthority() || !CanInteract(InteractingController))
	{
		return false;
	}

	AFrontierPlayerState* FrontierPlayerState = InteractingController
		? InteractingController->GetPlayerState<AFrontierPlayerState>()
		: nullptr;
	UFrontierRaidInventoryComponent* RaidInventory = FrontierPlayerState
		? FrontierPlayerState->GetRaidInventoryComponent()
		: nullptr;
	FFrontierInventorySlot TargetSlot;
	if (!RaidInventory
		|| !RaidInventory->GetSlot(TargetSlotIndex, TargetSlot)
		|| TargetSlot.bOccupied)
	{
		return false;
	}

	if (!RaidInventory->SetItemAtSlot(TargetSlotIndex, ItemInstance))
	{
		return false;
	}

	Destroy();
	return true;
}

FText AFrontierDroppedItemActor::GetInteractionDisplayName(const AFrontierPlayerController* InteractingController) const
{
	return ItemInstance.IsValid() ? ItemInstance.GetDisplayNameText() : FText::FromString(TEXT("Item"));
}

FText AFrontierDroppedItemActor::GetInteractionActionText(const AFrontierPlayerController* InteractingController) const
{
	return NSLOCTEXT("FrontierInteraction", "AcquireItemAction", "획득");
}

FText AFrontierDroppedItemActor::GetInteractionPromptText(const AFrontierPlayerController* InteractingController) const
{
	if (ItemInstance.IsValid())
	{
		return FText::FromString(FString::Printf(TEXT("Press F to loot %s"), *ItemInstance.GetDisplayNameText().ToString()));
	}

	return FText::FromString(TEXT("Press F to loot"));
}

void AFrontierDroppedItemActor::InitializeDroppedItem(const FFrontierItemInstance& InItemInstance)
{
	ItemInstance = InItemInstance;
	ItemInstance.EnsureRuntimeIdentity();
	RefreshDroppedItemVisual();
	ApplyDropPhysics();
}

const FFrontierItemInstance& AFrontierDroppedItemActor::GetItemInstance() const
{
	return ItemInstance;
}

FVector AFrontierDroppedItemActor::GetInteractionWorldLocation() const
{
	return StaticMeshComponent ? StaticMeshComponent->GetComponentLocation() : GetActorLocation();
}

void AFrontierDroppedItemActor::GetInteractionHighlightComponents(TArray<UPrimitiveComponent*>& OutComponents) const
{
	OutComponents.Reset();
	if (StaticMeshComponent)
	{
		OutComponents.Add(StaticMeshComponent);
	}
	if (SkeletalMeshComponent && SkeletalMeshComponent->GetSkeletalMeshAsset())
	{
		OutComponents.Add(SkeletalMeshComponent);
	}
}

void AFrontierDroppedItemActor::RefreshDroppedItemVisual()
{
	if (!StaticMeshComponent || !SkeletalMeshComponent)
	{
		return;
	}

	StaticMeshComponent->SetVisibility(true, true);
	StaticMeshComponent->SetHiddenInGame(false, true);
	StaticMeshComponent->SetWorldScale3D(ResolveDropWorldScale());
	SkeletalMeshComponent->SetVisibility(false, true);
	SkeletalMeshComponent->SetHiddenInGame(true, true);
	SkeletalMeshComponent->SetSkeletalMesh(nullptr);
	SkeletalMeshComponent->SetWorldScale3D(ResolveDropWorldScale());

	const TSoftObjectPtr<UStaticMesh> StaticMesh = !ItemInstance.ItemTemplateData.Common.DropStaticMesh.IsNull()
		? ItemInstance.ItemTemplateData.Common.DropStaticMesh
		: nullptr;
	if (!StaticMesh.IsNull())
	{
		if (UStaticMesh* ResolvedStaticMesh = StaticMesh.LoadSynchronous())
		{
			StaticMeshComponent->SetStaticMesh(ResolvedStaticMesh);
			return;
		}
	}
}

void AFrontierDroppedItemActor::ApplyDropPhysics()
{
	if (!StaticMeshComponent)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(PhysicsSettleTimerHandle);
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	StaticMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	StaticMeshComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	StaticMeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	StaticMeshComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	StaticMeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	StaticMeshComponent->SetEnableGravity(true);
	StaticMeshComponent->SetSimulatePhysics(true);
	StaticMeshComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
	StaticMeshComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

	const FVector Forward = GetActorForwardVector().GetSafeNormal();
	const FVector Impulse = (Forward * ResolveDropImpulseForward()) + FVector(0.0f, 0.0f, ResolveDropImpulseUpward());
	StaticMeshComponent->AddImpulse(Impulse, NAME_None, true);

	if (PhysicsSettleDelay <= 0.0f)
	{
		FinalizeDropPhysics();
		return;
	}

	GetWorldTimerManager().SetTimer(
		PhysicsSettleTimerHandle,
		this,
		&AFrontierDroppedItemActor::FinalizeDropPhysics,
		PhysicsSettleDelay,
		false);
}

void AFrontierDroppedItemActor::FinalizeDropPhysics()
{
	if (!StaticMeshComponent)
	{
		return;
	}

	StaticMeshComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
	StaticMeshComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	StaticMeshComponent->SetSimulatePhysics(false);
	StaticMeshComponent->SetEnableGravity(false);
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FVector AFrontierDroppedItemActor::ResolveDropWorldScale() const
{
	return ItemInstance.IsValid() ? ItemInstance.ItemTemplateData.Common.DropWorldScale : FVector(1.0f, 1.0f, 1.0f);
}

float AFrontierDroppedItemActor::ResolveDropImpulseForward() const
{
	return ItemInstance.IsValid() ? ItemInstance.ItemTemplateData.Common.DropImpulseForward : 180.0f;
}

float AFrontierDroppedItemActor::ResolveDropImpulseUpward() const
{
	return ItemInstance.IsValid() ? ItemInstance.ItemTemplateData.Common.DropImpulseUpward : 120.0f;
}
