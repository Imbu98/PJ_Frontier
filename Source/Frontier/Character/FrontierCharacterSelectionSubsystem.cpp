#include "Character/FrontierCharacterSelectionSubsystem.h"

#include "Character/FrontierCharacterAppearanceDataAsset.h"
#include "Character/FrontierCharacterAppearanceSettings.h"
#include "Frontier.h"
#include "Kismet/GameplayStatics.h"
#include "Save/FrontierCharacterSaveGame.h"

const FString UFrontierCharacterSelectionSubsystem::CharacterSaveSlotName =
	TEXT("FrontierCharacterSelection");

void UFrontierCharacterSelectionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	InitializeCharacterSelection();
}

void UFrontierCharacterSelectionSubsystem::InitializeCharacterSelection()
{
	if (bIsInitialized)
	{
		return;
	}

	bIsInitialized = true;
	LoadCharacterSelection();
}

bool UFrontierCharacterSelectionSubsystem::SetSelectedCharacterType(
	const EFrontierCharacterType NewCharacterType)
{
	InitializeCharacterSelection();

	const EFrontierCharacterType SafeType = IsValidFrontierCharacterType(NewCharacterType)
		? NewCharacterType
		: EFrontierCharacterType::DarkKnight;
	if (SelectedCharacterType == SafeType)
	{
		return true;
	}

	SelectedCharacterType = SafeType;
	const bool bSaved = SaveCharacterSelection();
	OnSelectedCharacterChanged.Broadcast(SelectedCharacterType);

	FRONTIER_LOG(
		Log,
		TEXT("Character selection changed. CharacterType=%s Saved=%d"),
		*UEnum::GetValueAsString(SelectedCharacterType),
		bSaved ? 1 : 0);
	return bSaved;
}

bool UFrontierCharacterSelectionSubsystem::SaveCharacterSelection()
{
	UFrontierCharacterSaveGame* SaveGame = Cast<UFrontierCharacterSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UFrontierCharacterSaveGame::StaticClass()));
	if (!SaveGame)
	{
		FRONTIER_LOG(Error, TEXT("Failed to create character selection SaveGame."));
		return false;
	}

	SaveGame->SelectedCharacterType = IsValidFrontierCharacterType(SelectedCharacterType)
		? SelectedCharacterType
		: EFrontierCharacterType::DarkKnight;
	const bool bSaved = UGameplayStatics::SaveGameToSlot(
		SaveGame,
		CharacterSaveSlotName,
		CharacterSaveUserIndex);
	if (!bSaved)
	{
		FRONTIER_LOG(Warning, TEXT("Failed to save character selection."));
	}
	return bSaved;
}

bool UFrontierCharacterSelectionSubsystem::LoadCharacterSelection()
{
	SelectedCharacterType = EFrontierCharacterType::DarkKnight;

	if (!UGameplayStatics::DoesSaveGameExist(CharacterSaveSlotName, CharacterSaveUserIndex))
	{
		FRONTIER_LOG(Log, TEXT("Character selection save does not exist. Using DarkKnight."));
		return SaveCharacterSelection();
	}

	const UFrontierCharacterSaveGame* SaveGame = Cast<UFrontierCharacterSaveGame>(
		UGameplayStatics::LoadGameFromSlot(CharacterSaveSlotName, CharacterSaveUserIndex));
	if (!SaveGame || !IsValidFrontierCharacterType(SaveGame->SelectedCharacterType))
	{
		FRONTIER_LOG(Warning, TEXT("Failed to load a valid character selection. Using DarkKnight."));
		return false;
	}

	SelectedCharacterType = SaveGame->SelectedCharacterType;
	FRONTIER_LOG(
		Log,
		TEXT("Character selection loaded. CharacterType=%s"),
		*UEnum::GetValueAsString(SelectedCharacterType));
	return true;
}

const FFrontierCharacterAppearanceData*
UFrontierCharacterSelectionSubsystem::GetSelectedAppearance() const
{
	return FindAppearance(SelectedCharacterType);
}

const FFrontierCharacterAppearanceData*
UFrontierCharacterSelectionSubsystem::FindAppearance(
	const EFrontierCharacterType CharacterType) const
{
	const UFrontierCharacterAppearanceDataAsset* AppearanceData = GetAppearanceDataAsset();
	if (!AppearanceData)
	{
		return nullptr;
	}

	const EFrontierCharacterType SafeType = IsValidFrontierCharacterType(CharacterType)
		? CharacterType
		: EFrontierCharacterType::DarkKnight;
	return AppearanceData->FindAppearance(SafeType);
}

UFrontierCharacterAppearanceDataAsset*
UFrontierCharacterSelectionSubsystem::GetAppearanceDataAsset() const
{
	if (CachedAppearanceData)
	{
		return CachedAppearanceData;
	}

	const UFrontierCharacterAppearanceSettings* Settings =
		GetDefault<UFrontierCharacterAppearanceSettings>();
	if (!Settings || Settings->CharacterAppearanceData.IsNull())
	{
		FRONTIER_LOG(
			Warning,
			TEXT("CharacterAppearanceData is not configured in Project Settings > Game > Frontier Character Appearance."));
		return nullptr;
	}

	UFrontierCharacterSelectionSubsystem* MutableThis =
		const_cast<UFrontierCharacterSelectionSubsystem*>(this);
	MutableThis->CachedAppearanceData = Settings->CharacterAppearanceData.LoadSynchronous();
	if (!MutableThis->CachedAppearanceData)
	{
		FRONTIER_LOG(
			Error,
			TEXT("Failed to load CharacterAppearanceData. Asset=%s"),
			*Settings->CharacterAppearanceData.ToSoftObjectPath().ToString());
	}
	return MutableThis->CachedAppearanceData;
}
