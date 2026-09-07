#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierTeamMemberWidget.generated.h"

class AFrontierPlayerState;
class UProgressBar;
class UTextBlock;
class UVerticalBox;

UCLASS()
class FRONTIER_API UFrontierTeamMemberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void SetObservedPlayerState(AFrontierPlayerState* PlayerState);
	AFrontierPlayerState* GetObservedPlayerState() const;
	void RefreshFromPlayerState();

private:
	void BuildWidgetTreeIfNeeded();
	void UnbindObservedPlayerState();
	void HandleObservedVitalsChanged(AFrontierPlayerState* PlayerState);
	void HandleObservedTeamChanged(AFrontierPlayerState* PlayerState, int32 TeamId);
	void HandleObservedPlayerNameChanged(AFrontierPlayerState* PlayerState);

	UPROPERTY(Transient)
	TObjectPtr<AFrontierPlayerState> ObservedPlayerState;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StaminaText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> StaminaBar;
};
