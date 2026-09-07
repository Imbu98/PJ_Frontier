#include "UI/FrontierCharacterStatPanelWidget.h"

#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "AbilitySystem/FrontierAttributeSet.h"
#include "Components/TextBlock.h"
#include "Game/FrontierPlayerState.h"

void UFrontierCharacterStatPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	BindAttributeDelegates();
	RefreshStats();
}

void UFrontierCharacterStatPanelWidget::NativeDestruct()
{
	UnbindAttributeDelegates();
	Super::NativeDestruct();
}

void UFrontierCharacterStatPanelWidget::SetObservedPlayerState(AFrontierPlayerState* InPlayerState)
{
	if (ObservedPlayerState == InPlayerState)
	{
		RefreshStats();
		return;
	}

	UnbindAttributeDelegates();
	ObservedPlayerState = InPlayerState;
	BindAttributeDelegates();
	RefreshStats();
}

void UFrontierCharacterStatPanelWidget::RefreshStats()
{
	if (!CachedAttributeSet)
	{
		if (Text_HP)
		{
			Text_HP->SetText(FText::FromString(TEXT("HP: -")));
		}
		if (Text_AttackPower)
		{
			Text_AttackPower->SetText(FText::FromString(TEXT("Attack: -")));
		}
		if (Text_Defense)
		{
			Text_Defense->SetText(FText::FromString(TEXT("Defense: -")));
		}
		return;
	}

	if (Text_HP)
	{
		Text_HP->SetText(FText::FromString(FString::Printf(
			TEXT("HP: %.0f / %.0f"),
			CachedAttributeSet->GetHealth(),
			CachedAttributeSet->GetMaxHealth())));
	}
	if (Text_AttackPower)
	{
		Text_AttackPower->SetText(FText::FromString(FString::Printf(
			TEXT("Attack: %.0f"),
			CachedAttributeSet->GetAttackPower())));
	}
	if (Text_Defense)
	{
		Text_Defense->SetText(FText::FromString(FString::Printf(
			TEXT("Defense: %.0f"),
			CachedAttributeSet->GetDefense())));
	}
}

void UFrontierCharacterStatPanelWidget::BindAttributeDelegates()
{
	if (!ObservedPlayerState)
	{
		ObservedPlayerState = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>() : nullptr;
	}

	CachedAbilitySystemComponent = ObservedPlayerState ? ObservedPlayerState->GetFrontierAbilitySystemComponent() : nullptr;
	CachedAttributeSet = ObservedPlayerState ? const_cast<UFrontierAttributeSet*>(ObservedPlayerState->GetFrontierAttributeSet()) : nullptr;
	if (!CachedAbilitySystemComponent || !CachedAttributeSet)
	{
		return;
	}

	HealthChangedHandle = CachedAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetHealthAttribute()).AddUObject(this, &UFrontierCharacterStatPanelWidget::HandleAttributeChanged);
	MaxHealthChangedHandle = CachedAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &UFrontierCharacterStatPanelWidget::HandleAttributeChanged);
	AttackPowerChangedHandle = CachedAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetAttackPowerAttribute()).AddUObject(this, &UFrontierCharacterStatPanelWidget::HandleAttributeChanged);
	DefenseChangedHandle = CachedAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetDefenseAttribute()).AddUObject(this, &UFrontierCharacterStatPanelWidget::HandleAttributeChanged);
}

void UFrontierCharacterStatPanelWidget::UnbindAttributeDelegates()
{
	if (CachedAbilitySystemComponent)
	{
		if (HealthChangedHandle.IsValid())
		{
			CachedAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		}
		if (MaxHealthChangedHandle.IsValid())
		{
			CachedAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
		}
		if (AttackPowerChangedHandle.IsValid())
		{
			CachedAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetAttackPowerAttribute()).Remove(AttackPowerChangedHandle);
		}
		if (DefenseChangedHandle.IsValid())
		{
			CachedAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetDefenseAttribute()).Remove(DefenseChangedHandle);
		}
	}

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	AttackPowerChangedHandle.Reset();
	DefenseChangedHandle.Reset();
	CachedAbilitySystemComponent = nullptr;
	CachedAttributeSet = nullptr;
}

void UFrontierCharacterStatPanelWidget::HandleAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshStats();
}
