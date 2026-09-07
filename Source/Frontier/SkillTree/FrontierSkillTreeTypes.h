#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Inventory/FrontierItemSharedTypes.h"
#include "FrontierSkillTreeTypes.generated.h"

class UGameplayEffect;
class UTexture2D;

UENUM(BlueprintType)
enum class EFrontierSkillTreeRequestResult : uint8
{
	Success,
	InvalidNode,
	AlreadyMaxRank,
	InsufficientPoints,
	MissingPrerequisite,
	InvalidContext,
	NotAuthority,
	InvalidData
};

/** How a node with multiple incoming connections evaluates its prerequisites. */
UENUM(BlueprintType)
enum class EFrontierSkillTreePrerequisitePolicy : uint8
{
	/** Every connected prerequisite node must have the requested rank. */
	All,

	/** At least one connected prerequisite node must have the requested rank. */
	Any
};

/** Shared, two-tone palette used by node borders and their connection lines. */
USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierSkillTreeVisualPalette
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locked")
	FLinearColor LockedPrimary = FLinearColor(0.055f, 0.065f, 0.08f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locked")
	FLinearColor LockedAccent = FLinearColor(0.14f, 0.16f, 0.20f, 0.78f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Common")
	FLinearColor CommonPrimary = FLinearColor(0.72f, 0.69f, 0.62f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Common")
	FLinearColor CommonAccent = FLinearColor(1.0f, 0.92f, 0.72f, 1.0f);

	/** Steel-blue rather than a saturated primary blue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sword")
	FLinearColor SwordPrimary = FLinearColor(0.11f, 0.28f, 0.58f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sword")
	FLinearColor SwordAccent = FLinearColor(0.42f, 0.67f, 1.0f, 1.0f);

	/** Wine red with a restrained ember highlight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axe")
	FLinearColor AxePrimary = FLinearColor(0.48f, 0.07f, 0.11f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axe")
	FLinearColor AxeAccent = FLinearColor(0.92f, 0.30f, 0.22f, 1.0f);

	/** Antique-gold instead of a flat yellow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bow")
	FLinearColor BowPrimary = FLinearColor(0.58f, 0.34f, 0.055f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bow")
	FLinearColor BowAccent = FLinearColor(1.0f, 0.72f, 0.20f, 1.0f);

	/** Deep jade with a soft green highlight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spear")
	FLinearColor SpearPrimary = FLinearColor(0.055f, 0.34f, 0.18f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spear")
	FLinearColor SpearAccent = FLinearColor(0.30f, 0.74f, 0.43f, 1.0f);

	/** Neutral silver used by Normal and None weapon attacks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Element Tint")
	FLinearColor NormalElementTint = FLinearColor(FColor(184, 188, 196));

	/** Restrained ember red rather than a saturated primary red. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Element Tint")
	FLinearColor FireElementTint = FLinearColor(FColor(200, 79, 62));

	/** Soft sky blue for ice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Element Tint")
	FLinearColor IceElementTint = FLinearColor(FColor(115, 183, 214));

	/** Muted amethyst for poison. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Element Tint")
	FLinearColor PoisonElementTint = FLinearColor(FColor(139, 90, 163));

	/** Warm antique gold for lightning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Element Tint")
	FLinearColor LightningElementTint = FLinearColor(FColor(216, 180, 90));

	void ResolveCategoryColors(
		FGameplayTag CategoryTag,
		FLinearColor& OutPrimary,
		FLinearColor& OutAccent) const;

	FLinearColor ResolveElementTint(EFrontierElementalType ElementalType) const;
};

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierSkillTreeStatModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree")
	FGameplayTag StatTag;

	/** Delta contributed by this single rank. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree")
	float Magnitude = 0.0f;
};

/** Attack power that applies only when both the equipped weapon family and attack element match. */
USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierSkillTreeConditionalAttackModifier
{
	GENERATED_BODY()

	/** Use a family tag such as Weapon.Type.Sword, not a concrete item tag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree")
	FGameplayTag WeaponFamilyTag;

	/** None is normalized to Normal at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree")
	EFrontierElementalType ElementalType = EFrontierElementalType::Normal;

	/** Delta contributed by this single purchased rank. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree")
	float Magnitude = 0.0f;
};

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierSkillTreePrerequisite
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree")
	FGameplayTag NodeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree", meta=(ClampMin="1"))
	int32 RequiredRank = 1;
};

/** A directed line in the tree, derived from a target node's prerequisite. */
USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierSkillTreeConnection
{
	GENERATED_BODY()

	/** The prerequisite node where the line starts. */
	UPROPERTY(BlueprintReadOnly, Category="Skill Tree")
	FGameplayTag SourceNodeTag;

	/** The unlockable node where the line ends. */
	UPROPERTY(BlueprintReadOnly, Category="Skill Tree")
	FGameplayTag TargetNodeTag;

	UPROPERTY(BlueprintReadOnly, Category="Skill Tree")
	int32 RequiredSourceRank = 1;
};

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierSkillTreeRankDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Tree", meta=(TitleProperty="StatTag"))
	TArray<FFrontierSkillTreeStatModifier> StatModifiers;

	/** Does not create GAS attributes; these values are summed into a server-side lookup cache. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Tree", meta=(TitleProperty="WeaponFamilyTag"))
	TArray<FFrontierSkillTreeConditionalAttackModifier> ConditionalAttackModifiers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Tree")
	TArray<TSubclassOf<UGameplayEffect>> GrantedEffects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Tree")
	FGameplayTagContainer GrantedTags;
};

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierSkillTreeNodeDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Tree")
	FGameplayTag NodeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Tree")
	FGameplayTag TreeCategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Tree")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Tree", meta=(MultiLine=true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Tree")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** Optional outer elemental artwork used only by weapon/element attack nodes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Tree", meta=(DisplayName="Element Icon (Weapon Attack Only)"))
	TObjectPtr<UTexture2D> ElementIcon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Tree")
	FVector2D UIPosition = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Tree", meta=(ClampMin="1"))
	int32 MaxRank = 1;

	/** Cost for each rank. The last entry is reused if MaxRank is larger. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Tree")
	TArray<int32> RankCosts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Tree", meta=(TitleProperty="NodeTag"))
	TArray<FFrontierSkillTreePrerequisite> Prerequisites;

	/** Applies only when this node has more than one prerequisite connection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Tree")
	EFrontierSkillTreePrerequisitePolicy PrerequisitePolicy = EFrontierSkillTreePrerequisitePolicy::All;

	/** Per-rank deltas. The last entry is reused if MaxRank is larger. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Tree")
	TArray<FFrontierSkillTreeRankDefinition> Ranks;
};

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierSavedSkillTreeNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree")
	FGameplayTag NodeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree", meta=(ClampMin="0"))
	int32 Rank = 0;
};

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierSkillTreeProgressionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree", meta=(ClampMin="0"))
	int32 AvailableSkillPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree", meta=(ClampMin="0"))
	int32 TotalSkillPointsEarned = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree", meta=(TitleProperty="NodeTag"))
	TArray<FFrontierSavedSkillTreeNode> Nodes;
};

