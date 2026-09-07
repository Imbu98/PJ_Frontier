#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FrontierUISettings.generated.h"

class UFrontierCommonPopupWidget;
class UFrontierRaidLoadingWidget;
class USoundBase;

/** Project-wide UI classes and viewport-layer settings. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Frontier UI"))
class FRONTIER_API UFrontierUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	virtual FName GetSectionName() const override { return TEXT("Frontier UI"); }

	/** Create one WBP derived from FrontierCommonPopupWidget and assign it here. */
	UPROPERTY(Config, EditAnywhere, Category="Common Popup")
	TSoftClassPtr<UFrontierCommonPopupWidget> CommonPopupWidgetClass;

	UPROPERTY(Config, EditAnywhere, Category="Common Popup")
	int32 CommonPopupZOrder = 1000;

	/** Loading screens used during travel. One configured class is selected randomly per transition. */
	UPROPERTY(Config, EditAnywhere, Category="Raid Loading Screen")
	TArray<TSoftClassPtr<UFrontierRaidLoadingWidget>> RaidLoadingWidgetClasses;

	/** Played locally when the player inventory or lobby storage opens. */
	UPROPERTY(Config, EditAnywhere, Category="UI Audio")
	TSoftObjectPtr<USoundBase> InventoryOpenSound;

	/** Played locally when a world loot container UI opens. */
	UPROPERTY(Config, EditAnywhere, Category="UI Audio")
	TSoftObjectPtr<USoundBase> LootContainerOpenSound;

	UPROPERTY(Config, EditAnywhere, Category="UI Audio", meta=(ClampMin="0.0"))
	float UIOpenSoundVolumeMultiplier = 1.0f;

	UPROPERTY(Config, EditAnywhere, Category="UI Audio", meta=(ClampMin="0.01"))
	float UIOpenSoundPitchMultiplier = 1.0f;
};
