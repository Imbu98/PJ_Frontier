#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FrontierCharacterAppearanceSettings.generated.h"

class UFrontierCharacterAppearanceDataAsset;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Frontier Character Appearance"))
class FRONTIER_API UFrontierCharacterAppearanceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	virtual FName GetSectionName() const override { return TEXT("Frontier Character Appearance"); }

	UPROPERTY(Config, EditAnywhere, Category="Appearance")
	TSoftObjectPtr<UFrontierCharacterAppearanceDataAsset> CharacterAppearanceData;
};
