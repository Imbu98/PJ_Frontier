#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Inventory/FrontierItemSharedTypes.h"
#include "FrontierSkillTypes.generated.h"

class UGameplayAbility;
class UFrontierSkillDataAsset;
class UTexture2D;

UENUM(BlueprintType)
enum class EFrontierSkillModifierOperation : uint8
{
	Override,
	Add,
	Multiply
};

USTRUCT(BlueprintType)
struct FFrontierSkillModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	FGameplayTag ParameterTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	EFrontierSkillModifierOperation Operation = EFrontierSkillModifierOperation::Add;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	float Value = 0.0f;
};

/** RowName is the SkillId used by the skill template table. */
USTRUCT(BlueprintType)
struct FFrontierSkillBalanceTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill|Base", meta=(TitleProperty="ParameterTag"))
	TArray<FFrontierSkillModifier> BaseEffects;

	/** Serialized property name is retained for existing DataTable compatibility. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill|Milestone", meta=(DisplayName="Level 4 Effects", TitleProperty="ParameterTag"))
	TArray<FFrontierSkillModifier> Level3Effects;

	/** Serialized property name is retained for existing DataTable compatibility. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill|Milestone", meta=(DisplayName="Level 8 Effects", TitleProperty="ParameterTag"))
	TArray<FFrontierSkillModifier> Level6Effects;

	/** Serialized property name is retained for existing DataTable compatibility. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill|Milestone", meta=(DisplayName="Level 12 Effects", TitleProperty="ParameterTag"))
	TArray<FFrontierSkillModifier> Level9Effects;
};

USTRUCT(BlueprintType)
struct FFrontierWeightedSkillCount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill", meta=(ClampMin="1", ClampMax="3"))
	int32 SkillCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill", meta=(ClampMin="0.0"))
	float Weight = 1.0f;
};

USTRUCT(BlueprintType)
struct FFrontierSkillRarityWeight
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill")
	EFrontierItemRarity SkillRarity = EFrontierItemRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill", meta=(DeprecatedProperty, DeprecationMessage="Use SkillRarity enum."))
	FGameplayTag SkillRarityTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill", meta=(ClampMin="0.0"))
	float Weight = 1.0f;
};

USTRUCT(BlueprintType)
struct FFrontierSkillCandidateByRarity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill")
	FGameplayTag SkillTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill")
	EFrontierElementalType ElementalType = EFrontierElementalType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill", meta=(ClampMin="0.0"))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill")
	bool bIsAdvancedSkill = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill", meta=(ClampMin="0"))
	int32 MinWeaponLevel = 0;
};

USTRUCT(BlueprintType)
struct FFrontierSkillRarityCandidateGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill")
	EFrontierItemRarity SkillRarity = EFrontierItemRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill", meta=(DeprecatedProperty, DeprecationMessage="Use SkillRarity enum."))
	FGameplayTag SkillRarityTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill", meta=(TitleProperty="SkillTag"))
	TArray<FFrontierSkillCandidateByRarity> SkillCandidates;
};

USTRUCT(BlueprintType)
struct FFrontierWeaponTypeSkillDataAsset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill")
	FGameplayTag RequiredWeaponTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill")
	TSoftObjectPtr<UFrontierSkillDataAsset> WeaponTypeSkillDataAsset;
};

USTRUCT(BlueprintType)
struct FFrontierGeneratedWeaponSkill
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill", meta=(DisplayName="SkillId"))
	FString SkillTemplateId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill")
	FGameplayTag SkillTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill", meta=(DeprecatedProperty, DeprecationMessage="Skill rarity is backend generation metadata only. Runtime ability grant uses SkillLevel."))
	EFrontierItemRarity SkillRarity = EFrontierItemRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill", meta=(DeprecatedProperty, DeprecationMessage="Skill rarity is backend generation metadata only. Runtime ability grant uses SkillLevel."))
	FGameplayTag SkillRarityTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill", meta=(DeprecatedProperty, DeprecationMessage="Advanced limits are enforced by the backend. Kept only for response validation/logging."))
	bool bIsAdvancedSkill = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill", meta=(ClampMin="1"))
	int32 SkillLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill", meta=(ClampMin="0", ClampMax="2"))
	int32 SlotIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct FFrontierEquipmentSkillOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill")
	FGameplayTag SkillTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill")
	int32 LevelValue = 0;
};

USTRUCT(BlueprintType)
struct FFrontierSkillLevelEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill")
	FGameplayTag SkillTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill")
	int32 Level = 0;
};

USTRUCT(BlueprintType)
struct FFrontierSkillCooldownEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill")
	FGameplayTag SkillTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill")
	float CooldownDuration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill")
	float CooldownEndTime = 0.0f;
};

UENUM(BlueprintType)
enum class EFrontierSkillDescriptionValueFormat : uint8
{
	Number,
	Integer,
	Percent,
	Seconds
};

USTRUCT(BlueprintType)
struct FFrontierSkillDescriptionParameter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill|Description")
	FGameplayTag ParameterTag;

	/** Base value before skill balance and milestone modifiers are applied. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill|Description")
	float DefaultValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill|Description")
	EFrontierSkillDescriptionValueFormat Format = EFrontierSkillDescriptionValueFormat::Number;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill|Description", meta=(ClampMin="0", ClampMax="3"))
	int32 DecimalPlaces = 0;
};

USTRUCT(BlueprintType)
struct FFrontierSkillInfo
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill")
	FName SkillId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill")
	FGameplayTag SkillTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill", meta=(MultiLine=true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill|Description", meta=(TitleProperty="ParameterTag"))
	TArray<FFrontierSkillDescriptionParameter> DescriptionParameters;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill")
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill", meta=(ClampMin="0.0"))
	float Cooldown = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill")
	FGameplayTag CooldownTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill", meta=(ClampMin="0.0"))
	float StaminaCost = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill", meta=(ClampMin="1"))
	int32 MaxLevel = 10;

	
};

USTRUCT(BlueprintType)
struct FFrontierSkillTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	FName SkillId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	FGameplayTag SkillTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill", meta=(MultiLine=true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill|Description", meta=(TitleProperty="ParameterTag"))
	TArray<FFrontierSkillDescriptionParameter> DescriptionParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	EFrontierElementalType ElementalType = EFrontierElementalType::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill", meta=(ClampMin="1"))
	int32 MaxLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill", meta=(ClampMin="0.0"))
	float Cooldown = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	FGameplayTag CooldownTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill", meta=(ClampMin="0.0"))
	float StaminaCost = 0.0f;


};
