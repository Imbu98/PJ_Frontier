#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "FrontierUserSettingsSaveGame.generated.h"

UCLASS()
class FRONTIER_API UFrontierUserSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	float MouseSensitivity = 50.0f;
};
