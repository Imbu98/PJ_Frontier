#include "Components/FrontierEquipmentSkillComponent.h"

#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/FrontierGameplayAbility_AreaSkill.h"
#include "AbilitySystem/FrontierAttributeSet.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Components/FrontierLoadoutComponent.h"
#include "Frontier.h"
#include "Game/FrontierPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "Inventory/Items/FrontierWeaponItemDataAsset.h"
#include "Net/UnrealNetwork.h"
#include "Skill/FrontierSkillDataSubsystem.h"
#include "Skill/FrontierSkillDataAsset.h"
#include "Skill/FrontierWeaponSkillGenerationDataAsset.h"
#include "Tags/FrontierGameplayTags.h"
#include "Weapons/FrontierWeaponDataAsset.h"

UFrontierEquipmentSkillComponent::UFrontierEquipmentSkillComponent()
{
	

	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UFrontierEquipmentSkillComponent::BeginPlay()
{
	Super::BeginPlay();

	

	if (AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(GetOwner()))
	{
		if (UFrontierLoadoutComponent* LoadoutComponent = PlayerState->GetLoadoutComponent())
		{
			LoadoutComponent->OnLoadoutChanged.AddDynamic(this, &UFrontierEquipmentSkillComponent::HandleLoadoutChanged);
		}
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		RefreshFromLoadout();
	}
}

void UFrontierEquipmentSkillComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UFrontierEquipmentSkillComponent, GeneratedSkills, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UFrontierEquipmentSkillComponent, RuntimeGeneratedSkills, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UFrontierEquipmentSkillComponent, SkillLevels, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UFrontierEquipmentSkillComponent, SkillCooldowns, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UFrontierEquipmentSkillComponent, CurrentSkillDataAsset, COND_OwnerOnly);
}

void UFrontierEquipmentSkillComponent::RefreshFromLoadout()
{
	

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(GetOwner());
	const UFrontierLoadoutComponent* LoadoutComponent = PlayerState ? PlayerState->GetLoadoutComponent() : nullptr;
	if (!LoadoutComponent)
	{
		GeneratedSkills.Reset();
		RuntimeGeneratedSkills.Reset();
		SkillLevels.Reset();
		CurrentSkillDataAsset = nullptr;
		RuntimeSkillInfoCache.Reset();
		RebuildActiveSkillCache();
		SyncGrantedEquipmentAbilities();
		BroadcastSkillDataChanged();
		return;
	}

	RebuildSkillDataFromSlots(LoadoutComponent->GetLoadoutSlots());
}

void UFrontierEquipmentSkillComponent::RequestUseSkillSlot(const int32 SlotIndex)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ServerUseSkillSlot_Implementation(SlotIndex);
		return;
	}

	ServerUseSkillSlot(SlotIndex);
}

void UFrontierEquipmentSkillComponent::ServerUseSkillSlot_Implementation(const int32 SlotIndex)
{
	FGameplayTag SkillTag;
	const FFrontierSkillInfo* SkillInfo = nullptr;
	if (!CanUseSkillSlot(SlotIndex, SkillTag, SkillInfo))
	{
		return;
	}

	AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(GetOwner());
	UFrontierAbilitySystemComponent* AbilitySystemComponent = PlayerState ? PlayerState->GetFrontierAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent || !SkillInfo || !SkillInfo->AbilityClass)
	{
		FRONTIER_LOG(Warning, TEXT("Equipment skill activation failed because ASC or AbilityClass is missing. Skill=%s"), *SkillTag.ToString());
		return;
	}

	const int32 AbilityLevel = FMath::Max(1, GetSkillLevel(SkillTag));
	FGameplayAbilitySpec* GrantedSpec = FindGrantedEquipmentAbilitySpec(SkillInfo->AbilityClass);
	if (!GrantedSpec)
	{
		SyncGrantedEquipmentAbilities();
		GrantedSpec = FindGrantedEquipmentAbilitySpec(SkillInfo->AbilityClass);
	}

	if (!GrantedSpec)
	{
		FRONTIER_LOG(Warning, TEXT("Equipment skill activation failed because its owned AbilitySpec is unavailable. Skill=%s Ability=%s"),
			*SkillTag.ToString(),
			*GetNameSafe(SkillInfo->AbilityClass.Get()));
		return;
	}

	if (GrantedSpec->Level != AbilityLevel)
	{
		GrantedSpec->Level = AbilityLevel;
		AbilitySystemComponent->MarkAbilitySpecDirty(*GrantedSpec);
	}

	const UGameplayAbility* AbilityDefaultObject = SkillInfo->AbilityClass->GetDefaultObject<UGameplayAbility>();
	const bool bIsAreaSkill = AbilityDefaultObject && AbilityDefaultObject->IsA<UFrontierGameplayAbility_AreaSkill>();
	if (bIsAreaSkill)
	{
		PendingAreaSkillTag = SkillTag;
		PendingAreaSkillInfo = *SkillInfo;
		PendingAreaSkillAbilityClass = SkillInfo->AbilityClass;
		BroadcastSkillDataChanged();
	}

	const bool bActivated = AbilitySystemComponent->TryActivateAbility(GrantedSpec->Handle);
	if (!bActivated)
	{
		if (bIsAreaSkill)
		{
			ClearPendingAreaSkill(SkillInfo->AbilityClass);
		}
		return;
	}

	if (bIsAreaSkill)
	{
		return;
	}

	ConsumeStamina(*SkillInfo);

	if (SkillInfo->Cooldown > 0.0f)
	{
		SetSkillCooldown(SkillTag, SkillInfo->Cooldown);
	}
}

bool UFrontierEquipmentSkillComponent::CanCommitPendingAreaSkill(const TSubclassOf<UGameplayAbility> AbilityClass) const
{
	if (!AbilityClass || PendingAreaSkillAbilityClass != AbilityClass || !PendingAreaSkillTag.IsValid())
	{
		return true;
	}

	return HasEnoughStamina(PendingAreaSkillInfo) && !IsSkillOnCooldown(PendingAreaSkillTag);
}

bool UFrontierEquipmentSkillComponent::CommitPendingAreaSkill(const TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!CanCommitPendingAreaSkill(AbilityClass))
	{
		return false;
	}

	if (!AbilityClass || PendingAreaSkillAbilityClass != AbilityClass || !PendingAreaSkillTag.IsValid())
	{
		return true;
	}

	ConsumeStamina(PendingAreaSkillInfo);
	if (PendingAreaSkillInfo.Cooldown > 0.0f)
	{
		SetSkillCooldown(PendingAreaSkillTag, PendingAreaSkillInfo.Cooldown);
	}

	ClearPendingAreaSkill(AbilityClass);
	return true;
}

void UFrontierEquipmentSkillComponent::ClearPendingAreaSkill(const TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (AbilityClass && PendingAreaSkillAbilityClass != AbilityClass)
	{
		return;
	}

	PendingAreaSkillTag = FGameplayTag();
	PendingAreaSkillInfo = FFrontierSkillInfo();
	PendingAreaSkillAbilityClass = nullptr;
	BroadcastSkillDataChanged();
}

FGameplayTag UFrontierEquipmentSkillComponent::GetSkillTagAtSlot(const int32 SlotIndex) const
{
	return ActiveSkillTags.IsValidIndex(SlotIndex) ? ActiveSkillTags[SlotIndex] : FGameplayTag();
}

bool UFrontierEquipmentSkillComponent::GetGeneratedSkillAtSlot(const int32 SlotIndex, FFrontierGeneratedWeaponSkill& OutGeneratedSkill) const
{
	for (const FFrontierRuntimeSkillData& RuntimeSkill : RuntimeGeneratedSkills)
	{
		if (RuntimeSkill.SlotIndex == SlotIndex)
		{
			OutGeneratedSkill = FFrontierGeneratedWeaponSkill();
			OutGeneratedSkill.SkillTemplateId = RuntimeSkill.SkillTemplateId;
			OutGeneratedSkill.SkillTag = RuntimeSkill.SkillTag;
			OutGeneratedSkill.SkillLevel = RuntimeSkill.SkillLevel;
			OutGeneratedSkill.SlotIndex = RuntimeSkill.SlotIndex;
			return true;
		}
	}

	for (const FFrontierGeneratedWeaponSkill& GeneratedSkill : GeneratedSkills)
	{
		if (GeneratedSkill.SlotIndex == SlotIndex)
		{
			OutGeneratedSkill = GeneratedSkill;
			return true;
		}
	}

	return false;
}

bool UFrontierEquipmentSkillComponent::GetRuntimeSkillAtSlot(const int32 SlotIndex, FFrontierRuntimeSkillData& OutRuntimeSkill) const
{
	for (const FFrontierRuntimeSkillData& RuntimeSkill : RuntimeGeneratedSkills)
	{
		if (RuntimeSkill.SlotIndex == SlotIndex)
		{
			OutRuntimeSkill = RuntimeSkill;
			return true;
		}
	}

	FFrontierGeneratedWeaponSkill LegacySkill;
	if (GetGeneratedSkillAtSlot(SlotIndex, LegacySkill))
	{
		OutRuntimeSkill = FFrontierRuntimeSkillData();
		OutRuntimeSkill.SkillTemplateId = LegacySkill.SkillTemplateId;
		OutRuntimeSkill.SkillTag = LegacySkill.SkillTag;
		OutRuntimeSkill.SkillLevel = LegacySkill.SkillLevel;
		OutRuntimeSkill.SlotIndex = LegacySkill.SlotIndex;
		return true;
	}

	return false;
}

int32 UFrontierEquipmentSkillComponent::GetSkillLevel(const FGameplayTag SkillTag) const
{
	if (!SkillTag.IsValid())
	{
		return 0;
	}

	for (const FFrontierSkillLevelEntry& Entry : SkillLevels)
	{
		if (Entry.SkillTag == SkillTag)
		{
			return FMath::Max(1, Entry.Level);
		}
	}

	return 1;
}

float UFrontierEquipmentSkillComponent::GetCooldownRemaining(const FGameplayTag SkillTag) const
{
	if (!SkillTag.IsValid() || !GetWorld())
	{
		return 0.0f;
	}

	const FFrontierSkillCooldownEntry* Entry = FindCooldownEntry(SkillTag);
	return Entry ? FMath::Max(0.0f, Entry->CooldownEndTime - GetCooldownTimeSeconds()) : 0.0f;
}

float UFrontierEquipmentSkillComponent::GetCooldownDuration(const FGameplayTag SkillTag) const
{
	const FFrontierSkillCooldownEntry* Entry = FindCooldownEntry(SkillTag);
	return Entry ? Entry->CooldownDuration : 0.0f;
}

bool UFrontierEquipmentSkillComponent::IsSkillOnCooldown(const FGameplayTag SkillTag) const
{
	return GetCooldownRemaining(SkillTag) > 0.0f;
}

const FFrontierSkillInfo* UFrontierEquipmentSkillComponent::FindSkillInfo(const FGameplayTag SkillTag) const
{
	if (const FFrontierSkillInfo* CachedSkillInfo = RuntimeSkillInfoCache.Find(SkillTag))
	{
		return CachedSkillInfo;
	}

	if (const FFrontierSkillTableRow* SkillData = FindSkillTableRow(SkillTag))
	{
		ResolvedSkillInfoScratch = FFrontierSkillInfo();
		ResolvedSkillInfoScratch.SkillId = SkillData->SkillId;
		ResolvedSkillInfoScratch.SkillTag = SkillData->SkillTag;
		ResolvedSkillInfoScratch.DisplayName = SkillData->DisplayName;
		ResolvedSkillInfoScratch.Description = SkillData->Description;
		ResolvedSkillInfoScratch.DescriptionParameters = SkillData->DescriptionParameters;
		ResolvedSkillInfoScratch.Icon = SkillData->Icon.LoadSynchronous();
		ResolvedSkillInfoScratch.AbilityClass = SkillData->AbilityClass;
		ResolvedSkillInfoScratch.Cooldown = SkillData->Cooldown;
		ResolvedSkillInfoScratch.CooldownTag = SkillData->CooldownTag;
		ResolvedSkillInfoScratch.StaminaCost = SkillData->StaminaCost;
		ResolvedSkillInfoScratch.MaxLevel = SkillData->MaxLevel;
		return &ResolvedSkillInfoScratch;
	}

	return CurrentSkillDataAsset ? CurrentSkillDataAsset->FindSkillInfo(SkillTag) : nullptr;
}

const FFrontierSkillTableRow* UFrontierEquipmentSkillComponent::FindSkillTableRow(const FGameplayTag SkillTag) const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFrontierSkillDataSubsystem* SkillDataSubsystem = GameInstance ? GameInstance->GetSubsystem<UFrontierSkillDataSubsystem>() : nullptr;
	return SkillDataSubsystem ? SkillDataSubsystem->FindSkillData(SkillTag) : nullptr;
}

const FFrontierSkillTableRow* UFrontierEquipmentSkillComponent::FindSkillTableRowById(const FName SkillId) const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFrontierSkillDataSubsystem* SkillDataSubsystem = GameInstance ? GameInstance->GetSubsystem<UFrontierSkillDataSubsystem>() : nullptr;
	return SkillDataSubsystem ? SkillDataSubsystem->FindSkillDataById(SkillId) : nullptr;
}

FName UFrontierEquipmentSkillComponent::FindSkillIdByTag(const FGameplayTag SkillTag) const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFrontierSkillDataSubsystem* SkillDataSubsystem = GameInstance ? GameInstance->GetSubsystem<UFrontierSkillDataSubsystem>() : nullptr;
	return SkillDataSubsystem ? SkillDataSubsystem->FindSkillId(SkillTag) : NAME_None;
}

const FFrontierSkillTableRow* UFrontierEquipmentSkillComponent::ResolveSkillTableRow(const FFrontierRuntimeSkillData& RuntimeSkill) const
{
	if (!RuntimeSkill.SkillTemplateId.IsEmpty())
	{
		if (const FFrontierSkillTableRow* SkillData = FindSkillTableRowById(FName(*RuntimeSkill.SkillTemplateId)))
		{
			return SkillData;
		}
	}

	return RuntimeSkill.SkillTag.IsValid() ? FindSkillTableRow(RuntimeSkill.SkillTag) : nullptr;
}

void UFrontierEquipmentSkillComponent::OnRep_GeneratedSkills()
{

	RebuildActiveSkillCache();
	SyncGrantedEquipmentAbilities();
	BroadcastSkillDataChanged();
}

void UFrontierEquipmentSkillComponent::OnRep_SkillLevels()
{
	
	BroadcastSkillDataChanged();
}

void UFrontierEquipmentSkillComponent::OnRep_SkillCooldowns()
{
	
	BroadcastSkillDataChanged();
}

void UFrontierEquipmentSkillComponent::OnRep_CurrentSkillDataAsset()
{
	
	BroadcastSkillDataChanged();
}

void UFrontierEquipmentSkillComponent::HandleLoadoutChanged(const TArray<FFrontierLoadoutSlot>& Slots)
{
	

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		RebuildSkillDataFromSlots(Slots);
	}
}

bool UFrontierEquipmentSkillComponent::CanUseSkillSlot(
	const int32 SlotIndex,
	FGameplayTag& OutSkillTag,
	const FFrontierSkillInfo*& OutSkillInfo) const
{
	OutSkillTag = GetSkillTagAtSlot(SlotIndex);
	OutSkillInfo = FindSkillInfo(OutSkillTag);
	if (!OutSkillTag.IsValid() || !OutSkillInfo || !OutSkillInfo->AbilityClass)
	{
		return false;
	}

	if (IsSkillOnCooldown(OutSkillTag))
	{
		return false;
	}

	const AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(GetOwner());
	const UFrontierAbilitySystemComponent* AbilitySystemComponent = PlayerState ? PlayerState->GetFrontierAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		return false;
	}

	const AFrontierPlayerCharacter* PlayerCharacter = PlayerState ? Cast<AFrontierPlayerCharacter>(PlayerState->GetPawn()) : nullptr;
	const UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;
	if (MovementComponent && MovementComponent->IsFalling())
	{
		return false;
	}

	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	const bool bStateAllowsUse = !AbilitySystemComponent->HasMatchingGameplayTag(Tags.StateDead)
		&& !AbilitySystemComponent->HasMatchingGameplayTag(Tags.StateCCStun)
		&& !AbilitySystemComponent->HasMatchingGameplayTag(Tags.StateActionAttacking)
		&& !AbilitySystemComponent->HasMatchingGameplayTag(Tags.StateCombatSkill);

	return bStateAllowsUse && HasEnoughStamina(*OutSkillInfo);
}

bool UFrontierEquipmentSkillComponent::HasEnoughStamina(const FFrontierSkillInfo& SkillInfo) const
{
	

	if (SkillInfo.StaminaCost <= 0.0f)
	{
		return true;
	}

	const AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(GetOwner());
	const UFrontierAttributeSet* AttributeSet = PlayerState ? PlayerState->GetFrontierAttributeSet() : nullptr;
	const bool bHasEnoughStamina = AttributeSet && AttributeSet->GetStamina() >= SkillInfo.StaminaCost;

	return bHasEnoughStamina;
}

void UFrontierEquipmentSkillComponent::ConsumeStamina(const FFrontierSkillInfo& SkillInfo)
{
	

	if (SkillInfo.StaminaCost <= 0.0f)
	{
		return;
	}

	AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(GetOwner());
	UFrontierAttributeSet* AttributeSet = PlayerState ? const_cast<UFrontierAttributeSet*>(PlayerState->GetFrontierAttributeSet()) : nullptr;
	if (!AttributeSet)
	{
		FRONTIER_LOG(Warning, TEXT("Could not consume equipment skill stamina because AttributeSet is missing."));
		return;
	}

	const float OldStamina = AttributeSet->GetStamina();
	UFrontierAbilitySystemComponent* AbilitySystemComponent = PlayerState->GetFrontierAbilitySystemComponent();
	if (!AbilitySystemComponent || !AbilitySystemComponent->ApplyStaminaDelta(-SkillInfo.StaminaCost))
	{
		// Preserve the previous behavior if actor info is temporarily unavailable.
		AttributeSet->SetStamina(FMath::Max(0.0f, OldStamina - SkillInfo.StaminaCost));
	}
	if (const AController* Controller = Cast<AController>(PlayerState->GetOwner()))
	{
		if (AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(Controller->GetPawn()))
		{
			PlayerCharacter->NotifyStaminaConsumptionFinished();
		}
	}
}

void UFrontierEquipmentSkillComponent::RebuildSkillDataFromSlots(const TArray<FFrontierLoadoutSlot>& Slots)
{
	

	GeneratedSkills.Reset();
	RuntimeGeneratedSkills.Reset();
	SkillLevels.Reset();
	CurrentSkillDataAsset = nullptr;
	RuntimeSkillInfoCache.Reset();
	CurrentGrantedItemInstanceId.Reset();
	FString MainWeaponItemInstanceId;

	// The main weapon used to be the only source of equipment skills. Keep the
	// loadout order deterministic while also including other equipped items that
	// can own generated skills.
	const EFrontierEquipmentSlot SkillBearingSlots[] =
	{
		EFrontierEquipmentSlot::MainWeapon,
		EFrontierEquipmentSlot::SubWeapon,
		EFrontierEquipmentSlot::Helmet,
		EFrontierEquipmentSlot::Chest,
		EFrontierEquipmentSlot::Gloves,
		EFrontierEquipmentSlot::Boots,
		EFrontierEquipmentSlot::Necklace,
		EFrontierEquipmentSlot::Ring
	};

	for (const FFrontierLoadoutSlot& Slot : Slots)
	{
		if (!Slot.bOccupied || !Slot.ItemInstance.IsValid())
		{
			continue;
		}

		if (Slot.SlotType == EFrontierEquipmentSlot::MainWeapon)
		{
			MainWeaponItemInstanceId = Slot.ItemInstance.ItemInstanceId.ToString(EGuidFormats::DigitsWithHyphensLower);
			CurrentSkillDataAsset = ResolveSkillDataAssetForItem(Slot.ItemInstance);
		}
	}

	TMap<FGameplayTag, int32> WeaponSkillIndices;
	TMap<FGameplayTag, int32> TotalSkillLevels;
	TArray<FGameplayTag> SkillLevelOrder;
	for (const EFrontierEquipmentSlot SkillBearingSlot : SkillBearingSlots)
	{
		for (const FFrontierLoadoutSlot& Slot : Slots)
		{
			if (Slot.SlotType != SkillBearingSlot || !Slot.bOccupied || !Slot.ItemInstance.IsValid())
			{
				continue;
			}

			const FString ItemInstanceId = Slot.ItemInstance.ItemInstanceId.ToString(EGuidFormats::DigitsWithHyphensLower);
			TSet<int32> SeenItemSkillSlots;
			for (const FFrontierRuntimeSkillData& SourceSkill : Slot.ItemInstance.RuntimeGeneratedSkills)
			{
				FFrontierRuntimeSkillData RuntimeSkill = SourceSkill;
				const FFrontierSkillTableRow* SkillData = ResolveSkillTableRow(RuntimeSkill);
				if (!RuntimeSkill.SkillTag.IsValid() && SkillData)
				{
					RuntimeSkill.SkillTag = SkillData->SkillTag;
				}
				if (!RuntimeSkill.SkillTag.IsValid())
				{
					FRONTIER_LOG(Error, TEXT("Generated skill rejected because SkillId lookup failed or SkillTag is invalid. Owner=%s ItemInstanceId=%s SkillId=%s SlotIndex=%d"),
						*GetNameSafe(GetOwner()),
						*ItemInstanceId,
						*RuntimeSkill.SkillTemplateId,
						RuntimeSkill.SlotIndex);
					continue;
				}
				if (SeenItemSkillSlots.Contains(RuntimeSkill.SlotIndex))
				{
					FRONTIER_LOG(Error, TEXT("Generated skill has duplicate SlotIndex in equipped item. Owner=%s ItemInstanceId=%s SkillTag=%s SlotIndex=%d"),
						*GetNameSafe(GetOwner()),
						*ItemInstanceId,
						*RuntimeSkill.SkillTag.ToString(),
						RuntimeSkill.SlotIndex);
					continue;
				}
				SeenItemSkillSlots.Add(RuntimeSkill.SlotIndex);

				if (RuntimeSkill.SkillLevel < 1)
				{
					FRONTIER_LOG(Error, TEXT("Generated skill rejected by SkillLevel range. Owner=%s ItemInstanceId=%s SkillTag=%s SkillLevel=%d"),
						*GetNameSafe(GetOwner()),
						*ItemInstanceId,
						*RuntimeSkill.SkillTag.ToString(),
						RuntimeSkill.SkillLevel);
					RuntimeSkill.SkillLevel = 1;
				}

				if (SkillData)
				{
					RuntimeSkill.SkillLevel = FMath::Clamp(RuntimeSkill.SkillLevel, 1, SkillData->MaxLevel);

					FFrontierSkillInfo& CachedSkillInfo = RuntimeSkillInfoCache.FindOrAdd(RuntimeSkill.SkillTag);
					CachedSkillInfo.SkillId = RuntimeSkill.SkillTemplateId.IsEmpty()
						? FindSkillIdByTag(RuntimeSkill.SkillTag)
						: FName(*RuntimeSkill.SkillTemplateId);
					CachedSkillInfo.SkillTag = SkillData->SkillTag;
					CachedSkillInfo.DisplayName = SkillData->DisplayName;
					CachedSkillInfo.Description = SkillData->Description;
					CachedSkillInfo.DescriptionParameters = SkillData->DescriptionParameters;
					CachedSkillInfo.Icon = SkillData->Icon.LoadSynchronous();
					CachedSkillInfo.AbilityClass = SkillData->AbilityClass;
					CachedSkillInfo.Cooldown = SkillData->Cooldown;
					CachedSkillInfo.CooldownTag = SkillData->CooldownTag;
					CachedSkillInfo.StaminaCost = SkillData->StaminaCost;
					CachedSkillInfo.MaxLevel = SkillData->MaxLevel;
				}
				else if (const FFrontierSkillInfo* SkillInfo = CurrentSkillDataAsset ? CurrentSkillDataAsset->FindSkillInfo(RuntimeSkill.SkillTag) : nullptr)
				{
					RuntimeSkill.SkillLevel = FMath::Clamp(RuntimeSkill.SkillLevel, 1, SkillInfo->MaxLevel);
				}
				else
				{
					FRONTIER_LOG(Error, TEXT("Skill DataTable lookup failed. Owner=%s ItemInstanceId=%s SkillId=%s SkillTag=%s"),
						*GetNameSafe(GetOwner()),
						*ItemInstanceId,
						*RuntimeSkill.SkillTemplateId,
						*RuntimeSkill.SkillTag.ToString());
				}

				const FGameplayTag SkillTag = RuntimeSkill.SkillTag;
				if (!TotalSkillLevels.Contains(SkillTag))
				{
					SkillLevelOrder.Add(SkillTag);
				}
				TotalSkillLevels.FindOrAdd(SkillTag) += FMath::Max(1, RuntimeSkill.SkillLevel);

				// Only weapon skills are loadout skill slots. Other equipment still
				// contributes to the total level used by the weapon skill.
				if (Slot.ItemInstance.GetCategory() != EFrontierItemCategory::Weapon)
				{
					continue;
				}

				if (int32* ExistingIndex = WeaponSkillIndices.Find(SkillTag))
				{
					FFrontierRuntimeSkillData& ExistingSkill = RuntimeGeneratedSkills[*ExistingIndex];
					if (RuntimeSkill.SkillLevel > ExistingSkill.SkillLevel)
					{
						const int32 AggregatedSlotIndex = ExistingSkill.SlotIndex;
						ExistingSkill = RuntimeSkill;
						ExistingSkill.SlotIndex = AggregatedSlotIndex;
						GeneratedSkills[*ExistingIndex].SkillTemplateId = RuntimeSkill.SkillTemplateId;
						GeneratedSkills[*ExistingIndex].SkillTag = RuntimeSkill.SkillTag;
						GeneratedSkills[*ExistingIndex].SkillLevel = RuntimeSkill.SkillLevel;
						GeneratedSkills[*ExistingIndex].SlotIndex = AggregatedSlotIndex;
					}
					continue;
				}

				RuntimeSkill.SlotIndex = RuntimeGeneratedSkills.Num();
				const int32 AggregatedIndex = RuntimeGeneratedSkills.Add(RuntimeSkill);
				WeaponSkillIndices.Add(SkillTag, AggregatedIndex);

				FFrontierGeneratedWeaponSkill& LegacySkill = GeneratedSkills.AddDefaulted_GetRef();
				LegacySkill.SkillTemplateId = RuntimeSkill.SkillTemplateId;
				LegacySkill.SkillTag = RuntimeSkill.SkillTag;
				LegacySkill.SkillLevel = RuntimeSkill.SkillLevel;
				LegacySkill.SlotIndex = RuntimeSkill.SlotIndex;
			}
	}
	}

	for (const FGameplayTag& SkillTag : SkillLevelOrder)
	{
		FFrontierSkillLevelEntry& SkillLevelEntry = SkillLevels.AddDefaulted_GetRef();
		SkillLevelEntry.SkillTag = SkillTag;
		SkillLevelEntry.Level = TotalSkillLevels.FindRef(SkillTag);
	}

	CurrentGrantedItemInstanceId = MainWeaponItemInstanceId;

	RebuildActiveSkillCache();
	SyncGrantedEquipmentAbilities();
	BroadcastSkillDataChanged();
}

UFrontierSkillDataAsset* UFrontierEquipmentSkillComponent::ResolveSkillDataAssetForItem(const FFrontierItemInstance& ItemInstance) const
{
	if (ItemInstance.GetCategory() != EFrontierItemCategory::Weapon)
	{
		return nullptr;
	}

	const UFrontierWeaponDataAsset* WeaponData = ItemInstance.GetWeaponData().LoadSynchronous();
	const UFrontierWeaponSkillGenerationDataAsset* SkillGenerationData = ItemInstance.GetSkillGenerationData().LoadSynchronous();
	if (!WeaponData || !SkillGenerationData)
	{
		return nullptr;
	}

	return SkillGenerationData->FindSkillDataAssetForWeaponType(WeaponData->WeaponTypeTag);
}

void UFrontierEquipmentSkillComponent::GrantItemSkillsFromServerData(const FFrontierItemInstance& EquippedItem)
{
	const FString ItemInstanceId = EquippedItem.ItemInstanceId.ToString(EGuidFormats::DigitsWithHyphensLower);
	const FString ItemTemplateId = EquippedItem.GetTemplateId().ToString();
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		FRONTIER_LOG(Error, TEXT("Client attempted to call server-only GrantItemSkillsFromServerData. Owner=%s ItemInstanceId=%s ItemTemplateId=%s"),
			*GetNameSafe(GetOwner()),
			*ItemInstanceId,
			*ItemTemplateId);
		return;
	}

	// Loadout is the source of truth when the item is already equipped. Rebuild
	// all skill-bearing equipment so a single-item update cannot discard ring or
	// necklace skills.
	if (const AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(GetOwner()))
	{
		if (const UFrontierLoadoutComponent* LoadoutComponent = PlayerState->GetLoadoutComponent())
		{
			for (const FFrontierLoadoutSlot& Slot : LoadoutComponent->GetLoadoutSlots())
			{
				if (Slot.bOccupied
					&& Slot.ItemInstance.IsValid()
					&& Slot.ItemInstance.ItemInstanceId == EquippedItem.ItemInstanceId)
				{
					RebuildSkillDataFromSlots(LoadoutComponent->GetLoadoutSlots());
					return;
				}
			}
		}
	}

	const bool bIsWeapon = EquippedItem.GetCategory() == EFrontierItemCategory::Weapon;
	RuntimeGeneratedSkills.Reset();
	GeneratedSkills.Reset();
	SkillLevels.Reset();
	RuntimeSkillInfoCache.Reset();
	CurrentGrantedItemInstanceId = bIsWeapon ? ItemInstanceId : FString();
	TMap<FGameplayTag, int32> TotalSkillLevels;
	TArray<FGameplayTag> SkillLevelOrder;

	for (const FFrontierRuntimeSkillData& SourceSkill : EquippedItem.RuntimeGeneratedSkills)
	{
		FFrontierRuntimeSkillData RuntimeSkill = SourceSkill;
		const FFrontierSkillTableRow* SkillData = ResolveSkillTableRow(RuntimeSkill);
		if (!RuntimeSkill.SkillTag.IsValid() && SkillData)
		{
			RuntimeSkill.SkillTag = SkillData->SkillTag;
		}

		if (!RuntimeSkill.SkillTag.IsValid())
		{
			FRONTIER_LOG(Error, TEXT("Generated skill rejected because SkillId lookup failed or SkillTag is invalid. ItemInstanceId=%s ItemTemplateId=%s SkillId=%s SlotIndex=%d"),
				*ItemInstanceId,
				*ItemTemplateId,
				*RuntimeSkill.SkillTemplateId,
				RuntimeSkill.SlotIndex);
			continue;
		}

		if (SkillData)
		{
			RuntimeSkill.SkillLevel = FMath::Max(1, RuntimeSkill.SkillLevel);
			if (!SkillData->AbilityClass)
			{
				FRONTIER_LOG(Error, TEXT("AbilityClass missing for generated skill. ItemInstanceId=%s SkillTemplateId=%s SkillTag=%s"),
					*ItemInstanceId,
					*RuntimeSkill.SkillTemplateId,
					*RuntimeSkill.SkillTag.ToString());
			}
			if (RuntimeSkill.SkillLevel > SkillData->MaxLevel)
			{
				FRONTIER_LOG(Error, TEXT("SkillLevel exceeds MaxLevel. ItemInstanceId=%s SkillTemplateId=%s SkillTag=%s SkillLevel=%d MaxLevel=%d"),
					*ItemInstanceId,
					*RuntimeSkill.SkillTemplateId,
					*RuntimeSkill.SkillTag.ToString(),
				RuntimeSkill.SkillLevel,
				SkillData->MaxLevel);
			}
			RuntimeSkill.SkillLevel = FMath::Clamp(RuntimeSkill.SkillLevel, 1, SkillData->MaxLevel);

			FFrontierSkillInfo& CachedSkillInfo = RuntimeSkillInfoCache.FindOrAdd(RuntimeSkill.SkillTag);
			CachedSkillInfo.SkillId = RuntimeSkill.SkillTemplateId.IsEmpty()
				? FindSkillIdByTag(RuntimeSkill.SkillTag)
				: FName(*RuntimeSkill.SkillTemplateId);
			CachedSkillInfo.SkillTag = SkillData->SkillTag;
			CachedSkillInfo.DisplayName = SkillData->DisplayName;
			CachedSkillInfo.Description = SkillData->Description;
			CachedSkillInfo.DescriptionParameters = SkillData->DescriptionParameters;
			CachedSkillInfo.Icon = SkillData->Icon.LoadSynchronous();
			CachedSkillInfo.AbilityClass = SkillData->AbilityClass;
			CachedSkillInfo.Cooldown = SkillData->Cooldown;
			CachedSkillInfo.CooldownTag = SkillData->CooldownTag;
			CachedSkillInfo.StaminaCost = SkillData->StaminaCost;
			CachedSkillInfo.MaxLevel = SkillData->MaxLevel;
		}
		else
		{
			FRONTIER_LOG(Error, TEXT("Skill DataTable lookup failed. ItemInstanceId=%s SkillId=%s SkillTag=%s"),
				*ItemInstanceId,
				*RuntimeSkill.SkillTemplateId,
				*RuntimeSkill.SkillTag.ToString());
		}

		if (!TotalSkillLevels.Contains(RuntimeSkill.SkillTag))
		{
			SkillLevelOrder.Add(RuntimeSkill.SkillTag);
		}
		TotalSkillLevels.FindOrAdd(RuntimeSkill.SkillTag) += FMath::Max(1, RuntimeSkill.SkillLevel);

		if (bIsWeapon)
		{
			RuntimeSkill.SlotIndex = RuntimeGeneratedSkills.Num();
			RuntimeGeneratedSkills.Add(RuntimeSkill);

			FFrontierGeneratedWeaponSkill& LegacySkill = GeneratedSkills.AddDefaulted_GetRef();
			LegacySkill.SkillTemplateId = RuntimeSkill.SkillTemplateId;
			LegacySkill.SkillTag = RuntimeSkill.SkillTag;
			LegacySkill.SkillLevel = RuntimeSkill.SkillLevel;
			LegacySkill.SlotIndex = RuntimeSkill.SlotIndex;
		}
	}

	for (const FGameplayTag& SkillTag : SkillLevelOrder)
	{
		FFrontierSkillLevelEntry& LevelEntry = SkillLevels.AddDefaulted_GetRef();
		LevelEntry.SkillTag = SkillTag;
		LevelEntry.Level = TotalSkillLevels.FindRef(SkillTag);
	}

	RebuildActiveSkillCache();
	SyncGrantedEquipmentAbilities();
	BroadcastSkillDataChanged();
}

void UFrontierEquipmentSkillComponent::SyncGrantedEquipmentAbilities()
{
	FRONTIER_LOG_FUNC();

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(GetOwner());
	UFrontierAbilitySystemComponent* AbilitySystemComponent = PlayerState ? PlayerState->GetFrontierAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		return;
	}

	GrantedEquipmentAbilityHandlesByItemInstanceId.Reset();

	TMap<TSubclassOf<UGameplayAbility>, int32> DesiredAbilityLevels;
	for (const FGameplayTag& SkillTag : ActiveSkillTags)
	{
		if (!SkillTag.IsValid())
		{
			continue;
		}

		const FFrontierSkillInfo* SkillInfo = FindSkillInfo(SkillTag);
		if (!SkillInfo || !SkillInfo->AbilityClass)
		{
			continue;
		}

		const int32 DesiredLevel = FMath::Max(1, GetSkillLevel(SkillTag));
		int32& AbilityLevel = DesiredAbilityLevels.FindOrAdd(SkillInfo->AbilityClass);
		AbilityLevel = FMath::Max(AbilityLevel, DesiredLevel);
	}

	for (const TPair<TSubclassOf<UGameplayAbility>, int32>& Pair : DesiredAbilityLevels)
	{
		FGameplayAbilitySpec* GrantedSpec = FindGrantedEquipmentAbilitySpec(Pair.Key);
		if (!GrantedSpec)
		{
			GrantedEquipmentAbilityHandles.Remove(Pair.Key);
			const FGameplayAbilitySpecHandle GrantedHandle = AbilitySystemComponent->GiveAbility(
				FGameplayAbilitySpec(Pair.Key, Pair.Value, INDEX_NONE, this));
			if (GrantedHandle.IsValid())
			{
				GrantedEquipmentAbilityHandles.Add(Pair.Key, GrantedHandle);
				if (!CurrentGrantedItemInstanceId.IsEmpty())
				{
					GrantedEquipmentAbilityHandlesByItemInstanceId.FindOrAdd(CurrentGrantedItemInstanceId).Add(GrantedHandle);
				}
				FRONTIER_LOG(Log, TEXT("Granted equipment-owned ability. Ability=%s Level=%d Handle=%s"),
					*GetNameSafe(Pair.Key.Get()),
					Pair.Value,
					*GrantedHandle.ToString());
			}
			continue;
		}

		if (const FGameplayAbilitySpecHandle* GrantedHandle = GrantedEquipmentAbilityHandles.Find(Pair.Key);
			GrantedHandle && GrantedHandle->IsValid() && !CurrentGrantedItemInstanceId.IsEmpty())
		{
			GrantedEquipmentAbilityHandlesByItemInstanceId.FindOrAdd(CurrentGrantedItemInstanceId).AddUnique(*GrantedHandle);
		}

		if (GrantedSpec->Level != Pair.Value)
		{
			FRONTIER_LOG(Log, TEXT("Updating equipment-owned ability level. Ability=%s OldLevel=%d NewLevel=%d"),
				*GetNameSafe(Pair.Key.Get()),
				GrantedSpec->Level,
				Pair.Value);
			GrantedSpec->Level = Pair.Value;
			AbilitySystemComponent->MarkAbilitySpecDirty(*GrantedSpec);
		}
	}

	TSet<TSubclassOf<UGameplayAbility>> CurrentAbilityClasses;
	for (const TPair<TSubclassOf<UGameplayAbility>, int32>& Pair : DesiredAbilityLevels)
	{
		CurrentAbilityClasses.Add(Pair.Key);
	}
	RemoveGrantedEquipmentAbilitiesNotIn(CurrentAbilityClasses);
}

void UFrontierEquipmentSkillComponent::RemoveGrantedEquipmentAbilitiesNotIn(const TSet<TSubclassOf<UGameplayAbility>>& CurrentAbilityClasses)
{
	TArray<TSubclassOf<UGameplayAbility>> AbilityClassesToRemove;
	for (const TPair<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle>& Pair : GrantedEquipmentAbilityHandles)
	{
		if (!CurrentAbilityClasses.Contains(Pair.Key))
		{
			AbilityClassesToRemove.Add(Pair.Key);
		}
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : AbilityClassesToRemove)
	{
		RemoveGrantedEquipmentAbility(AbilityClass);
	}
}

void UFrontierEquipmentSkillComponent::RemoveGrantedEquipmentAbility(const TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilityClass)
	{
		return;
	}

	const FGameplayAbilitySpecHandle* GrantedHandle = GrantedEquipmentAbilityHandles.Find(AbilityClass);
	if (!GrantedHandle || !GrantedHandle->IsValid())
	{
		GrantedEquipmentAbilityHandles.Remove(AbilityClass);
		return;
	}

	const FGameplayAbilitySpecHandle HandleToRemove = *GrantedHandle;
	AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(GetOwner());
	UFrontierAbilitySystemComponent* AbilitySystemComponent = PlayerState ? PlayerState->GetFrontierAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		GrantedEquipmentAbilityHandles.Remove(AbilityClass);
		return;
	}

	FGameplayAbilitySpec* GrantedSpec = AbilitySystemComponent->FindAbilitySpecFromHandle(*GrantedHandle);
	if (!GrantedSpec)
	{
		GrantedEquipmentAbilityHandles.Remove(AbilityClass);
		return;
	}

	const bool bRemoveAfterActiveAbilityEnds = GrantedSpec->IsActive();
	FRONTIER_LOG(Log, TEXT("Removing equipment-owned ability. Ability=%s Handle=%s Deferred=%d"),
		*GetNameSafe(AbilityClass.Get()),
		*GrantedHandle->ToString(),
		bRemoveAfterActiveAbilityEnds ? 1 : 0);

	if (bRemoveAfterActiveAbilityEnds)
	{
		// Let an in-flight skill finish; GAS removes the spec from the ASC when activation ends.
		AbilitySystemComponent->SetRemoveAbilityOnEnd(*GrantedHandle);
	}
	else
	{
		if (PendingAreaSkillAbilityClass == AbilityClass)
		{
			ClearPendingAreaSkill(AbilityClass);
		}
		AbilitySystemComponent->ClearAbility(*GrantedHandle);
	}
	GrantedEquipmentAbilityHandles.Remove(AbilityClass);
	for (auto It = GrantedEquipmentAbilityHandlesByItemInstanceId.CreateIterator(); It; ++It)
	{
		It.Value().Remove(HandleToRemove);
		if (It.Value().IsEmpty())
		{
			It.RemoveCurrent();
		}
	}
}

FGameplayAbilitySpec* UFrontierEquipmentSkillComponent::FindGrantedEquipmentAbilitySpec(const TSubclassOf<UGameplayAbility> AbilityClass) const
{
	if (!AbilityClass)
	{
		return nullptr;
	}

	const FGameplayAbilitySpecHandle* GrantedHandle = GrantedEquipmentAbilityHandles.Find(AbilityClass);
	if (!GrantedHandle || !GrantedHandle->IsValid())
	{
		return nullptr;
	}

	const AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(GetOwner());
	UFrontierAbilitySystemComponent* AbilitySystemComponent = PlayerState ? PlayerState->GetFrontierAbilitySystemComponent() : nullptr;
	return AbilitySystemComponent ? AbilitySystemComponent->FindAbilitySpecFromHandle(*GrantedHandle) : nullptr;
}

void UFrontierEquipmentSkillComponent::SetSkillCooldown(const FGameplayTag SkillTag, const float CooldownDuration)
{
	if (!SkillTag.IsValid() || !GetWorld())
	{
		return;
	}

	FFrontierSkillCooldownEntry* Entry = FindMutableCooldownEntry(SkillTag);
	if (!Entry)
	{
		Entry = &SkillCooldowns.AddDefaulted_GetRef();
		Entry->SkillTag = SkillTag;
	}

	Entry->CooldownDuration = CooldownDuration;
	Entry->CooldownEndTime = GetCooldownTimeSeconds() + CooldownDuration;
	BroadcastSkillDataChanged();

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

void UFrontierEquipmentSkillComponent::RebuildActiveSkillCache()
{
	

	ActiveSkillTags.Reset();
	if (!RuntimeGeneratedSkills.IsEmpty())
	{
		TArray<FFrontierRuntimeSkillData> SortedSkills = RuntimeGeneratedSkills;
		SortedSkills.Sort([](const FFrontierRuntimeSkillData& Left, const FFrontierRuntimeSkillData& Right)
		{
			return Left.SlotIndex < Right.SlotIndex;
		});

		for (const FFrontierRuntimeSkillData& RuntimeSkill : SortedSkills)
		{
			if (ActiveSkillTags.Num() >= MaxSkillSlots)
			{
				break;
			}

			ActiveSkillTags.Add(RuntimeSkill.SkillTag);
		}

		while (ActiveSkillTags.Num() < MaxSkillSlots)
		{
			ActiveSkillTags.Add(FGameplayTag());
		}
		return;
	}

	TArray<FFrontierGeneratedWeaponSkill> SortedSkills = GeneratedSkills;
	SortedSkills.Sort([](const FFrontierGeneratedWeaponSkill& Left, const FFrontierGeneratedWeaponSkill& Right)
	{
		return Left.SlotIndex < Right.SlotIndex;
	});

	for (const FFrontierGeneratedWeaponSkill& GeneratedSkill : SortedSkills)
	{
		if (ActiveSkillTags.Num() >= MaxSkillSlots)
		{
			break;
		}

		ActiveSkillTags.Add(GeneratedSkill.SkillTag);
	}

	while (ActiveSkillTags.Num() < MaxSkillSlots)
	{
		ActiveSkillTags.Add(FGameplayTag());
	}
}

FFrontierSkillCooldownEntry* UFrontierEquipmentSkillComponent::FindMutableCooldownEntry(const FGameplayTag SkillTag)
{
	for (FFrontierSkillCooldownEntry& Entry : SkillCooldowns)
	{
		if (Entry.SkillTag == SkillTag)
		{
			return &Entry;
		}
	}

	return nullptr;
}

const FFrontierSkillCooldownEntry* UFrontierEquipmentSkillComponent::FindCooldownEntry(const FGameplayTag SkillTag) const
{
	for (const FFrontierSkillCooldownEntry& Entry : SkillCooldowns)
	{
		if (Entry.SkillTag == SkillTag)
		{
			return &Entry;
		}
	}

	return nullptr;
}

float UFrontierEquipmentSkillComponent::GetCooldownTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	const AGameStateBase* GameState = World->GetGameState();
	if (GameState)
	{
		return GameState->GetServerWorldTimeSeconds();
	}

	return World->GetTimeSeconds();
}

void UFrontierEquipmentSkillComponent::BroadcastSkillDataChanged()
{
	OnEquipmentSkillsChanged.Broadcast();
}
