#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierEnemyHealthWidget.generated.h"

class AFrontierEnemyCharacter;
class UFrontierAbilitySystemComponent;
class UFrontierAttributeSet;
class UProgressBar;
struct FOnAttributeChangeData;

UCLASS()
class FRONTIER_API UFrontierEnemyHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void BindToEnemy(AFrontierEnemyCharacter* EnemyCharacter);

private:
	void BuildWidgetTreeIfNeeded();
	void UnbindFromEnemy();
	void RefreshFromCachedAttributes();
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData);

	UPROPERTY(Transient)
	TObjectPtr<AFrontierEnemyCharacter> CachedEnemyCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierAbilitySystemComponent> CachedAbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierAttributeSet> CachedAttributeSet;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> HPBar;

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
};
