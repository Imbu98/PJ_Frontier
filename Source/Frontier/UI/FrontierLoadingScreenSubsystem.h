#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FrontierLoadingScreenSubsystem.generated.h"

class UUserWidget;

UCLASS()
class FRONTIER_API UFrontierLoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	void StartRaidLoadingScreen();
	void RecreateRaidLoadingScreen();
	void StopRaidLoadingScreen();
	void SetRaidLoadingProgress(float InProgress, const FText& InStatusMessage);
	bool IsRaidLoadingScreenActive() const { return bRaidLoadingScreenActive; }

private:
	void CreateRaidLoadingWidget();

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LoadingScreenUserWidget;

	float RaidLoadingProgress = 0.0f;
	FText RaidLoadingStatusMessage = FText::FromString(TEXT("Loading..."));

	bool bRaidLoadingScreenActive = false;
};
