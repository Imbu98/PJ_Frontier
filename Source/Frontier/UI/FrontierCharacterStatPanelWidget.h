#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayEffectTypes.h"
#include "FrontierCharacterStatPanelWidget.generated.h"

class AFrontierPlayerState;
class UFrontierAbilitySystemComponent;
class UFrontierAttributeSet;
class UTextBlock;

UCLASS()
class FRONTIER_API UFrontierCharacterStatPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetObservedPlayerState(AFrontierPlayerState* InPlayerState);
	void RefreshStats();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void BindAttributeDelegates();
	void UnbindAttributeDelegates();
	
	void HandleAttributeChanged(const FOnAttributeChangeData& ChangeData);
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_AttackPower;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_HP;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_Defense;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierPlayerState> ObservedPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierAbilitySystemComponent> CachedAbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierAttributeSet> CachedAttributeSet;

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FDelegateHandle AttackPowerChangedHandle;
	FDelegateHandle DefenseChangedHandle;
};
