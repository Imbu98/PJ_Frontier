#include "Inventory/FrontierInventoryTypes.h"

#include "Frontier.h"
#include "Engine/DataTable.h"
#include "Inventory/Items/FrontierArmorItemDataAsset.h"
#include "Inventory/Items/FrontierConsumableItemDataAsset.h"
#include "Inventory/Items/FrontierItemCatalogSubsystem.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "Inventory/Items/FrontierWeaponItemDataAsset.h"
#include "Skill/FrontierWeaponSkillGenerationDataAsset.h"
#include "Tags/FrontierGameplayTags.h"
#include "Weapons/FrontierWeaponDataAsset.h"

namespace
{
FString NormalizeBackendEnumString(const FString& Value)
{
	FString Normalized = Value;
	Normalized.TrimStartAndEndInline();
	Normalized.ReplaceInline(TEXT("-"), TEXT("_"));
	Normalized.ReplaceInline(TEXT(" "), TEXT("_"));
	return Normalized.ToUpper();
}

}

bool IsValidElementForCategory(const EFrontierItemCategory Category, const EFrontierElementalType ElementalType)
{
	if (Category == EFrontierItemCategory::Weapon)
	{
		return ElementalType != EFrontierElementalType::None;
	}
	if (Category == EFrontierItemCategory::Armor)
	{
		return ElementalType == EFrontierElementalType::None
			|| ElementalType == EFrontierElementalType::Normal;
	}
	if (Category == EFrontierItemCategory::Accessory)
	{
		return ElementalType == EFrontierElementalType::None
			|| ElementalType == EFrontierElementalType::Normal;
	}

	return ElementalType == EFrontierElementalType::None;
}

bool TryParseItemCategory(const FString& Value, EFrontierItemCategory& OutCategory)
{
	const FString Normalized = NormalizeBackendEnumString(Value);
	if (Normalized == TEXT("WEAPON"))
	{
		OutCategory = EFrontierItemCategory::Weapon;
		return true;
	}
	if (Normalized == TEXT("ARMOR"))
	{
		OutCategory = EFrontierItemCategory::Armor;
		return true;
	}
	if (Normalized == TEXT("CONSUMABLE"))
	{
		OutCategory = EFrontierItemCategory::Consumable;
		return true;
	}
	if (Normalized == TEXT("MATERIAL"))
	{
		OutCategory = EFrontierItemCategory::Material;
		return true;
	}
	if (Normalized == TEXT("ACCESSORY"))
	{
		OutCategory = EFrontierItemCategory::Accessory;
		return true;
	}

	FRONTIER_LOG(Error, TEXT("Unknown item category string from backend. Value=%s"), *Value);
	return false;
}

bool TryParseElementalType(const FString& Value, EFrontierElementalType& OutElementalType)
{
	const FString Normalized = NormalizeBackendEnumString(Value);
	if (Normalized == TEXT("NONE"))
	{
		OutElementalType = EFrontierElementalType::None;
		return true;
	}
	if (Normalized == TEXT("NORMAL"))
	{
		OutElementalType = EFrontierElementalType::Normal;
		return true;
	}
	if (Normalized == TEXT("FIRE"))
	{
		OutElementalType = EFrontierElementalType::Fire;
		return true;
	}
	if (Normalized == TEXT("ICE"))
	{
		OutElementalType = EFrontierElementalType::Ice;
		return true;
	}
	if (Normalized == TEXT("LIGHTNING"))
	{
		OutElementalType = EFrontierElementalType::Lightning;
		return true;
	}
	if (Normalized == TEXT("POISON"))
	{
		OutElementalType = EFrontierElementalType::Poison;
		return true;
	}

	FRONTIER_LOG(Error, TEXT("Unknown elemental type string from backend. Value=%s"), *Value);
	return false;
}

bool TryParseItemRarity(const FString& Value, EFrontierItemRarity& OutRarity)
{
	const FString Normalized = NormalizeBackendEnumString(Value);
	if (Normalized == TEXT("COMMON") || Value.Contains(TEXT("Common"), ESearchCase::IgnoreCase))
	{
		OutRarity = EFrontierItemRarity::Common;
		return true;
	}
	if (Normalized == TEXT("RARE") || Value.Contains(TEXT("Rare"), ESearchCase::IgnoreCase))
	{
		OutRarity = EFrontierItemRarity::Rare;
		return true;
	}
	if (Normalized == TEXT("EPIC") || Value.Contains(TEXT("Epic"), ESearchCase::IgnoreCase))
	{
		OutRarity = EFrontierItemRarity::Epic;
		return true;
	}
	if (Normalized == TEXT("LEGENDARY") || Value.Contains(TEXT("Legendary"), ESearchCase::IgnoreCase))
	{
		OutRarity = EFrontierItemRarity::Legendary;
		return true;
	}

	FRONTIER_LOG(Error, TEXT("Unknown item rarity string from backend. Value=%s"), *Value);
	return false;
}

FGameplayTag ConvertItemRarityToGameplayTag(const EFrontierItemRarity Rarity)
{
	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	switch (Rarity)
	{
	case EFrontierItemRarity::Rare:
		return Tags.ItemRarityRare;
	case EFrontierItemRarity::Epic:
		return Tags.ItemRarityEpic;
	case EFrontierItemRarity::Legendary:
		return Tags.ItemRarityLegendary;
	case EFrontierItemRarity::Common:
	default:
		return Tags.ItemRarityCommon;
	}
}

FString ConvertItemRarityToBackendString(const EFrontierItemRarity Rarity)
{
	switch (Rarity)
	{
	case EFrontierItemRarity::Rare:
		return TEXT("RARE");
	case EFrontierItemRarity::Epic:
		return TEXT("EPIC");
	case EFrontierItemRarity::Legendary:
		return TEXT("LEGENDARY");
	case EFrontierItemRarity::Common:
	default:
		return TEXT("COMMON");
	}
}

EFrontierItemCategory ConvertLegacyItemTypeToCategory(const EFrontierItemType ItemType)
{
	switch (ItemType)
	{
	case EFrontierItemType::Weapon:
		return EFrontierItemCategory::Weapon;
	case EFrontierItemType::Armor:
		return EFrontierItemCategory::Armor;
	case EFrontierItemType::Consumable:
		return EFrontierItemCategory::Consumable;
	case EFrontierItemType::Accessory:
		return EFrontierItemCategory::Accessory;
	case EFrontierItemType::Material:
	default:
		return EFrontierItemCategory::Material;
	}
}

FFrontierItemInstance FFrontierItemInstance::CreateFromItemData(const UFrontierItemDataAsset* InItemData, const int32 InQuantity)
{
	FFrontierItemInstance NewInstance;
	if (!InItemData || InQuantity <= 0)
	{
		return NewInstance;
	}

	NewInstance.ItemTemplateData = UFrontierItemCatalogSubsystem::BuildTemplateDataFromItemData(*InItemData);
	NewInstance.Quantity = InQuantity;
	NewInstance.FinalRarity = EFrontierItemRarity::Common;
	NewInstance.Durability = static_cast<float>(InItemData->MaxDurability);
	NewInstance.EnsureRuntimeIdentity();
	FRONTIER_LOG(Log, TEXT("Created local template-only item instance. Backend must provide generated stats and skills before raid gameplay. ItemTemplateId=%s"),
		*InItemData->GetTemplateId().ToString());
	return NewInstance;
}

FName FFrontierItemInstance::GetTemplateId() const
{
	return ItemTemplateData.ItemTemplateId;
}

bool FFrontierItemInstance::HasItemTemplateData() const
{
	return !ItemTemplateData.ItemTemplateId.IsNone();
}

const FFrontierItemTemplateData* FFrontierItemInstance::GetItemTemplateData() const
{
	return HasItemTemplateData() ? &ItemTemplateData.Common : nullptr;
}

FText FFrontierItemInstance::GetDisplayNameText() const
{
	if (HasItemTemplateData())
	{
		if (!ItemTemplateData.Common.DisplayName.IsEmpty())
		{
			return ItemTemplateData.Common.DisplayName;
		}
	}

	return FText::FromName(GetTemplateId());
}

FText FFrontierItemInstance::GetDescriptionText() const
{
	if (HasItemTemplateData())
	{
		if (!ItemTemplateData.Common.Description.IsEmpty())
		{
			return ItemTemplateData.Common.Description;
		}
	}

	return FText::GetEmpty();
}

TSoftObjectPtr<UTexture2D> FFrontierItemInstance::GetIcon() const
{
	if (HasItemTemplateData())
	{
		if (!ItemTemplateData.Common.Icon.IsNull())
		{
			return ItemTemplateData.Common.Icon;
		}
	}

	return TSoftObjectPtr<UTexture2D>();
}

EFrontierItemCategory FFrontierItemInstance::GetCategory() const
{
	if (HasItemTemplateData())
	{
		return ItemTemplateData.Common.Category;
	}

	return EFrontierItemCategory::Material;
}

bool FFrontierItemInstance::IsStackable() const
{
	if (HasItemTemplateData())
	{
		return ItemTemplateData.Common.bStackable;
	}

	return false;
}

int32 FFrontierItemInstance::GetMaxStack() const
{
	if (HasItemTemplateData())
	{
		return FMath::Max(1, ItemTemplateData.Common.MaxStack);
	}

	return 1;
}

int32 FFrontierItemInstance::GetMaxDurability() const
{
	if (HasItemTemplateData())
	{
		return FMath::Max(0, ItemTemplateData.MaxDurability);
	}

	return 0;
}

float FFrontierItemInstance::GetItemWeight() const
{
	if (HasItemTemplateData())
	{
		return FMath::Max(0.0f, ItemTemplateData.Common.ItemWeight);
	}

	return 0.0f;
}

int32 FFrontierItemInstance::GetSellPriceGold() const
{
	if (HasItemTemplateData())
	{
		return FMath::Max(0, ItemTemplateData.Common.SellPriceGold);
	}

	return 0;
}

EFrontierEquipmentSlot FFrontierItemInstance::GetEquipSlot() const
{
	return HasItemTemplateData() ? ItemTemplateData.EquipSlot : EFrontierEquipmentSlot::None;
}

const TArray<EFrontierEquipmentSlot>& FFrontierItemInstance::GetAllowedEquipSlots() const
{
	if (HasItemTemplateData())
	{
		return ItemTemplateData.AllowedEquipSlots;
	}

	static const TArray<EFrontierEquipmentSlot> EmptySlots;
	return EmptySlots;
}

TSoftObjectPtr<UFrontierWeaponDataAsset> FFrontierItemInstance::GetWeaponData() const
{
	return HasItemTemplateData() ? ItemTemplateData.WeaponData : TSoftObjectPtr<UFrontierWeaponDataAsset>();
}

TSoftClassPtr<AFrontierWeaponBase> FFrontierItemInstance::GetWeaponActorClass() const
{
	return HasItemTemplateData() ? ItemTemplateData.WeaponActorClass : TSoftClassPtr<AFrontierWeaponBase>();
}

TSoftObjectPtr<UFrontierWeaponSkillGenerationDataAsset> FFrontierItemInstance::GetSkillGenerationData() const
{
	return HasItemTemplateData() ? ItemTemplateData.SkillGenerationData : TSoftObjectPtr<UFrontierWeaponSkillGenerationDataAsset>();
}

bool FFrontierItemInstance::ResolveArmorAppearance(
	const EFrontierCharacterType CharacterType,
	FFrontierArmorAppearanceData& OutAppearance) const
{
	if (!HasItemTemplateData() || GetCategory() != EFrontierItemCategory::Armor)
	{
		return false;
	}

	for (const FFrontierArmorAppearanceData& Appearance : ItemTemplateData.ArmorAppearances)
	{
		if (Appearance.CharacterType == CharacterType)
		{
			OutAppearance = Appearance;
			return !Appearance.EquipmentStaticMesh.IsNull()
				|| !Appearance.EquipmentSkeletalMesh.IsNull();
		}
	}

	if (!ItemTemplateData.EquipmentStaticMesh.IsNull() || !ItemTemplateData.EquipmentMesh.IsNull())
	{
		OutAppearance.CharacterType = CharacterType;
		OutAppearance.EquipmentStaticMesh = ItemTemplateData.EquipmentStaticMesh;
		OutAppearance.EquipmentSkeletalMesh = ItemTemplateData.EquipmentMesh;
		OutAppearance.CharacterAttachSocketName = ItemTemplateData.CharacterAttachSocketName;
		OutAppearance.EquipOffset = ItemTemplateData.EquipOffset;
		return true;
	}

	return false;
}

TSoftClassPtr<UGameplayEffect> FFrontierItemInstance::GetConsumeEffectClass() const
{
	return HasItemTemplateData() ? ItemTemplateData.ConsumeEffectClass : TSoftClassPtr<UGameplayEffect>();
}

float FFrontierItemInstance::GetHealthRestoreAmount() const
{
	return HasItemTemplateData() ? FMath::Max(0.0f, ItemTemplateData.HealthRestoreAmount) : 0.0f;
}

float FFrontierItemInstance::GetStaminaRestoreAmount() const
{
	return HasItemTemplateData() ? FMath::Max(0.0f, ItemTemplateData.StaminaRestoreAmount) : 0.0f;
}

float FFrontierItemInstance::GetUseTime() const
{
	return HasItemTemplateData() ? FMath::Max(0.0f, ItemTemplateData.UseTime) : 0.0f;
}

TSoftObjectPtr<UAnimMontage> FFrontierItemInstance::GetUseMontage() const
{
	return HasItemTemplateData() ? ItemTemplateData.UseMontage : TSoftObjectPtr<UAnimMontage>();
}

FName FFrontierItemInstance::GetPotionSocketName() const
{
	return HasItemTemplateData() ? ItemTemplateData.PotionSocketName : NAME_None;
}

TSoftObjectPtr<UStaticMesh> FFrontierItemInstance::GetPotionMesh() const
{
	return HasItemTemplateData() ? ItemTemplateData.PotionMesh : TSoftObjectPtr<UStaticMesh>();
}

const FTransform& FFrontierItemInstance::GetPotionMeshRelativeTransform() const
{
	static const FTransform IdentityTransform = FTransform::Identity;
	return HasItemTemplateData() ? ItemTemplateData.PotionMeshRelativeTransform : IdentityTransform;
}

bool FFrontierItemInstance::IsUsableInRaid() const
{
	return HasItemTemplateData() && ItemTemplateData.bUsableInRaid;
}

bool FFrontierItemInstance::IsUsableInLobby() const
{
	return HasItemTemplateData() && ItemTemplateData.bUsableInLobby;
}

void FFrontierItemInstance::EnsureInstanceId()
{
	if (!RequiresInstanceId())
	{
		// Stackable runtime loot may remain transient, but never discard an ID assigned by the backend.
		return;
	}

	if (!ItemInstanceId.IsValid())
	{
		ItemInstanceId = FGuid::NewGuid();
	}
}

void FFrontierItemInstance::EnsureRuntimeIdentity()
{
	EnsureInstanceId();
}

bool FFrontierItemInstance::RequiresInstanceId() const
{
	if (HasItemTemplateData())
	{
		return !ItemTemplateData.Common.bStackable
			|| ItemTemplateData.Common.Category == EFrontierItemCategory::Weapon
			|| ItemTemplateData.Common.Category == EFrontierItemCategory::Armor
			|| ItemTemplateData.Common.Category == EFrontierItemCategory::Accessory;
	}

	return false;
}

float FFrontierItemInstance::GetCurrentFinalStatValue(const FGameplayTag StatTag, const float FallbackValue) const
{
	for (const FFrontierRuntimeStatData& RuntimeStat : RuntimeGeneratedStats)
	{
		if (RuntimeStat.StatTag == StatTag)
		{
			return RuntimeStat.FinalValue;
		}
	}

	return FallbackValue;
}

void FFrontierItemInstance::BuildActiveSkillTags(TArray<FGameplayTag>& OutActiveSkillTags) const
{
	OutActiveSkillTags.Reset();
	if (GetCategory() != EFrontierItemCategory::Weapon)
	{
		return;
	}

	if (!RuntimeGeneratedSkills.IsEmpty())
	{
		TArray<FFrontierRuntimeSkillData> SortedSkills = RuntimeGeneratedSkills;
		SortedSkills.Sort([](const FFrontierRuntimeSkillData& Left, const FFrontierRuntimeSkillData& Right)
		{
			return Left.SlotIndex < Right.SlotIndex;
		});

		for (const FFrontierRuntimeSkillData& GeneratedSkill : SortedSkills)
		{
			if (GeneratedSkill.SkillTag.IsValid())
			{
				OutActiveSkillTags.Add(GeneratedSkill.SkillTag);
			}
		}
		return;
	}

}

FGameplayTag FFrontierItemInstance::GetDisplayRarityTag() const
{
	if (!IsValid())
	{
		return FGameplayTag();
	}

	const FGameplayTag RarityTag = ConvertItemRarityToGameplayTag(FinalRarity);
	return RarityTag;
}

EFrontierItemRarity FFrontierItemInstance::GetDisplayRarity() const
{
	if (!IsValid())
	{
		return EFrontierItemRarity::Common;
	}

	return FinalRarity;
}
