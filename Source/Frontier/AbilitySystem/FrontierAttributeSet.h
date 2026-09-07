#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "FrontierAttributeSet.generated.h"

#define FRONTIER_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class FRONTIER_API UFrontierAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UFrontierAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void GetReplicatedCustomConditionState(FCustomPropertyConditionState& OutActiveState) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Attributes")
	FGameplayAttributeData Health;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="Attributes")
	FGameplayAttributeData MaxHealth;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AttackPower, Category="Attributes")
	FGameplayAttributeData AttackPower;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, AttackPower)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Defense, Category="Attributes")
	FGameplayAttributeData Defense;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, Defense)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_FireAttackPower, Category="Attributes|Elemental")
	FGameplayAttributeData FireAttackPower;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, FireAttackPower)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_IceAttackPower, Category="Attributes|Elemental")
	FGameplayAttributeData IceAttackPower;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, IceAttackPower)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_LightningAttackPower, Category="Attributes|Elemental")
	FGameplayAttributeData LightningAttackPower;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, LightningAttackPower)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_PoisonAttackPower, Category="Attributes|Elemental")
	FGameplayAttributeData PoisonAttackPower;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, PoisonAttackPower)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_FireResistance, Category="Attributes|Elemental")
	FGameplayAttributeData FireResistance;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, FireResistance)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_IceResistance, Category="Attributes|Elemental")
	FGameplayAttributeData IceResistance;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, IceResistance)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_LightningResistance, Category="Attributes|Elemental")
	FGameplayAttributeData LightningResistance;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, LightningResistance)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_PoisonResistance, Category="Attributes|Elemental")
	FGameplayAttributeData PoisonResistance;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, PoisonResistance)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_SwordAttackPower, Category="Attributes|Passive")
	FGameplayAttributeData SwordAttackPower;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, SwordAttackPower)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AxeAttackPower, Category="Attributes|Passive")
	FGameplayAttributeData AxeAttackPower;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, AxeAttackPower)

	/** Additive ratio: 0.10 means sword attack montages and recovery are 10% faster. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_SwordAttackSpeedBonus, Category="Attributes|Passive")
	FGameplayAttributeData SwordAttackSpeedBonus;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, SwordAttackSpeedBonus)

	/** Additive ratio: 0.10 means axe attack montages and recovery are 10% faster. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AxeAttackSpeedBonus, Category="Attributes|Passive")
	FGameplayAttributeData AxeAttackSpeedBonus;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, AxeAttackSpeedBonus)

	/** Additive ratio: 0.10 means +10% to both walk and sprint speed. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MoveSpeedBonus, Category="Attributes|Passive")
	FGameplayAttributeData MoveSpeedBonus;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, MoveSpeedBonus)

	/** Additive ratio: 0.10 means +10% to JumpZVelocity. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_JumpPowerBonus, Category="Attributes|Passive")
	FGameplayAttributeData JumpPowerBonus;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, JumpPowerBonus)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_LobbyStorageSlotBonus, Category="Attributes|Passive")
	FGameplayAttributeData LobbyStorageSlotBonus;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, LobbyStorageSlotBonus)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_RaidInventorySlotBonus, Category="Attributes|Passive")
	FGameplayAttributeData RaidInventorySlotBonus;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, RaidInventorySlotBonus)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Stamina, Category="Attributes")
	FGameplayAttributeData Stamina;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, Stamina)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxStamina, Category="Attributes")
	FGameplayAttributeData MaxStamina;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, MaxStamina)

	UPROPERTY(BlueprintReadOnly, Category="Attributes")
	FGameplayAttributeData Damage;
	FRONTIER_ATTRIBUTE_ACCESSORS(UFrontierAttributeSet, Damage)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Defense(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_FireAttackPower(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_IceAttackPower(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_LightningAttackPower(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_PoisonAttackPower(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_FireResistance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_IceResistance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_LightningResistance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_PoisonResistance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_SwordAttackPower(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_AxeAttackPower(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_SwordAttackSpeedBonus(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_AxeAttackSpeedBonus(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MoveSpeedBonus(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_JumpPowerBonus(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_LobbyStorageSlotBonus(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_RaidInventorySlotBonus(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);

private:
	void ClampHealth();
	void ClampStamina();
};

