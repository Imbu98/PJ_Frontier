#include "UI/FrontierEnemyHealthWidget.h"

#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "AbilitySystem/FrontierAttributeSet.h"
#include "Blueprint/WidgetTree.h"
#include "Character/FrontierEnemyCharacter.h"
#include "Components/ProgressBar.h"
#include "Frontier.h"

void UFrontierEnemyHealthWidget::NativeConstruct()
{
	
	Super::NativeConstruct();
	//BuildWidgetTreeIfNeeded();
}

void UFrontierEnemyHealthWidget::NativeDestruct()
{
	
	UnbindFromEnemy();
	Super::NativeDestruct();
}

void UFrontierEnemyHealthWidget::BindToEnemy(AFrontierEnemyCharacter* EnemyCharacter)
{
	FRONTIER_LOG(Log, TEXT("Binding enemy health widget. Enemy=%s"), *GetNameSafe(EnemyCharacter));
	UnbindFromEnemy();

	CachedEnemyCharacter = EnemyCharacter;
	CachedAbilitySystemComponent = CachedEnemyCharacter ? CachedEnemyCharacter->GetFrontierAbilitySystemComponent() : nullptr;
	CachedAttributeSet = CachedEnemyCharacter
		? const_cast<UFrontierAttributeSet*>(CachedEnemyCharacter->GetFrontierAttributeSet())
		: nullptr;

	if (!CachedAbilitySystemComponent || !CachedAttributeSet)
	{
		FRONTIER_LOG(Warning, TEXT("Enemy health widget binding failed because ASC or AttributeSet is unavailable. Enemy=%s"), *GetNameSafe(EnemyCharacter));
		return;
	}

	HealthChangedHandle = CachedAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UFrontierEnemyHealthWidget::HandleHealthChanged);
	MaxHealthChangedHandle = CachedAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &UFrontierEnemyHealthWidget::HandleMaxHealthChanged);

	RefreshFromCachedAttributes();
}

void UFrontierEnemyHealthWidget::BuildWidgetTreeIfNeeded()
{
	

	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	HPBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HPBar"));
	HPBar->SetPercent(1.0f);
	HPBar->SetFillColorAndOpacity(FLinearColor(0.8f, 0.05f, 0.02f, 1.0f));
	WidgetTree->RootWidget = HPBar;
}

void UFrontierEnemyHealthWidget::UnbindFromEnemy()
{
	

	if (CachedAbilitySystemComponent)
	{
		if (HealthChangedHandle.IsValid())
		{
			CachedAbilitySystemComponent
				->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetHealthAttribute())
				.Remove(HealthChangedHandle);
		}

		if (MaxHealthChangedHandle.IsValid())
		{
			CachedAbilitySystemComponent
				->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetMaxHealthAttribute())
				.Remove(MaxHealthChangedHandle);
		}
	}

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	CachedEnemyCharacter = nullptr;
	CachedAbilitySystemComponent = nullptr;
	CachedAttributeSet = nullptr;
}

void UFrontierEnemyHealthWidget::RefreshFromCachedAttributes()
{
	

	if (!HPBar || !CachedEnemyCharacter || !CachedAttributeSet)
	{
		return;
	}

	const float Health = CachedAttributeSet->GetHealth();
	const float MaxHealth = FMath::Max(CachedAttributeSet->GetMaxHealth(), 1.0f);
	const float HealthPercent = FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f);

	HPBar->SetPercent(HealthPercent);
	SetVisibility(Health > 0.0f && !CachedEnemyCharacter->IsDead()
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
}

void UFrontierEnemyHealthWidget::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshFromCachedAttributes();
}

void UFrontierEnemyHealthWidget::HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshFromCachedAttributes();
}
