#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Character/FrontierCharacterTypes.h"
#include "Inventory/FrontierItemSharedTypes.h"
#include "Skill/FrontierSkillTypes.h"
#include "FrontierInventoryTypes.generated.h"

class UFrontierItemDataAsset;
class UFrontierWeaponDataAsset;
class UFrontierWeaponSkillGenerationDataAsset;
class UGameplayEffect;
class UAnimMontage;
class USkeletalMesh;
class UStaticMesh;
class UTexture2D;
class AFrontierWeaponBase;

UENUM(BlueprintType)
enum class EFrontierItemType : uint8
{
	None,
	Weapon,
	Armor,
	Consumable,
	Material,
	Quest,
	Misc,
	Accessory
};

UENUM(BlueprintType)
enum class EFrontierEquipmentSlot : uint8
{
	None,
	MainWeapon,
	SubWeapon,
	Helmet,
	Chest,
	Gloves,
	Boots,
	Necklace,
	Ring
};

USTRUCT(BlueprintType)
struct FFrontierItemTemplateData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item", meta=(MultiLine=true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	EFrontierItemCategory Category = EFrontierItemCategory::Material;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	FName SubCategory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	EFrontierElementalType ElementalType = EFrontierElementalType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	bool bStackable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	int32 MaxStack = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	EFrontierItemBindState BindState = EFrontierItemBindState::Unbound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	bool bTradable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	int32 SellPriceGold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	float ItemWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Drop")
	TSoftObjectPtr<UStaticMesh> DropStaticMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Drop")
	FVector DropWorldScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Drop", meta=(ClampMin="0.0"))
	float DropImpulseForward = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Drop", meta=(ClampMin="0.0"))
	float DropImpulseUpward = 120.0f;

};

/** Fields shared by all equippable item templates. Non-equipment templates do not contain this data. */
USTRUCT(BlueprintType)
struct FFrontierEquipmentTemplateData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Equipment", meta=(ClampMin="0.0"))
	float BaseValue = 0.0f;
};

USTRUCT(BlueprintType)
struct FFrontierWeaponTemplateData : public FTableRowBase
{
	GENERATED_BODY()
	
	FFrontierWeaponTemplateData()
	{
		ItemTemplateData.Category = EFrontierItemCategory::Weapon;
		ItemTemplateData.bStackable = false;
		ItemTemplateData.MaxStack = 1;
		MaxDurability = 100;
	}


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	FFrontierItemTemplateData ItemTemplateData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Equipment")
	EFrontierEquipmentSlot EquipSlot = EFrontierEquipmentSlot::MainWeapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Equipment")
	FFrontierEquipmentTemplateData EquipmentData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item", meta=(ClampMin="0"))
	int32 MaxDurability = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Weapon")
	TSoftObjectPtr<UFrontierWeaponDataAsset> WeaponData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Weapon")
	TSoftClassPtr<AFrontierWeaponBase> WeaponActorClass;
};

USTRUCT(BlueprintType)
struct FFrontierArmorAppearanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Armor")
	EFrontierCharacterType CharacterType = EFrontierCharacterType::DarkKnight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Armor")
	TSoftObjectPtr<USkeletalMesh> EquipmentSkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Armor")
	TSoftObjectPtr<UStaticMesh> EquipmentStaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Armor")
	FName CharacterAttachSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Armor")
	FTransform EquipOffset = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct FFrontierArmorTemplateData : public FTableRowBase
{
	GENERATED_BODY()
	
	FFrontierArmorTemplateData()
	{
		ItemTemplateData.Category = EFrontierItemCategory::Armor;
		ItemTemplateData.bStackable = false;
		ItemTemplateData.MaxStack = 1;
		MaxDurability = 100;
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	FFrontierItemTemplateData ItemTemplateData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Equipment")
	EFrontierEquipmentSlot EquipSlot = EFrontierEquipmentSlot::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Equipment")
	FFrontierEquipmentTemplateData EquipmentData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item", meta=(ClampMin="0"))
	int32 MaxDurability = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Armor")
	TArray<FFrontierArmorAppearanceData> CharacterAppearances;
};

USTRUCT(BlueprintType)
struct FFrontierConsumableTemplateData : public FTableRowBase
{
	GENERATED_BODY()
	
	FFrontierConsumableTemplateData()
	{
		ItemTemplateData.Category = EFrontierItemCategory::Consumable;
		ItemTemplateData.bStackable = true;
		ItemTemplateData.MaxStack = 99;

	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	FFrontierItemTemplateData ItemTemplateData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Consumable")
	TSoftClassPtr<UGameplayEffect> ConsumeEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Consumable", meta=(ClampMin="0.0"))
	float HealthRestoreAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Consumable", meta=(ClampMin="0.0"))
	float StaminaRestoreAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Consumable", meta=(ClampMin="0.0"))
	float UseTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Consumable|Use Animation")
	TSoftObjectPtr<UAnimMontage> UseMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Consumable|Use Animation")
	FName PotionSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Consumable|Use Animation")
	TSoftObjectPtr<UStaticMesh> PotionMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Consumable|Use Animation")
	FTransform PotionMeshRelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Consumable")
	FGameplayTag CooldownGroupTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Consumable")
	bool bUsableInRaid = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Consumable")
	bool bUsableInLobby = false;
};

USTRUCT(BlueprintType)
struct FFrontierMaterialTemplateData : public FTableRowBase
{
	GENERATED_BODY()
	
	FFrontierMaterialTemplateData()
	{
		ItemTemplateData.Category = EFrontierItemCategory::Material;
		ItemTemplateData.bStackable = true;
		ItemTemplateData.MaxStack = 99;

	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	FFrontierItemTemplateData ItemTemplateData;
};

/**
 * Typed template row for equippable accessories. Accessory skills and stats
 * are supplied by the runtime item instance; no weapon attack data belongs here.
 */
USTRUCT(BlueprintType)
struct FFrontierAccessoryTemplateData : public FTableRowBase
{
	GENERATED_BODY()

	FFrontierAccessoryTemplateData()
	{
		ItemTemplateData.Category = EFrontierItemCategory::Accessory;
		ItemTemplateData.bStackable = false;
		ItemTemplateData.MaxStack = 1;
		MaxDurability = 100;
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	FFrontierItemTemplateData ItemTemplateData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Equipment")
	EFrontierEquipmentSlot EquipSlot = EFrontierEquipmentSlot::Necklace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Equipment")
	FFrontierEquipmentTemplateData EquipmentData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item", meta=(ClampMin="0"))
	int32 MaxDurability = 100;
};

/**
 * Runtime catalog record assembled from exactly one typed DataTable row.
 * This is not an editable DataTable row; unrelated fields remain empty by category.
 */
USTRUCT(BlueprintType)
struct FFrontierResolvedItemTemplateData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item")
	FName ItemTemplateId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item")
	FFrontierItemTemplateData Common;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Equipment")
	EFrontierEquipmentSlot EquipSlot = EFrontierEquipmentSlot::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Equipment")
	float EquipmentBaseValue = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Equipment")
	TArray<EFrontierEquipmentSlot> AllowedEquipSlots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item")
	int32 MaxDurability = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Equipment")
	TSoftObjectPtr<USkeletalMesh> EquipmentMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Armor")
	TArray<FFrontierArmorAppearanceData> ArmorAppearances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Armor|Legacy")
	TSoftObjectPtr<UStaticMesh> EquipmentStaticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Armor|Legacy")
	FName CharacterAttachSocketName = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Armor|Legacy")
	FTransform EquipOffset = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Weapon")
	TSoftObjectPtr<UFrontierWeaponDataAsset> WeaponData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Weapon")
	TSoftClassPtr<AFrontierWeaponBase> WeaponActorClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Weapon")
	TSoftObjectPtr<UFrontierWeaponSkillGenerationDataAsset> SkillGenerationData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Consumable")
	TSoftClassPtr<UGameplayEffect> ConsumeEffectClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Consumable")
	float HealthRestoreAmount = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Consumable")
	float StaminaRestoreAmount = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Consumable")
	float UseTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Consumable|Use Animation")
	TSoftObjectPtr<UAnimMontage> UseMontage;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Consumable|Use Animation")
	FName PotionSocketName = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Consumable|Use Animation")
	TSoftObjectPtr<UStaticMesh> PotionMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Consumable|Use Animation")
	FTransform PotionMeshRelativeTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Consumable")
	FGameplayTag CooldownGroupTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Consumable")
	bool bUsableInRaid = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|Consumable")
	bool bUsableInLobby = false;
};

USTRUCT(BlueprintType)
struct FFrontierItemStatModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	FGameplayTag StatTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	float Magnitude = 0.0f;
};

USTRUCT(BlueprintType)
struct FFrontierRuntimeStatData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FString OptionId;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FGameplayTag StatTag;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FString Unit;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	float BaseValue = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	float RandomValue = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	float UpgradeValue = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	float FinalValue = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	float NormalizedValue = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	bool bHasNormalizedValue = false;
};

USTRUCT(BlueprintType)
struct FFrontierRuntimeSkillData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Inventory", meta=(DisplayName="SkillId"))
	FString SkillTemplateId;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FGameplayTag SkillTag;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FString SkillRarity;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FString SkillElementalType;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	int32 SkillLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	int32 SlotIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct FFrontierItemInstance
{
	GENERATED_BODY()

	static FFrontierItemInstance CreateFromItemData(const UFrontierItemDataAsset* InItemData, int32 InQuantity = 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	FGuid ItemInstanceId;

	/** Raid-scoped identity issued by join authorization or generated by the dedicated server for new loot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory|Raid")
	FGuid RaidItemId;

	/** Permanent backend identity for an item brought into the raid. Invalid for newly generated raid loot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory|Raid")
	FGuid OriginItemInstanceId;

	/** Backend/DS loot source identity. Empty for brought-in items and legacy temporary loot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory|Raid")
	FString RaidLootSourceId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
	FFrontierResolvedItemTemplateData ItemTemplateData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
	EFrontierItemRarity FinalRarity = EFrontierItemRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	int32 Quantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	int32 EnhancementLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
	float Durability = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
	EFrontierItemBindState BindState = EFrontierItemBindState::Unbound;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
	TArray<FString> InstanceTags;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
	TArray<FFrontierRuntimeStatData> RuntimeGeneratedStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory|Skills")
	TArray<FFrontierRuntimeSkillData> RuntimeGeneratedSkills;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory|Equipment Score")
	float NormalizedScore = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory|Equipment Score")
	float EquipmentScore = 0.0f;

	FName GetTemplateId() const;
	bool HasItemTemplateData() const;
	const FFrontierItemTemplateData* GetItemTemplateData() const;
	FText GetDisplayNameText() const;
	FText GetDescriptionText() const;
	TSoftObjectPtr<UTexture2D> GetIcon() const;
	EFrontierItemCategory GetCategory() const;
	bool IsStackable() const;
	int32 GetMaxStack() const;
	int32 GetMaxDurability() const;
	float GetItemWeight() const;
	int32 GetSellPriceGold() const;
	EFrontierEquipmentSlot GetEquipSlot() const;
	const TArray<EFrontierEquipmentSlot>& GetAllowedEquipSlots() const;
	TSoftObjectPtr<UFrontierWeaponDataAsset> GetWeaponData() const;
	TSoftClassPtr<AFrontierWeaponBase> GetWeaponActorClass() const;
	TSoftObjectPtr<UFrontierWeaponSkillGenerationDataAsset> GetSkillGenerationData() const;
	bool ResolveArmorAppearance(
		EFrontierCharacterType CharacterType,
		FFrontierArmorAppearanceData& OutAppearance) const;
	TSoftClassPtr<UGameplayEffect> GetConsumeEffectClass() const;
	float GetHealthRestoreAmount() const;
	float GetStaminaRestoreAmount() const;
	float GetUseTime() const;
	TSoftObjectPtr<UAnimMontage> GetUseMontage() const;
	FName GetPotionSocketName() const;
	TSoftObjectPtr<UStaticMesh> GetPotionMesh() const;
	const FTransform& GetPotionMeshRelativeTransform() const;
	bool IsUsableInRaid() const;
	bool IsUsableInLobby() const;
	void EnsureInstanceId();
	void EnsureRuntimeIdentity();
	bool RequiresInstanceId() const;
	float GetCurrentFinalStatValue(FGameplayTag StatTag, float FallbackValue = 0.0f) const;
	void BuildActiveSkillTags(TArray<FGameplayTag>& OutActiveSkillTags) const;
	EFrontierItemRarity GetDisplayRarity() const;
	FGameplayTag GetDisplayRarityTag() const;
	bool IsValid() const
	{
		return HasItemTemplateData() && !GetTemplateId().IsNone() && Quantity > 0;
	}
};

USTRUCT(BlueprintType)
struct FFrontierInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	bool bOccupied = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	FFrontierItemInstance ItemInstance;
};

USTRUCT(BlueprintType)
struct FFrontierLoadoutSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	EFrontierEquipmentSlot SlotType = EFrontierEquipmentSlot::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	bool bOccupied = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	FFrontierItemInstance ItemInstance;
};
