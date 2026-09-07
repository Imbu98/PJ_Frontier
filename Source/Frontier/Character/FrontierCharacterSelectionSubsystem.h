#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Character/FrontierCharacterTypes.h"
#include "FrontierCharacterSelectionSubsystem.generated.h"

class UFrontierCharacterAppearanceDataAsset;
struct FFrontierCharacterAppearanceData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FFrontierSelectedCharacterChangedSignature,
	EFrontierCharacterType,
	CharacterType);

UCLASS()
class FRONTIER_API UFrontierCharacterSelectionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// This subsystem is the persistence boundary. Replace its SaveGame calls with the
	// Backend profile API when character selection becomes server-authoritative.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category="Frontier|Character")
	void InitializeCharacterSelection();

	UFUNCTION(BlueprintCallable, Category="Frontier|Character")
	bool SetSelectedCharacterType(EFrontierCharacterType NewCharacterType);

	UFUNCTION(BlueprintPure, Category="Frontier|Character")
	EFrontierCharacterType GetSelectedCharacterType() const { return SelectedCharacterType; }

	UFUNCTION(BlueprintCallable, Category="Frontier|Character")
	bool SaveCharacterSelection();

	UFUNCTION(BlueprintCallable, Category="Frontier|Character")
	bool LoadCharacterSelection();

	const FFrontierCharacterAppearanceData* GetSelectedAppearance() const;
	const FFrontierCharacterAppearanceData* FindAppearance(EFrontierCharacterType CharacterType) const;

	UFUNCTION(BlueprintPure, Category="Frontier|Character")
	UFrontierCharacterAppearanceDataAsset* GetAppearanceDataAsset() const;

	UPROPERTY(BlueprintAssignable, Category="Frontier|Character")
	FFrontierSelectedCharacterChangedSignature OnSelectedCharacterChanged;

	static const FString CharacterSaveSlotName;
	static constexpr int32 CharacterSaveUserIndex = 0;

private:
	UPROPERTY(Transient)
	EFrontierCharacterType SelectedCharacterType = EFrontierCharacterType::DarkKnight;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierCharacterAppearanceDataAsset> CachedAppearanceData;

	bool bIsInitialized = false;
};
