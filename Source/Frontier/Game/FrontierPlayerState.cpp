#include "Game/FrontierPlayerState.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "AbilitySystem/FrontierAttributeSet.h"
#include "Character/FrontierBaseCharacter.h"
#include "Components/FrontierEquipmentSkillComponent.h"
#include "Components/FrontierSkillTreeComponent.h"
#include "Components/FrontierLoadoutComponent.h"
#include "Components/FrontierStorageComponent.h"
#include "Components/FrontierRaidInventoryComponent.h"
#include "Components/FrontierQuickSlotComponent.h"
#include "Frontier.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Inventory/Items/FrontierArmorItemDataAsset.h"
#include "Inventory/Items/FrontierConsumableItemDataAsset.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "Inventory/Items/FrontierWeaponItemDataAsset.h"
#include "Net/UnrealNetwork.h"

AFrontierPlayerState::AFrontierPlayerState()
{
	

	AbilitySystemComponent = CreateDefaultSubobject<UFrontierAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<UFrontierAttributeSet>(TEXT("AttributeSet"));
	StorageComponent = CreateDefaultSubobject<UFrontierStorageComponent>(TEXT("StorageComponent"));
	RaidInventoryComponent = CreateDefaultSubobject<UFrontierRaidInventoryComponent>(TEXT("RaidInventoryComponent"));
	LoadoutComponent = CreateDefaultSubobject<UFrontierLoadoutComponent>(TEXT("LoadoutComponent"));
	EquipmentSkillComponent = CreateDefaultSubobject<UFrontierEquipmentSkillComponent>(TEXT("EquipmentSkillComponent"));
	SkillTreeComponent = CreateDefaultSubobject<UFrontierSkillTreeComponent>(TEXT("SkillTreeComponent"));
	QuickSlotComponent = CreateDefaultSubobject<UFrontierQuickSlotComponent>(TEXT("QuickSlotComponent"));
	// Continuous GAS values use this cap; transactional state explicitly requests immediate updates.
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(10.0f);

}

void AFrontierPlayerState::SetPlayerName(const FString& S)
{
	const FString PreviousName = GetPlayerName();
	Super::SetPlayerName(S);

	if (!PreviousName.Equals(GetPlayerName(), ESearchCase::CaseSensitive))
	{
		OnPlayerNameChanged.Broadcast(this);
	}
}

void AFrontierPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	if (AFrontierPlayerState* FrontierPlayerState = Cast<AFrontierPlayerState>(PlayerState))
	{
		FrontierPlayerState->BackendPlayerId = BackendPlayerId;
		FrontierPlayerState->VerifiedSteamId = VerifiedSteamId;
	}
}

void AFrontierPlayerState::SetBackendIdentity(const FString& InBackendPlayerId, const FString& InSteamId)
{
	if (!HasAuthority())
	{
		return;
	}
	BackendPlayerId = InBackendPlayerId;
	VerifiedSteamId = InSteamId;
}

int64 AFrontierPlayerState::GetBackendUserId() const
{
	int64 BackendUserId = 0;
	LexTryParseString(BackendUserId, *BackendPlayerId);
	return BackendUserId;
}

UAbilitySystemComponent* AFrontierPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UFrontierAbilitySystemComponent* AFrontierPlayerState::GetFrontierAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

const UFrontierAttributeSet* AFrontierPlayerState::GetFrontierAttributeSet() const
{
	return AttributeSet;
}

UFrontierStorageComponent* AFrontierPlayerState::GetStorageComponent() const
{
	return StorageComponent;
}

UFrontierRaidInventoryComponent* AFrontierPlayerState::GetRaidInventoryComponent() const
{
	return RaidInventoryComponent;
}

UFrontierLoadoutComponent* AFrontierPlayerState::GetLoadoutComponent() const
{
	return LoadoutComponent;
}

UFrontierEquipmentSkillComponent* AFrontierPlayerState::GetEquipmentSkillComponent() const
{
	return EquipmentSkillComponent;
}

UFrontierSkillTreeComponent* AFrontierPlayerState::GetSkillTreeComponent() const
{
	return SkillTreeComponent;
}

UFrontierQuickSlotComponent* AFrontierPlayerState::GetQuickSlotComponent() const
{
	return QuickSlotComponent;
}

bool AFrontierPlayerState::GrantSkillPointsForLevelUps(const int32 LevelsGained)
{
	return HasAuthority() && SkillTreeComponent
		&& SkillTreeComponent->GrantSkillPointsForLevelUps(LevelsGained);
}

int32 AFrontierPlayerState::GetTeamId() const
{
	return TeamId;
}

void AFrontierPlayerState::SetTeamId(const int32 NewTeamId)
{
	FRONTIER_LOG(Log, TEXT("Requested player team change. PlayerState=%s OldTeam=%d NewTeam=%d"),
		*GetNameSafe(this),
		TeamId,
		NewTeamId);

	if (!HasAuthority())
	{
		FRONTIER_LOG(Warning, TEXT("Ignoring team change request on non-authority PlayerState."));
		return;
	}

	const int32 PreviousTeamId = TeamId;
	TeamId = NewTeamId;
	if (AController* OwnerController = Cast<AController>(GetOwner()))
	{
		if (AFrontierBaseCharacter* ControlledCharacter = Cast<AFrontierBaseCharacter>(OwnerController->GetPawn()))
		{
			ControlledCharacter->SetTeam(AFrontierBaseCharacter::ResolvePlayerTeam(TeamId));
		}
	}
	if (PreviousTeamId == TeamId)
	{
		return;
	}
	OnRep_TeamId(PreviousTeamId);
	ForceNetUpdate();
}

float AFrontierPlayerState::GetDisplayHealth() const
{
	return PublicVitalsSnapshot.Health;
}

FFrontierPublicVitalsSnapshot AFrontierPlayerState::GetPublicVitalsSnapshot() const
{
	return PublicVitalsSnapshot;
}

float AFrontierPlayerState::GetDisplayMaxHealth() const
{
	return PublicVitalsSnapshot.MaxHealth;
}

float AFrontierPlayerState::GetDisplayStamina() const
{
	return PublicVitalsSnapshot.Stamina;
}

float AFrontierPlayerState::GetDisplayMaxStamina() const
{
	return PublicVitalsSnapshot.MaxStamina;
}

bool AFrontierPlayerState::IsDisplayDead() const
{
	return PublicVitalsSnapshot.bIsDead;
}

bool AFrontierPlayerState::IsRaidReady() const
{
	return bRaidReady;
}

void AFrontierPlayerState::SetRaidReady(const bool bNewRaidReady)
{
	if (!HasAuthority() || bRaidReady == bNewRaidReady)
	{
		return;
	}

	bRaidReady = bNewRaidReady;
	OnRep_RaidReady();
	ForceNetUpdate();
}

void AFrontierPlayerState::ApplyPredictedRaidReady(const bool bNewRaidReady)
{
	if (HasAuthority() || bRaidReady == bNewRaidReady)
	{
		return;
	}

	bRaidReady = bNewRaidReady;
	OnReadyChanged.Broadcast(this);
}

void AFrontierPlayerState::LoseRaidItemsOnDeath()
{
	if (!HasAuthority())
	{
		return;
	}

	if (RaidInventoryComponent)
	{
		RaidInventoryComponent->SetSlotsFromSnapshot(TArray<FFrontierInventorySlot>());
	}

	if (LoadoutComponent)
	{
		LoadoutComponent->SetLoadoutSlotsFromSnapshot(TArray<FFrontierLoadoutSlot>());
	}
}

void AFrontierPlayerState::BeginPlay()
{
	Super::BeginPlay();

	SyncVitalsFromAttributes();
	BindAttributeDelegates();

	if (HasAuthority())
	{
		GrantStartupAbilities();
#if WITH_EDITOR
		GrantDebugStartingItems();
#endif
	}
	
	
}

void AFrontierPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFrontierPlayerState, TeamId);
	DOREPLIFETIME(AFrontierPlayerState, PublicVitalsSnapshot);
	DOREPLIFETIME(AFrontierPlayerState, bRaidReady);
}

void AFrontierPlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();
	OnPlayerNameChanged.Broadcast(this);
}

void AFrontierPlayerState::GrantStartupAbilities()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	for (TSubclassOf<UGameplayAbility> AbilityClass : StartupAbilities)
	{
		if (!AbilityClass || AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass))
		{
			continue;
		}

		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
	}
}

void AFrontierPlayerState::GrantDebugStartingItems()
{
	if (!HasAuthority())
	{
		return;
	}

	if (RaidInventoryComponent)
	{
		for (UFrontierItemDataAsset* ItemTemplate : DebugStartingRaidItemTemplates)
		{
			if (!ItemTemplate)
			{
				continue;
			}

			RaidInventoryComponent->AddItem(FFrontierItemInstance::CreateFromItemData(ItemTemplate, 1));
		}
	}

	if (LoadoutComponent)
	{
		for (UFrontierItemDataAsset* ItemTemplate : DebugStartingLoadoutItemTemplates)
		{
			if (!ItemTemplate)
			{
				continue;
			}

			const FFrontierItemInstance ItemInstance = FFrontierItemInstance::CreateFromItemData(ItemTemplate, 1);

			if (const UFrontierWeaponItemDataAsset* WeaponItem = Cast<UFrontierWeaponItemDataAsset>(ItemTemplate))
			{
				LoadoutComponent->EquipItem(WeaponItem->EquipSlot, ItemInstance);
				continue;
			}

			if (const UFrontierArmorItemDataAsset* ArmorItem = Cast<UFrontierArmorItemDataAsset>(ItemTemplate))
			{
				LoadoutComponent->EquipItem(ArmorItem->EquipSlot, ItemInstance);
				continue;
			}

			if (const UFrontierConsumableItemDataAsset* ConsumableItem = Cast<UFrontierConsumableItemDataAsset>(ItemTemplate))
			{
				if (!ConsumableItem->AllowedEquipSlots.IsEmpty())
				{
					LoadoutComponent->EquipItem(ConsumableItem->AllowedEquipSlots[0], ItemInstance);
				}
			}
		}
	}

	if (EquipmentSkillComponent)
	{
		EquipmentSkillComponent->RefreshFromLoadout();
	}
}

void AFrontierPlayerState::BindAttributeDelegates()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (!HealthChangedHandle.IsValid())
	{
		HealthChangedHandle = AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetHealthAttribute())
			.AddUObject(this, &AFrontierPlayerState::HandleAttributeChanged);
	}

	if (!MaxHealthChangedHandle.IsValid())
	{
		MaxHealthChangedHandle = AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetMaxHealthAttribute())
			.AddUObject(this, &AFrontierPlayerState::HandleAttributeChanged);
	}

	if (!StaminaChangedHandle.IsValid())
	{
		StaminaChangedHandle = AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetStaminaAttribute())
			.AddUObject(this, &AFrontierPlayerState::HandleAttributeChanged);
	}

	if (!MaxStaminaChangedHandle.IsValid())
	{
		MaxStaminaChangedHandle = AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetMaxStaminaAttribute())
			.AddUObject(this, &AFrontierPlayerState::HandleAttributeChanged);
	}
}

void AFrontierPlayerState::SyncVitalsFromAttributes()
{
	if (!AttributeSet)
	{
		return;
	}

	SetDisplayVitals(
		AttributeSet->GetHealth(),
		AttributeSet->GetMaxHealth(),
		AttributeSet->GetStamina(),
		AttributeSet->GetMaxStamina(),
		AttributeSet->GetHealth() <= 0.0f);
}

void AFrontierPlayerState::SetDisplayVitals(
	const float NewHealth,
	const float NewMaxHealth,
	const float NewStamina,
	const float NewMaxStamina,
	const bool bNewDead)
{
	FFrontierPublicVitalsSnapshot NewSnapshot;
	NewSnapshot.Health = NewHealth;
	NewSnapshot.MaxHealth = NewMaxHealth;
	NewSnapshot.Stamina = NewStamina;
	NewSnapshot.MaxStamina = NewMaxStamina;
	NewSnapshot.bIsDead = bNewDead;

	const bool bChanged = !PublicVitalsSnapshot.IsEquivalentTo(NewSnapshot);
	PublicVitalsSnapshot = NewSnapshot;
	UpdateLegacyVitalsMirrors();

	if (bChanged)
	{
		OnVitalsChanged.Broadcast(this);
	}
}

void AFrontierPlayerState::UpdateLegacyVitalsMirrors()
{
	DisplayHealth = PublicVitalsSnapshot.Health;
	DisplayMaxHealth = PublicVitalsSnapshot.MaxHealth;
	DisplayStamina = PublicVitalsSnapshot.Stamina;
	DisplayMaxStamina = PublicVitalsSnapshot.MaxStamina;
	bDisplayDead = PublicVitalsSnapshot.bIsDead;
}

void AFrontierPlayerState::HandleAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	SyncVitalsFromAttributes();
}

void AFrontierPlayerState::OnRep_TeamId(const int32 PreviousTeamId)
{
	FRONTIER_LOG(Log, TEXT("Player team replicated. PlayerState=%s OldTeam=%d NewTeam=%d"),
		*GetNameSafe(this),
		PreviousTeamId,
		TeamId);
	if (AFrontierBaseCharacter* ControlledCharacter = Cast<AFrontierBaseCharacter>(GetPawn()))
	{
		ControlledCharacter->SetTeam(AFrontierBaseCharacter::ResolvePlayerTeam(TeamId));
	}
	OnTeamIdChanged.Broadcast(this, TeamId);
}

void AFrontierPlayerState::OnRep_PublicVitalsSnapshot()
{
	UpdateLegacyVitalsMirrors();
	OnVitalsChanged.Broadcast(this);
}

void AFrontierPlayerState::OnRep_RaidReady()
{
	OnReadyChanged.Broadcast(this);
}
