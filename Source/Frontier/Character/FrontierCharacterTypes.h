#pragma once

#include "CoreMinimal.h"
#include "FrontierCharacterTypes.generated.h"

UENUM(BlueprintType)
enum class EFrontierCharacterType : uint8
{
	DarkKnight UMETA(DisplayName="Dark Knight"),
	DarkLady UMETA(DisplayName="Dark Lady")
};

inline bool IsValidFrontierCharacterType(const EFrontierCharacterType CharacterType)
{
	return CharacterType == EFrontierCharacterType::DarkKnight
		|| CharacterType == EFrontierCharacterType::DarkLady;
}
