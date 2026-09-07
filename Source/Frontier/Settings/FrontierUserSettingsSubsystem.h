#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FrontierUserSettingsSubsystem.generated.h"

UCLASS()
class FRONTIER_API UFrontierUserSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category="Frontier|Settings")
	void SetMouseSensitivity(float NewSensitivity);

	UFUNCTION(BlueprintPure, Category="Frontier|Settings")
	float GetMouseSensitivity() const { return MouseSensitivity; }

	UFUNCTION(BlueprintPure, Category="Frontier|Settings")
	float GetMouseSensitivityScale() const;

	UFUNCTION(BlueprintCallable, Category="Frontier|Settings")
	bool SaveSettings();

	UFUNCTION(BlueprintCallable, Category="Frontier|Settings")
	bool LoadSettings();

	static constexpr float MinimumMouseSensitivity = 0.0f;
	static constexpr float MaximumMouseSensitivity = 100.0f;
	static constexpr float DefaultMouseSensitivity = 50.0f;

private:
	static const FString SettingsSaveSlotName;
	static constexpr int32 SettingsSaveUserIndex = 0;

	UPROPERTY(Transient)
	float MouseSensitivity = DefaultMouseSensitivity;

	bool bSettingsDirty = false;
};
