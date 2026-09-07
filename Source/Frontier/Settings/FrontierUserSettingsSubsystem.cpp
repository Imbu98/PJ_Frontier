#include "Settings/FrontierUserSettingsSubsystem.h"

#include "Frontier.h"
#include "Kismet/GameplayStatics.h"
#include "Save/FrontierUserSettingsSaveGame.h"

const FString UFrontierUserSettingsSubsystem::SettingsSaveSlotName =
	TEXT("FrontierUserSettings");

void UFrontierUserSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!IsRunningDedicatedServer())
	{
		LoadSettings();
	}
}

void UFrontierUserSettingsSubsystem::SetMouseSensitivity(const float NewSensitivity)
{
	const float ClampedSensitivity = FMath::Clamp(
		NewSensitivity,
		MinimumMouseSensitivity,
		MaximumMouseSensitivity);
	if (FMath::IsNearlyEqual(MouseSensitivity, ClampedSensitivity))
	{
		return;
	}

	MouseSensitivity = ClampedSensitivity;
	bSettingsDirty = true;
}

float UFrontierUserSettingsSubsystem::GetMouseSensitivityScale() const
{
	return MouseSensitivity / DefaultMouseSensitivity;
}

bool UFrontierUserSettingsSubsystem::SaveSettings()
{
	if (IsRunningDedicatedServer() || !bSettingsDirty)
	{
		return true;
	}

	UFrontierUserSettingsSaveGame* SaveGame = Cast<UFrontierUserSettingsSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UFrontierUserSettingsSaveGame::StaticClass()));
	if (!SaveGame)
	{
		FRONTIER_LOG(Error, TEXT("Failed to create local user settings SaveGame."));
		return false;
	}

	SaveGame->MouseSensitivity = MouseSensitivity;
	const bool bSaved = UGameplayStatics::SaveGameToSlot(
		SaveGame,
		SettingsSaveSlotName,
		SettingsSaveUserIndex);
	if (bSaved)
	{
		bSettingsDirty = false;
	}
	else
	{
		FRONTIER_LOG(Warning, TEXT("Failed to save local user settings."));
	}

	return bSaved;
}

bool UFrontierUserSettingsSubsystem::LoadSettings()
{
	MouseSensitivity = DefaultMouseSensitivity;
	bSettingsDirty = false;

	if (!UGameplayStatics::DoesSaveGameExist(SettingsSaveSlotName, SettingsSaveUserIndex))
	{
		bSettingsDirty = true;
		return SaveSettings();
	}

	const UFrontierUserSettingsSaveGame* SaveGame = Cast<UFrontierUserSettingsSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SettingsSaveSlotName, SettingsSaveUserIndex));
	if (!SaveGame)
	{
		FRONTIER_LOG(Warning, TEXT("Failed to load local user settings. Using defaults."));
		return false;
	}

	MouseSensitivity = FMath::Clamp(
		SaveGame->MouseSensitivity,
		MinimumMouseSensitivity,
		MaximumMouseSensitivity);
	return true;
}
