#include "Loot/FrontierDeathLootContainerActor.h"

#include "Components/FrontierLootInventoryComponent.h"
#include "Components/SceneComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

AFrontierDeathLootContainerActor::AFrontierDeathLootContainerActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	FloatingVisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FloatingVisualRoot"));
	FloatingVisualRoot->SetupAttachment(Root);

	if (MeshComponent)
	{
		MeshComponent->SetupAttachment(FloatingVisualRoot);
	}

	RarityEffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("RarityEffectComponent"));
	RarityEffectComponent->SetupAttachment(FloatingVisualRoot);
	RarityEffectComponent->SetAutoActivate(false);
	RarityEffectComponent->SetVisibility(false, true);
}

void AFrontierDeathLootContainerActor::BeginPlay()
{
	Super::BeginPlay();

	InitialFloatingRootLocation = FloatingVisualRoot
		? FloatingVisualRoot->GetRelativeLocation()
		: FVector::ZeroVector;
	SetActorTickEnabled(
		GetNetMode() != NM_DedicatedServer
		&& FloatingVisualRoot
		&& FloatAmplitude > KINDA_SMALL_NUMBER
		&& FloatAngularSpeed > KINDA_SMALL_NUMBER);

	if (LootInventoryComponent)
	{
		LootInventoryComponent->OnInventoryChanged.AddUniqueDynamic(
			this,
			&AFrontierDeathLootContainerActor::HandleDeathLootInventoryChanged);
	}
	OnDeadPlayerLoadoutChanged.AddUniqueDynamic(
		this,
		&AFrontierDeathLootContainerActor::HandleDeathLootLoadoutChanged);

	RefreshRarityEffect();
}

void AFrontierDeathLootContainerActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!FloatingVisualRoot)
	{
		return;
	}

	FloatingElapsedSeconds += DeltaSeconds;
	FVector FloatingLocation = InitialFloatingRootLocation;
	FloatingLocation.Z += FMath::Sin(FloatingElapsedSeconds * FloatAngularSpeed) * FloatAmplitude;
	FloatingVisualRoot->SetRelativeLocation(FloatingLocation);
}

void AFrontierDeathLootContainerActor::HandleDeathLootInventoryChanged(
	const TArray<FFrontierInventorySlot>& UpdatedSlots)
{
	RefreshRarityEffect();
}

void AFrontierDeathLootContainerActor::HandleDeathLootLoadoutChanged(
	const TArray<FFrontierLoadoutSlot>& UpdatedSlots)
{
	RefreshRarityEffect();
}

void AFrontierDeathLootContainerActor::RefreshRarityEffect()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	switch (FindHighestLootRarity())
	{
	case EFrontierItemRarity::Legendary:
		SetRarityEffect(LegendaryLootEffect);
		break;
	case EFrontierItemRarity::Epic:
		SetRarityEffect(EpicLootEffect);
		break;
	default:
		SetRarityEffect(nullptr);
		break;
	}
}

EFrontierItemRarity AFrontierDeathLootContainerActor::FindHighestLootRarity() const
{
	EFrontierItemRarity HighestRarity = EFrontierItemRarity::Common;
	const auto ConsiderItem = [&HighestRarity](const FFrontierItemInstance& ItemInstance)
	{
		if (ItemInstance.IsValid()
			&& static_cast<uint8>(ItemInstance.GetDisplayRarity()) > static_cast<uint8>(HighestRarity))
		{
			HighestRarity = ItemInstance.GetDisplayRarity();
		}
	};

	if (LootInventoryComponent)
	{
		for (const FFrontierInventorySlot& Slot : LootInventoryComponent->GetSlots())
		{
			if (Slot.bOccupied)
			{
				ConsiderItem(Slot.ItemInstance);
			}
		}
	}

	for (const FFrontierLoadoutSlot& Slot : DeadPlayerLoadoutItems)
	{
		if (Slot.bOccupied)
		{
			ConsiderItem(Slot.ItemInstance);
		}
	}

	return HighestRarity;
}

void AFrontierDeathLootContainerActor::SetRarityEffect(UNiagaraSystem* DesiredEffect)
{
	if (!RarityEffectComponent)
	{
		return;
	}

	if (!DesiredEffect)
	{
		RarityEffectComponent->DeactivateImmediate();
		RarityEffectComponent->SetAsset(nullptr);
		RarityEffectComponent->SetVisibility(false, true);
		return;
	}

	if (RarityEffectComponent->GetAsset() != DesiredEffect)
	{
		RarityEffectComponent->DeactivateImmediate();
		RarityEffectComponent->SetAsset(DesiredEffect);
	}
	RarityEffectComponent->SetVisibility(true, true);
	if (!RarityEffectComponent->IsActive())
	{
		RarityEffectComponent->Activate(true);
	}
}
