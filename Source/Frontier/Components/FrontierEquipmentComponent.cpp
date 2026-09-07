#include "Components/FrontierEquipmentComponent.h"

#include "AbilitySystem/Effects/FrontierArmorDefenseGameplayEffect.h"
#include "AbilitySystem/Effects/FrontierAggregatedStatsGameplayEffect.h"
#include "AbilitySystem/Effects/FrontierWeaponAttackPowerGameplayEffect.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Character/FrontierBaseCharacter.h"
#include "Character/FrontierCharacterSelectionSubsystem.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Components/FrontierLoadoutComponent.h"
#include "Components/FrontierEquipmentSkillComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Frontier.h"
#include "Game/FrontierPlayerState.h"
#include "Inventory/Items/FrontierWeaponItemDataAsset.h"
#include "Net/UnrealNetwork.h"
#include "Tags/FrontierGameplayTags.h"
#include "TimerManager.h"
#include "Weapons/FrontierWeaponBase.h"
#include "Weapons/FrontierWeaponDataAsset.h"

UFrontierEquipmentComponent::UFrontierEquipmentComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
	ArmorDefenseEffectClass = UFrontierArmorDefenseGameplayEffect::StaticClass();
	WeaponAttackPowerEffectClass = UFrontierWeaponAttackPowerGameplayEffect::StaticClass();
	AggregatedStatsEffectClass = UFrontierAggregatedStatsGameplayEffect::StaticClass();
}

void UFrontierEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UFrontierEquipmentComponent, OwnedWeapons);
	DOREPLIFETIME(UFrontierEquipmentComponent, CurrentWeapon);
	DOREPLIFETIME(UFrontierEquipmentComponent, CurrentWeaponIndex);
	DOREPLIFETIME(UFrontierEquipmentComponent, ReplicatedArmorVisuals);
}

AFrontierWeaponBase* UFrontierEquipmentComponent::GetCurrentWeapon() const
{
	return CurrentWeapon;
}

UFrontierWeaponDataAsset* UFrontierEquipmentComponent::GetCurrentWeaponData() const
{
	return CurrentWeapon ? CurrentWeapon->GetWeaponData() : UnarmedWeaponData.Get();
}

FGameplayAbilitySpecHandle UFrontierEquipmentComponent::GetCurrentWeaponAttackAbilityHandle() const
{
	return CurrentWeaponAttackAbilityHandle;
}

EFrontierElementalType UFrontierEquipmentComponent::GetCurrentWeaponElementalType() const
{
	FFrontierLoadoutSlot LoadoutSlot;
	if (!GetCurrentWeaponLoadoutSlot(LoadoutSlot))
	{
		return EFrontierElementalType::Normal;
	}

	const FFrontierItemTemplateData* ItemTemplateData = LoadoutSlot.ItemInstance.GetItemTemplateData();
	if (ItemTemplateData && ItemTemplateData->ElementalType != EFrontierElementalType::None)
	{
		return ItemTemplateData->ElementalType;
	}

	FRONTIER_LOG(
		Warning,
		TEXT("Equipped weapon has no valid template elemental type. Using Normal. Owner=%s ItemTemplateId=%s"),
		*GetNameSafe(GetOwner()),
		*LoadoutSlot.ItemInstance.GetTemplateId().ToString());
	return EFrontierElementalType::Normal;
}

int32 UFrontierEquipmentComponent::GetCurrentWeaponIndex() const
{
	return CurrentWeaponIndex;
}

UFrontierWeaponItemDataAsset* UFrontierEquipmentComponent::GetCurrentWeaponItemData() const
{
	return nullptr;
}

float UFrontierEquipmentComponent::GetCurrentWeaponAttackPowerBonus() const
{
	FFrontierLoadoutSlot LoadoutSlot;
	if (!GetCurrentWeaponLoadoutSlot(LoadoutSlot))
	{
		return 0.0f;
	}

	return LoadoutSlot.ItemInstance.GetCurrentFinalStatValue(FFrontierGameplayTags::Get().StatAttackPower, 0.0f);
}

float UFrontierEquipmentComponent::GetTotalArmorDefenseBonus() const
{
	const UFrontierLoadoutComponent* LoadoutComponent = GetLoadoutComponent();
	if (!LoadoutComponent)
	{
		return 0.0f;
	}

	float TotalDefense = 0.0f;
	const EFrontierEquipmentSlot ArmorSlots[] = {
		EFrontierEquipmentSlot::Helmet,
		EFrontierEquipmentSlot::Chest,
		EFrontierEquipmentSlot::Gloves,
		EFrontierEquipmentSlot::Boots
	};

	for (const EFrontierEquipmentSlot SlotType : ArmorSlots)
	{
		FFrontierLoadoutSlot LoadoutSlot;
		if (!LoadoutComponent->FindLoadoutSlot(SlotType, LoadoutSlot) || !LoadoutSlot.bOccupied || !LoadoutSlot.ItemInstance.IsValid())
		{
			continue;
		}

		if (LoadoutSlot.ItemInstance.GetCategory() != EFrontierItemCategory::Armor)
		{
			continue;
		}

		TotalDefense += LoadoutSlot.ItemInstance.GetCurrentFinalStatValue(FFrontierGameplayTags::Get().StatDefense, 0.0f);
	}

	return TotalDefense;
}

void UFrontierEquipmentComponent::RefreshFromLoadout()
{
	BindLoadoutComponent();
	RefreshArmorVisuals();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		RebuildWeaponsFromLoadout();
		RefreshEquipmentStatsEffect();
	}
	else
	{
		// With no equipped weapon, CurrentWeapon remains null and no weapon
		// replication callback is guaranteed to run. Explicitly initialize the
		// unarmed data path on clients as well.
		RefreshWeaponVisualState();
	}
}

bool UFrontierEquipmentComponent::SelectWeaponByLoadoutSlot(const EFrontierEquipmentSlot SlotType)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()
		|| (SlotType != EFrontierEquipmentSlot::MainWeapon && SlotType != EFrontierEquipmentSlot::SubWeapon))
	{
		return false;
	}

	const UFrontierLoadoutComponent* LoadoutComponent = GetLoadoutComponent();
	if (!LoadoutComponent)
	{
		return false;
	}

	int32 WeaponIndex = 0;
	for (const FFrontierLoadoutSlot& LoadoutSlot : LoadoutComponent->GetLoadoutSlots())
	{
		if (!LoadoutSlot.bOccupied
			|| !LoadoutSlot.ItemInstance.IsValid()
			|| LoadoutSlot.ItemInstance.GetCategory() != EFrontierItemCategory::Weapon)
		{
			continue;
		}

		if (LoadoutSlot.SlotType == SlotType && OwnedWeapons.IsValidIndex(WeaponIndex))
		{
			EquipWeaponByIndex(WeaponIndex);
			return true;
		}
		++WeaponIndex;
	}

	return false;
}

void UFrontierEquipmentComponent::GrantItemSkillsFromServerData(const FFrontierItemInstance& EquippedItem)
{
	const FString ItemInstanceId = EquippedItem.ItemInstanceId.ToString(EGuidFormats::DigitsWithHyphensLower);
	const FString ItemTemplateId = EquippedItem.GetTemplateId().ToString();
	AFrontierBaseCharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		FRONTIER_LOG(Error, TEXT("Client attempted to call server-only equipment skill grant. Owner=%s ItemInstanceId=%s ItemTemplateId=%s"),
			*GetNameSafe(GetOwner()),
			*ItemInstanceId,
			*ItemTemplateId);
		return;
	}

	AFrontierPlayerState* FrontierPlayerState = OwnerCharacter->GetPlayerState<AFrontierPlayerState>();
	UFrontierEquipmentSkillComponent* EquipmentSkillComponent = FrontierPlayerState ? FrontierPlayerState->GetEquipmentSkillComponent() : nullptr;
	if (!EquipmentSkillComponent)
	{
		FRONTIER_LOG(Error, TEXT("Equipment skill grant failed because EquipmentSkillComponent is missing. Owner=%s ItemInstanceId=%s ItemTemplateId=%s"),
			*GetNameSafe(GetOwner()),
			*ItemInstanceId,
			*ItemTemplateId);
		return;
	}

	EquipmentSkillComponent->GrantItemSkillsFromServerData(EquippedItem);
}

void UFrontierEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	BindLoadoutComponent();
	RefreshArmorVisuals();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (GetLoadoutComponent())
		{
			RebuildWeaponsFromLoadout();
			RefreshEquipmentStatsEffect();
		}
		else
		{
			//InitializeDefaultWeapons();
			RefreshWeaponVisualState();
			RefreshEquipmentStatsEffect();
		}
	}
	else
	{
		RefreshWeaponVisualState();
	}
}

void UFrontierEquipmentComponent::OnRep_OwnedWeapons()
{
	
	RefreshWeaponVisualState();
}

void UFrontierEquipmentComponent::OnRep_CurrentWeapon()
{
	
	RefreshWeaponVisualState();
}

void UFrontierEquipmentComponent::OnRep_ReplicatedArmorVisuals()
{
	RebuildArmorComponentsFromReplicatedVisuals();
}

void UFrontierEquipmentComponent::HandleLoadoutChanged(const TArray<FFrontierLoadoutSlot>& InSlots)
{
	RefreshArmorVisuals();

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	RebuildWeaponsFromLoadout();
	RefreshEquipmentStatsEffect();
}

void UFrontierEquipmentComponent::RefreshArmorVisuals()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		RebuildReplicatedArmorVisuals();
	}

	RebuildArmorComponentsFromReplicatedVisuals();
}

void UFrontierEquipmentComponent::RefreshWeaponAnimationLayer()
{
	AFrontierBaseCharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter || OwnerCharacter->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	if (!CharacterMesh)
	{
		return;
	}

	if (!CharacterMesh->IsRegistered() || !CharacterMesh->GetAnimInstance())
	{
		if (!bWeaponAnimationLayerRefreshPending)
		{
			if (UWorld* World = GetWorld())
			{
				bWeaponAnimationLayerRefreshPending = true;
				World->GetTimerManager().SetTimerForNextTick(
					FTimerDelegate::CreateUObject(this, &UFrontierEquipmentComponent::ApplyWeaponAnimationLayer));
			}
		}
		return;
	}

	ApplyWeaponAnimationLayer();
}

void UFrontierEquipmentComponent::ApplyWeaponAnimationLayer()
{
	bWeaponAnimationLayerRefreshPending = false;

	AFrontierBaseCharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter || OwnerCharacter->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh();
	UAnimInstance* MainAnimInstance = CharacterMesh ? CharacterMesh->GetAnimInstance() : nullptr;
	if (!CharacterMesh || !CharacterMesh->IsRegistered() || !MainAnimInstance)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Weapon animation layer could not be applied because the character AnimInstance is not ready. Owner=%s MeshRegistered=%d"),
			*GetNameSafe(GetOwner()),
			CharacterMesh && CharacterMesh->IsRegistered());
		return;
	}

	if (LinkedWeaponAnimLayerClass)
	{
		CharacterMesh->UnlinkAnimClassLayers(LinkedWeaponAnimLayerClass);
		LinkedWeaponAnimLayerClass = nullptr;
	}

	const UFrontierWeaponDataAsset* WeaponData = GetCurrentWeaponData();
	if (!WeaponData || WeaponData->AnimLayerClass.IsNull())
	{
		return;
	}

	if (!bCurrentWeaponAttackAssetsReady || !PreloadedAttackAnimLayerClass)
	{
		return;
	}

	UClass* ResolvedAnimLayerClass = PreloadedAttackAnimLayerClass;
	if (!ResolvedAnimLayerClass)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Failed to load weapon animation layer class. Owner=%s WeaponData=%s Asset=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(WeaponData),
			*WeaponData->AnimLayerClass.ToSoftObjectPath().ToString());
		return;
	}

	CharacterMesh->LinkAnimClassLayers(ResolvedAnimLayerClass);

	const bool bLayerWasLinked =
		MainAnimInstance->GetLinkedAnimLayerInstanceByClass(ResolvedAnimLayerClass, true) != nullptr;

	if (!bLayerWasLinked)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Weapon animation layer class was loaded but no Linked Anim Layer node accepted it. Owner=%s WeaponData=%s MainAnimClass=%s LayerClass=%s. Verify that the main AnimBP contains a Linked Anim Layer node for the same ALI implemented by the layer class."),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(WeaponData),
			*GetNameSafe(MainAnimInstance->GetClass()),
			*GetNameSafe(ResolvedAnimLayerClass));
		return;
	}

	LinkedWeaponAnimLayerClass = ResolvedAnimLayerClass;
}

void UFrontierEquipmentComponent::RebuildReplicatedArmorVisuals()
{
	AActor* OwnerActor = GetOwner();
	UFrontierLoadoutComponent* LoadoutComponent = GetLoadoutComponent();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !LoadoutComponent)
	{
		return;
	}

	TArray<FFrontierReplicatedArmorVisual> NewVisuals;
	const EFrontierCharacterType CharacterType = ResolveOwnerCharacterType();
	for (const FFrontierLoadoutSlot& LoadoutSlot : LoadoutComponent->GetLoadoutSlots())
	{
		if (!LoadoutSlot.bOccupied
			|| !LoadoutSlot.ItemInstance.IsValid()
			|| LoadoutSlot.ItemInstance.GetCategory() != EFrontierItemCategory::Armor)
		{
			continue;
		}

		FFrontierArmorAppearanceData Appearance;
		if (!LoadoutSlot.ItemInstance.ResolveArmorAppearance(CharacterType, Appearance))
		{
			FRONTIER_LOG(
				Warning,
				TEXT("Armor appearance is missing for public visual. Owner=%s ItemTemplateId=%s CharacterType=%s"),
				*GetNameSafe(OwnerActor),
				*LoadoutSlot.ItemInstance.GetTemplateId().ToString(),
				*UEnum::GetValueAsString(CharacterType));
			continue;
		}

		FFrontierReplicatedArmorVisual& Visual = NewVisuals.AddDefaulted_GetRef();
		Visual.SlotType = LoadoutSlot.SlotType;
		Visual.CharacterType = CharacterType;
		Visual.ItemTemplateId = LoadoutSlot.ItemInstance.GetTemplateId();
		Visual.StaticMesh = Appearance.EquipmentStaticMesh;
		Visual.SkeletalMesh = Appearance.EquipmentSkeletalMesh;
		Visual.AttachSocketName = Appearance.CharacterAttachSocketName;
		Visual.RelativeTransform = Appearance.EquipOffset;
	}

	ReplicatedArmorVisuals = MoveTemp(NewVisuals);
	OwnerActor->ForceNetUpdate();
}

void UFrontierEquipmentComponent::RebuildArmorComponentsFromReplicatedVisuals()
{
	ClearArmorVisuals();

	AFrontierBaseCharacter* OwnerCharacter = GetOwnerCharacter();
	USkeletalMeshComponent* CharacterMesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	if (!OwnerCharacter
		|| !CharacterMesh
		|| OwnerCharacter->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	for (const FFrontierReplicatedArmorVisual& Visual : ReplicatedArmorVisuals)
	{
		USkeletalMeshComponent* AttachMesh = ResolveArmorAttachMesh(Visual.SlotType);
		if (!AttachMesh)
		{
			continue;
		}

		if (!Visual.StaticMesh.IsNull() && !Visual.SkeletalMesh.IsNull())
		{
			FRONTIER_LOG(
				Warning,
				TEXT("Armor has both StaticMesh and SkeletalMesh; StaticMesh takes priority. ItemTemplateId=%s CharacterType=%s"),
				*Visual.ItemTemplateId.ToString(),
				*UEnum::GetValueAsString(Visual.CharacterType));
		}

		USceneComponent* ArmorComponent = nullptr;
		if (!Visual.StaticMesh.IsNull())
		{
			UStaticMesh* StaticMesh = Visual.StaticMesh.LoadSynchronous();
			if (StaticMesh)
			{
				UStaticMeshComponent* StaticMeshComponent =
					NewObject<UStaticMeshComponent>(OwnerCharacter);
				StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				StaticMeshComponent->SetGenerateOverlapEvents(false);
				StaticMeshComponent->SetStaticMesh(StaticMesh);
				ArmorComponent = StaticMeshComponent;
			}
		}
		else if (!Visual.SkeletalMesh.IsNull())
		{
			USkeletalMesh* SkeletalMesh = Visual.SkeletalMesh.LoadSynchronous();
			if (SkeletalMesh)
			{
				USkeletalMeshComponent* SkeletalMeshComponent =
					NewObject<USkeletalMeshComponent>(OwnerCharacter);
				SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				SkeletalMeshComponent->SetGenerateOverlapEvents(false);
				SkeletalMeshComponent->SetSkeletalMesh(SkeletalMesh);
				ArmorComponent = SkeletalMeshComponent;
			}
		}

		if (!ArmorComponent)
		{
			FRONTIER_LOG(
				Warning,
				TEXT("Failed to load armor visual mesh. ItemTemplateId=%s CharacterType=%s"),
				*Visual.ItemTemplateId.ToString(),
				*UEnum::GetValueAsString(Visual.CharacterType));
			continue;
		}

		if (!Visual.AttachSocketName.IsNone()
			&& !AttachMesh->DoesSocketExist(Visual.AttachSocketName))
		{
			FRONTIER_LOG(
				Warning,
				TEXT("Armor attach socket does not exist. ItemTemplateId=%s CharacterType=%s Socket=%s"),
				*Visual.ItemTemplateId.ToString(),
				*UEnum::GetValueAsString(Visual.CharacterType),
				*Visual.AttachSocketName.ToString());
			ArmorComponent->DestroyComponent();
			continue;
		}

		OwnerCharacter->AddInstanceComponent(ArmorComponent);
		ArmorComponent->RegisterComponent();
		ArmorComponent->AttachToComponent(
			AttachMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			Visual.AttachSocketName);
		ArmorComponent->SetRelativeTransform(Visual.RelativeTransform);
		AttachedArmorComponents.Add(ArmorComponent);
	}
}

USkeletalMeshComponent* UFrontierEquipmentComponent::ResolveArmorAttachMesh(
	const EFrontierEquipmentSlot SlotType) const
{
	AFrontierBaseCharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter)
	{
		return nullptr;
	}

#if 0 // TEMP: Preserved modular armor-slot routing. Do not delete.
	const AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(OwnerCharacter);
	if (!PlayerCharacter)
	{
		return OwnerCharacter->GetMesh();
	}

	switch (SlotType)
	{
	case EFrontierEquipmentSlot::Helmet:
		return PlayerCharacter->GetHeadMesh();
	case EFrontierEquipmentSlot::Chest:
		return PlayerCharacter->GetArmorMesh();
	case EFrontierEquipmentSlot::Gloves:
		return PlayerCharacter->GetGloveMesh();
	case EFrontierEquipmentSlot::Boots:
		return PlayerCharacter->GetGreavesMesh();
	default:
		return OwnerCharacter->GetMesh();
	}
#endif

	// TEMP: Armor and helmets use the same-named sockets on the full character mesh.
	(void)SlotType;
	return OwnerCharacter->GetMesh();
}

void UFrontierEquipmentComponent::ClearArmorVisuals()
{
	AActor* OwnerActor = GetOwner();
	for (USceneComponent* ArmorComponent : AttachedArmorComponents)
	{
		if (!ArmorComponent)
		{
			continue;
		}

		if (OwnerActor)
		{
			OwnerActor->RemoveInstanceComponent(ArmorComponent);
		}
		ArmorComponent->DestroyComponent();
	}
	AttachedArmorComponents.Reset();
}

EFrontierCharacterType UFrontierEquipmentComponent::ResolveOwnerCharacterType() const
{
	const AFrontierBaseCharacter* OwnerCharacter = GetOwnerCharacter();
	if (const AFrontierPlayerCharacter* PlayerCharacter =
		Cast<AFrontierPlayerCharacter>(OwnerCharacter))
	{
		return PlayerCharacter->GetSelectedCharacterType();
	}

	if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled())
	{
		return EFrontierCharacterType::DarkKnight;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFrontierCharacterSelectionSubsystem* SelectionSubsystem = GameInstance
		? GameInstance->GetSubsystem<UFrontierCharacterSelectionSubsystem>()
		: nullptr;
	return SelectionSubsystem
		? SelectionSubsystem->GetSelectedCharacterType()
		: EFrontierCharacterType::DarkKnight;
}

void UFrontierEquipmentComponent::InitializeDefaultWeapons()
{
	

	AFrontierBaseCharacter* OwnerCharacter = GetOwnerCharacter();
	UWorld* World = GetWorld();
	if (!OwnerCharacter || !World || DefaultWeaponClasses.Num() == 0)
	{
		return;
	}

	for (TSubclassOf<AFrontierWeaponBase> WeaponClass : DefaultWeaponClasses)
	{
		if (!WeaponClass)
		{
			continue;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = OwnerCharacter;
		SpawnParameters.Instigator = OwnerCharacter;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AFrontierWeaponBase* SpawnedWeapon = World->SpawnActor<AFrontierWeaponBase>(
			WeaponClass,
			OwnerCharacter->GetActorTransform(),
			SpawnParameters);

		if (!SpawnedWeapon)
		{
			continue;
		}

		OwnedWeapons.Add(SpawnedWeapon);
	}

	if (bStartUnarmed && HasUnarmedState())
	{
		EquipWeaponByIndex(INDEX_NONE);
	}
	else if (OwnedWeapons.Num() > 0)
	{
		EquipWeaponByIndex(0);
	}
	else
	{
		RefreshWeaponVisualState();
	}
}

void UFrontierEquipmentComponent::RebuildWeaponsFromLoadout()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	UFrontierLoadoutComponent* LoadoutComponent = GetLoadoutComponent();
	if (!LoadoutComponent)
	{
		return;
	}

	const bool bPreferSubWeapon = CurrentWeaponIndex == 1;
	DestroyOwnedWeapons();

	const EFrontierEquipmentSlot WeaponSlots[] = {
		EFrontierEquipmentSlot::MainWeapon,
		EFrontierEquipmentSlot::SubWeapon
	};

	for (const EFrontierEquipmentSlot SlotType : WeaponSlots)
	{
		FFrontierLoadoutSlot LoadoutSlot;
		if (!LoadoutComponent->FindLoadoutSlot(SlotType, LoadoutSlot))
		{
			continue;
		}

		if (AFrontierWeaponBase* SpawnedWeapon = SpawnWeaponFromLoadoutSlot(LoadoutSlot))
		{
			OwnedWeapons.Add(SpawnedWeapon);
		}
	}

	if (OwnedWeapons.IsEmpty())
	{
		EquipWeaponByIndex(INDEX_NONE);
		return;
	}

	if (bPreferSubWeapon && OwnedWeapons.Num() > 1)
	{
		EquipWeaponByIndex(1);
		return;
	}

	EquipWeaponByIndex(0);
}

void UFrontierEquipmentComponent::DestroyOwnedWeapons()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	for (AFrontierWeaponBase* Weapon : OwnedWeapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->Destroy();
		}
	}

	OwnedWeapons.Empty();
	CurrentWeapon = nullptr;
	CurrentWeaponIndex = INDEX_NONE;
	RefreshWeaponAttackAbility();
}

void UFrontierEquipmentComponent::EquipWeaponByIndex(const int32 NewWeaponIndex)
{
	FRONTIER_LOG(Log, TEXT("EquipWeaponByIndex called. Owner=%s NewWeaponIndex=%d"), *GetNameSafe(GetOwner()), NewWeaponIndex);

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	if (NewWeaponIndex == INDEX_NONE)
	{
		CurrentWeaponIndex = INDEX_NONE;
		CurrentWeapon = nullptr;
		RefreshWeaponVisualState();
		RefreshWeaponAttackPowerEffect();
		return;
	}

	if (!OwnedWeapons.IsValidIndex(NewWeaponIndex))
	{
		return;
	}

	CurrentWeaponIndex = NewWeaponIndex;
	CurrentWeapon = OwnedWeapons[NewWeaponIndex];
	RefreshWeaponVisualState();
	RefreshWeaponAttackPowerEffect();
}

void UFrontierEquipmentComponent::RefreshWeaponVisualState()
{
	AFrontierBaseCharacter* OwnerCharacter = GetOwnerCharacter();
	PreloadCurrentWeaponAttackAssets();
	if (OwnerCharacter && OwnerCharacter->HasAuthority())
	{
		RefreshWeaponAttackAbility();
	}

	USkeletalMeshComponent* CharacterMesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	if (!CharacterMesh)
	{
		return;
	}

	if (IsValid(CurrentWeapon) && !OwnedWeapons.Contains(CurrentWeapon))
	{
		AttachWeaponToCharacterMesh(CurrentWeapon, CharacterMesh);
		CurrentWeapon->SetWeaponActive(true);
	}

	for (AFrontierWeaponBase* Weapon : OwnedWeapons)
	{
		if (!IsValid(Weapon))
		{
			continue;
		}

		const bool bIsCurrentWeapon = Weapon == CurrentWeapon;
		Weapon->SetWeaponActive(bIsCurrentWeapon);
		AttachWeaponToCharacterMesh(Weapon, CharacterMesh);
	}

	RefreshWeaponAnimationLayer();
	OnCurrentWeaponChanged.Broadcast(CurrentWeapon, CurrentWeaponIndex);
}

void UFrontierEquipmentComponent::PreloadCurrentWeaponAttackAssets()
{
	const UFrontierWeaponDataAsset* WeaponData = GetCurrentWeaponData();
	if (!WeaponData)
	{
		if (!CurrentWeapon && !UnarmedWeaponData)
		{
			FRONTIER_LOG(
				Warning,
				TEXT("Unarmed weapon data is not assigned. Owner=%s. Assign UnarmedWeaponData on the EquipmentComponent."),
				*GetNameSafe(GetOwner()));
		}

		PreloadedAttackWeaponData = nullptr;
		PreloadedAttackAssets.Reset();
		PreloadedAttackAbilityClass = nullptr;
		PreloadedAttackAnimLayerClass = nullptr;
		bCurrentWeaponAttackAssetsReady = false;
		AttackAssetPreloadHandle.Reset();
		return;
	}

	if (PreloadedAttackWeaponData == WeaponData
		&& (bCurrentWeaponAttackAssetsReady || AttackAssetPreloadHandle.IsValid()))
	{
		return;
	}

	PreloadedAttackWeaponData = const_cast<UFrontierWeaponDataAsset*>(WeaponData);
	PreloadedAttackAssets.Reset();
	PreloadedAttackAssets.SetNum(WeaponData->BasicComboAttacks.Num());
	PreloadedAttackAbilityClass = nullptr;
	PreloadedAttackAnimLayerClass = nullptr;
	bCurrentWeaponAttackAssetsReady = false;
	AttackAssetPreloadHandle.Reset();

	TArray<FSoftObjectPath> AssetPaths;
	for (const FFrontierAttackActionData& AttackData : WeaponData->BasicComboAttacks)
	{
		if (!AttackData.AttackMontage.IsNull())
		{
			AssetPaths.AddUnique(AttackData.AttackMontage.ToSoftObjectPath());
		}
		if (!AttackData.DamageEffectClass.IsNull())
		{
			AssetPaths.AddUnique(AttackData.DamageEffectClass.ToSoftObjectPath());
		}
		for (const TSoftClassPtr<UGameplayEffect>& AdditionalEffectClass : AttackData.AdditionalHitEffectClasses)
		{
			if (!AdditionalEffectClass.IsNull())
			{
				AssetPaths.AddUnique(AdditionalEffectClass.ToSoftObjectPath());
			}
		}
	}

	if (!WeaponData->AttackAbilityClass.IsNull())
	{
		AssetPaths.AddUnique(WeaponData->AttackAbilityClass.ToSoftObjectPath());
	}

	if (!WeaponData->AnimLayerClass.IsNull())
	{
		AssetPaths.AddUnique(WeaponData->AnimLayerClass.ToSoftObjectPath());
	}

	if (AssetPaths.IsEmpty())
	{
		CompletePreloadCurrentWeaponAttackAssets();
		return;
	}

	AttackAssetPreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		AssetPaths,
		FStreamableDelegate::CreateUObject(
			this,
			&UFrontierEquipmentComponent::CompletePreloadCurrentWeaponAttackAssets),
		FStreamableManager::AsyncLoadHighPriority);
}

void UFrontierEquipmentComponent::CompletePreloadCurrentWeaponAttackAssets()
{
	const UFrontierWeaponDataAsset* WeaponData = PreloadedAttackWeaponData;
	if (!WeaponData)
	{
		return;
	}

	for (int32 ComboIndex = 0; ComboIndex < WeaponData->BasicComboAttacks.Num(); ++ComboIndex)
	{
		if (!PreloadedAttackAssets.IsValidIndex(ComboIndex))
		{
			break;
		}

		const FFrontierAttackActionData& AttackData = WeaponData->BasicComboAttacks[ComboIndex];
		FFrontierPreloadedAttackAssets& CachedAssets = PreloadedAttackAssets[ComboIndex];
		CachedAssets.AttackMontage = AttackData.AttackMontage.Get();
		CachedAssets.DamageEffectClass = AttackData.DamageEffectClass.Get();
		CachedAssets.AdditionalHitEffectClasses.Reset();
		for (const TSoftClassPtr<UGameplayEffect>& AdditionalEffectClass : AttackData.AdditionalHitEffectClasses)
		{
			if (TSubclassOf<UGameplayEffect> LoadedEffectClass = AdditionalEffectClass.Get())
			{
				CachedAssets.AdditionalHitEffectClasses.Add(LoadedEffectClass);
			}
		}
	}

	PreloadedAttackAbilityClass = WeaponData->AttackAbilityClass.Get();
	PreloadedAttackAnimLayerClass = WeaponData->AnimLayerClass.Get();
	bCurrentWeaponAttackAssetsReady = true;
	AttackAssetPreloadHandle.Reset();

	if (AFrontierBaseCharacter* OwnerCharacter = GetOwnerCharacter())
	{
		RefreshWeaponAnimationLayer();
		if (OwnerCharacter->HasAuthority())
		{
			RefreshWeaponAttackAbility();
		}
	}
}

bool UFrontierEquipmentComponent::AreCurrentWeaponAttackAssetsReady() const
{
	return bCurrentWeaponAttackAssetsReady;
}

TSubclassOf<UGameplayAbility> UFrontierEquipmentComponent::GetPreloadedAttackAbilityClass() const
{
	return bCurrentWeaponAttackAssetsReady ? PreloadedAttackAbilityClass : nullptr;
}

bool UFrontierEquipmentComponent::GetPreloadedAttackActionAssets(
	const int32 ComboIndex,
	TObjectPtr<UAnimMontage>& OutAttackMontage,
	TSubclassOf<UGameplayEffect>& OutDamageEffectClass,
	TArray<TSubclassOf<UGameplayEffect>>& OutAdditionalHitEffectClasses) const
{
	OutAttackMontage = nullptr;
	OutDamageEffectClass = nullptr;
	OutAdditionalHitEffectClasses.Reset();

	if (!bCurrentWeaponAttackAssetsReady || !PreloadedAttackAssets.IsValidIndex(ComboIndex))
	{
		return false;
	}

	const FFrontierPreloadedAttackAssets& CachedAssets = PreloadedAttackAssets[ComboIndex];
	OutAttackMontage = CachedAssets.AttackMontage;
	OutDamageEffectClass = CachedAssets.DamageEffectClass;
	OutAdditionalHitEffectClasses = CachedAssets.AdditionalHitEffectClasses;
	return OutAttackMontage != nullptr;
}

void UFrontierEquipmentComponent::RefreshCurrentWeaponPresentation()
{
	RefreshWeaponAnimationLayer();
	OnCurrentWeaponChanged.Broadcast(CurrentWeapon, CurrentWeaponIndex);
}

void UFrontierEquipmentComponent::RefreshWeaponAttackAbility()
{
	AFrontierBaseCharacter* OwnerCharacter = GetOwnerCharacter();
	UFrontierAbilitySystemComponent* AbilitySystemComponent = OwnerCharacter
		? OwnerCharacter->GetFrontierAbilitySystemComponent()
		: nullptr;
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	const UFrontierWeaponDataAsset* WeaponData = GetCurrentWeaponData();
	if (!AreCurrentWeaponAttackAssetsReady())
	{
		return;
	}

	TSubclassOf<UGameplayAbility> DesiredAbilityClass = PreloadedAttackAbilityClass;

	if (CurrentWeaponAttackAbilityHandle.IsValid()
		&& CurrentWeaponAttackAbilityClass == DesiredAbilityClass
		&& AbilitySystemComponent->FindAbilitySpecFromHandle(CurrentWeaponAttackAbilityHandle))
	{
		return;
	}

	if (CurrentWeaponAttackAbilityHandle.IsValid())
	{
		AbilitySystemComponent->CancelAbilityHandle(CurrentWeaponAttackAbilityHandle);
		AbilitySystemComponent->ClearAbility(CurrentWeaponAttackAbilityHandle);
		CurrentWeaponAttackAbilityHandle = FGameplayAbilitySpecHandle();
		CurrentWeaponAttackAbilityClass = nullptr;
	}

	if (!DesiredAbilityClass)
	{
		FRONTIER_LOG(Warning, TEXT("Current weapon has no attack ability. Owner=%s WeaponData=%s"),
			*GetNameSafe(OwnerCharacter),
			*GetNameSafe(WeaponData));
		return;
	}

	CurrentWeaponAttackAbilityClass = DesiredAbilityClass;
	CurrentWeaponAttackAbilityHandle = AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(DesiredAbilityClass, 1, INDEX_NONE, this));
}

void UFrontierEquipmentComponent::RefreshWeaponAttackPowerEffect()
{
	RefreshEquipmentStatsEffect();
}

void UFrontierEquipmentComponent::RefreshArmorDefenseEffect()
{
	RefreshEquipmentStatsEffect();
}

float UFrontierEquipmentComponent::GetCurrentWeaponStatValue(const FGameplayTag StatTag) const
{
	if (!StatTag.IsValid())
	{
		return 0.0f;
	}

	FFrontierLoadoutSlot LoadoutSlot;
	return GetCurrentWeaponLoadoutSlot(LoadoutSlot)
		? LoadoutSlot.ItemInstance.GetCurrentFinalStatValue(StatTag, 0.0f)
		: 0.0f;
}

bool UFrontierEquipmentComponent::GetCurrentWeaponLoadoutSlot(FFrontierLoadoutSlot& OutLoadoutSlot) const
{
	OutLoadoutSlot = FFrontierLoadoutSlot();

	if (CurrentWeaponIndex == INDEX_NONE)
	{
		return false;
	}

	const UFrontierLoadoutComponent* LoadoutComponent = GetLoadoutComponent();
	if (!LoadoutComponent)
	{
		return false;
	}

	const EFrontierEquipmentSlot WeaponSlots[] = {
		EFrontierEquipmentSlot::MainWeapon,
		EFrontierEquipmentSlot::SubWeapon
	};

	int32 SpawnedWeaponIndex = 0;
	for (const EFrontierEquipmentSlot SlotType : WeaponSlots)
	{
		FFrontierLoadoutSlot CandidateSlot;
		if (!LoadoutComponent->FindLoadoutSlot(SlotType, CandidateSlot)
			|| !CandidateSlot.bOccupied
			|| !CandidateSlot.ItemInstance.IsValid()
			|| CandidateSlot.ItemInstance.GetCategory() != EFrontierItemCategory::Weapon
			|| CandidateSlot.ItemInstance.GetWeaponData().IsNull())
		{
			continue;
		}

		if (SpawnedWeaponIndex == CurrentWeaponIndex)
		{
			OutLoadoutSlot = MoveTemp(CandidateSlot);
			return true;
		}

		++SpawnedWeaponIndex;
	}

	return false;
}

float UFrontierEquipmentComponent::GetTotalDefensiveStatValue(const FGameplayTag StatTag) const
{
	if (!StatTag.IsValid())
	{
		return 0.0f;
	}

	const UFrontierLoadoutComponent* LoadoutComponent = GetLoadoutComponent();
	if (!LoadoutComponent)
	{
		return 0.0f;
	}

	const EFrontierEquipmentSlot DefensiveSlots[] = {
		EFrontierEquipmentSlot::Helmet,
		EFrontierEquipmentSlot::Chest,
		EFrontierEquipmentSlot::Gloves,
		EFrontierEquipmentSlot::Boots,
		EFrontierEquipmentSlot::Necklace,
		EFrontierEquipmentSlot::Ring
	};

	float TotalValue = 0.0f;
	for (const EFrontierEquipmentSlot SlotType : DefensiveSlots)
	{
		FFrontierLoadoutSlot LoadoutSlot;
		if (LoadoutComponent->FindLoadoutSlot(SlotType, LoadoutSlot)
			&& LoadoutSlot.bOccupied
			&& LoadoutSlot.ItemInstance.IsValid())
		{
			TotalValue += LoadoutSlot.ItemInstance.GetCurrentFinalStatValue(StatTag, 0.0f);
		}
	}

	return TotalValue;
}

void UFrontierEquipmentComponent::RefreshEquipmentStatsEffect()
{
	AFrontierBaseCharacter* OwnerCharacter = GetOwnerCharacter();
	UFrontierAbilitySystemComponent* OwnerASC = OwnerCharacter ? OwnerCharacter->GetFrontierAbilitySystemComponent() : nullptr;
	if (!OwnerASC)
	{
		return;
	}

	// Remove legacy handles as well so hot-reloaded instances cannot double-apply stats.
	if (ActiveWeaponAttackPowerEffectHandle.IsValid())
	{
		OwnerASC->RemoveActiveGameplayEffect(ActiveWeaponAttackPowerEffectHandle);
		ActiveWeaponAttackPowerEffectHandle.Invalidate();
	}
	if (ActiveArmorDefenseEffectHandle.IsValid())
	{
		OwnerASC->RemoveActiveGameplayEffect(ActiveArmorDefenseEffectHandle);
		ActiveArmorDefenseEffectHandle.Invalidate();
	}
	if (ActiveAggregatedStatsEffectHandle.IsValid())
	{
		OwnerASC->RemoveActiveGameplayEffect(ActiveAggregatedStatsEffectHandle);
		ActiveAggregatedStatsEffectHandle.Invalidate();
	}

	if (!AggregatedStatsEffectClass)
	{
		return;
	}

	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	const float AttackPower = GetCurrentWeaponStatValue(Tags.StatAttackPower);
	const float Defense = GetTotalDefensiveStatValue(Tags.StatDefense);
	const float FireAttackPower = GetCurrentWeaponStatValue(Tags.StatFireAttackPower);
	const float IceAttackPower = GetCurrentWeaponStatValue(Tags.StatIceAttackPower);
	const float LightningAttackPower = GetCurrentWeaponStatValue(Tags.StatLightningAttackPower);
	const float PoisonAttackPower = GetCurrentWeaponStatValue(Tags.StatPoisonAttackPower);
	const float FireResistance = GetTotalDefensiveStatValue(Tags.StatFireResistance);
	const float IceResistance = GetTotalDefensiveStatValue(Tags.StatIceResistance);
	const float LightningResistance = GetTotalDefensiveStatValue(Tags.StatLightningResistance);
	const float PoisonResistance = GetTotalDefensiveStatValue(Tags.StatPoisonResistance);

	FGameplayEffectContextHandle EffectContext = OwnerASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	FGameplayEffectSpecHandle SpecHandle = OwnerASC->MakeOutgoingSpec(
		AggregatedStatsEffectClass,
		1.0f,
		EffectContext);

	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataAttackPower, AttackPower);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataDefense, Defense);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataFireAttackPower, FireAttackPower);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataIceAttackPower, IceAttackPower);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataLightningAttackPower, LightningAttackPower);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataPoisonAttackPower, PoisonAttackPower);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataFireResistance, FireResistance);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataIceResistance, IceResistance);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataLightningResistance, LightningResistance);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataPoisonResistance, PoisonResistance);
	// Passive progression owns these attributes. Equipment keeps its independent aggregate effect neutral.
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataSwordAttackPower, 0.0f);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataAxeAttackPower, 0.0f);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataMaxHealthBonus, 0.0f);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataMaxStaminaBonus, 0.0f);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataSwordAttackSpeedBonus, 0.0f);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataAxeAttackSpeedBonus, 0.0f);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataMoveSpeedBonus, 0.0f);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataJumpPowerBonus, 0.0f);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataLobbyStorageSlotBonus, 0.0f);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataRaidInventorySlotBonus, 0.0f);
	ActiveAggregatedStatsEffectHandle = OwnerASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void UFrontierEquipmentComponent::AttachWeaponToCharacterMesh(AFrontierWeaponBase* Weapon, USkeletalMeshComponent* CharacterMesh) const
{
	if (!IsValid(Weapon) || !CharacterMesh)
	{
		return;
	}

	Weapon->AttachToComponent(
		CharacterMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		Weapon->GetCharacterAttachSocketName());
	Weapon->SetActorRelativeTransform(Weapon->GetEquipOffset());
}

AFrontierBaseCharacter* UFrontierEquipmentComponent::GetOwnerCharacter() const
{
	return Cast<AFrontierBaseCharacter>(GetOwner());
}

UFrontierLoadoutComponent* UFrontierEquipmentComponent::GetLoadoutComponent() const
{
	const AFrontierBaseCharacter* OwnerCharacter = GetOwnerCharacter();
	return OwnerCharacter ? OwnerCharacter->GetLoadoutComponent() : nullptr;
}

void UFrontierEquipmentComponent::BindLoadoutComponent()
{
	UFrontierLoadoutComponent* LoadoutComponent = GetLoadoutComponent();
	if (BoundLoadoutComponent == LoadoutComponent)
	{
		return;
	}

	UnbindLoadoutComponent();
	BoundLoadoutComponent = LoadoutComponent;

	if (BoundLoadoutComponent)
	{
		BoundLoadoutComponent->OnLoadoutChanged.AddDynamic(this, &UFrontierEquipmentComponent::HandleLoadoutChanged);
	}
}

void UFrontierEquipmentComponent::UnbindLoadoutComponent()
{
	if (BoundLoadoutComponent)
	{
		BoundLoadoutComponent->OnLoadoutChanged.RemoveDynamic(this, &UFrontierEquipmentComponent::HandleLoadoutChanged);
		BoundLoadoutComponent = nullptr;
	}
}

AFrontierWeaponBase* UFrontierEquipmentComponent::SpawnWeaponFromLoadoutSlot(const FFrontierLoadoutSlot& LoadoutSlot) const
{
	AFrontierBaseCharacter* OwnerCharacter = GetOwnerCharacter();
	UWorld* World = GetWorld();
	if (!OwnerCharacter || !World || !LoadoutSlot.bOccupied || !LoadoutSlot.ItemInstance.IsValid())
	{
		return nullptr;
	}

	if (LoadoutSlot.ItemInstance.GetCategory() != EFrontierItemCategory::Weapon)
	{
		return nullptr;
	}

	UFrontierWeaponDataAsset* WeaponData = LoadoutSlot.ItemInstance.GetWeaponData().LoadSynchronous();
	if (!WeaponData)
	{
		FRONTIER_LOG(Warning, TEXT("Weapon spawn failed because WeaponData is missing. Owner=%s ItemInstanceId=%s ItemTemplateId=%s"),
			*GetNameSafe(GetOwner()),
			*LoadoutSlot.ItemInstance.ItemInstanceId.ToString(EGuidFormats::DigitsWithHyphensLower),
			*LoadoutSlot.ItemInstance.GetTemplateId().ToString());
		return nullptr;
	}

	TSubclassOf<AFrontierWeaponBase> WeaponClass = LoadoutSlot.ItemInstance.GetWeaponActorClass().LoadSynchronous();
	if (!WeaponClass)
	{
		WeaponClass = AFrontierWeaponBase::StaticClass();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = OwnerCharacter;
	SpawnParameters.Instigator = OwnerCharacter;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AFrontierWeaponBase* SpawnedWeapon = World->SpawnActor<AFrontierWeaponBase>(
		WeaponClass,
		OwnerCharacter->GetActorTransform(),
		SpawnParameters);

	if (SpawnedWeapon)
	{
		SpawnedWeapon->SetWeaponData(WeaponData);
		SpawnedWeapon->SetEnhancementLevel(LoadoutSlot.ItemInstance.EnhancementLevel);
	}

	return SpawnedWeapon;
}

bool UFrontierEquipmentComponent::HasUnarmedState() const
{
	return UnarmedWeaponData != nullptr;
}

int32 UFrontierEquipmentComponent::GetNextWeaponIndex() const
{
	if (CurrentWeaponIndex == INDEX_NONE)
	{
		return OwnedWeapons.Num() > 0 ? 0 : INDEX_NONE;
	}

	const int32 NextWeaponIndex = CurrentWeaponIndex + 1;
	if (OwnedWeapons.IsValidIndex(NextWeaponIndex))
	{
		return NextWeaponIndex;
	}

	return HasUnarmedState() ? INDEX_NONE : 0;
}
