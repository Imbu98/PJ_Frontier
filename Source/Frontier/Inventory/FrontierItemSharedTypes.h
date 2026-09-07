#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "FrontierItemSharedTypes.generated.h"

UENUM(BlueprintType)
enum class EFrontierItemCategory : uint8
{
	Weapon,
	Armor,
	Consumable,
	Material,
	Accessory
};

UENUM(BlueprintType)
enum class EFrontierElementalType : uint8
{
	None,
	Normal,
	Fire,
	Ice,
	Lightning,
	Poison
};

UENUM(BlueprintType)
enum class EFrontierItemBindState : uint8
{
	Unbound,
	AccountBound,
	CharacterBound
};

UENUM(BlueprintType)
enum class EFrontierItemRarity : uint8
{
	Common,
	Rare,
	Epic,
	Legendary
};

enum class EFrontierItemType : uint8;

FRONTIER_API bool IsValidElementForCategory(EFrontierItemCategory Category, EFrontierElementalType ElementalType);
FRONTIER_API bool TryParseItemCategory(const FString& Value, EFrontierItemCategory& OutCategory);
FRONTIER_API bool TryParseElementalType(const FString& Value, EFrontierElementalType& OutElementalType);
FRONTIER_API bool TryParseItemRarity(const FString& Value, EFrontierItemRarity& OutRarity);
FRONTIER_API FGameplayTag ConvertItemRarityToGameplayTag(EFrontierItemRarity Rarity);
FRONTIER_API FString ConvertItemRarityToBackendString(EFrontierItemRarity Rarity);
FRONTIER_API EFrontierItemCategory ConvertLegacyItemTypeToCategory(EFrontierItemType ItemType);
