#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Character/FrontierCharacterTypes.h"
#include "FrontierCharacterSaveGame.generated.h"

UCLASS()
class FRONTIER_API UFrontierCharacterSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Character")
	EFrontierCharacterType SelectedCharacterType = EFrontierCharacterType::DarkKnight;
};
