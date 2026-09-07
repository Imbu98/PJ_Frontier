#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierOverheadPlayerNameWidget.generated.h"

class AFrontierPlayerState;
class UProgressBar;
class UTextBlock;

UCLASS()
class FRONTIER_API UFrontierOverheadPlayerNameWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	void SetObservedPlayerState(AFrontierPlayerState* InPlayerState);
	void SetEnemyPresentation(bool bInEnemyPresentation);
	void SetHealthPercent(float HealthPercent);

private:
	void RefreshFromPlayerState();
	void HandlePlayerNameChanged(AFrontierPlayerState* PlayerState);

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> HPBar;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierPlayerState> ObservedPlayerState;

	bool bEnemyPresentation = false;
};
