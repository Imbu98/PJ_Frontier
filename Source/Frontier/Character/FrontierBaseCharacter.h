#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/FrontierHitReactionTypes.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Character.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Loot/FrontierLootContainerActor.h"
#include "FrontierBaseCharacter.generated.h"

class UFrontierAbilitySystemComponent;
class AFrontierDeathLootContainerActor;
class UFrontierAttributeSet;
class UFrontierCombatComponent;
class UFrontierLootComponent;
class UFrontierStorageComponent;
class UFrontierRaidInventoryComponent;
class UFrontierLoadoutComponent;
class UAnimMontage;
class UStaticMesh;

USTRUCT(BlueprintType)
struct FFrontierHitReactMontageEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Animation")
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Animation")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|HitReaction")
	FGameplayTag HitReactionTag;
};

UENUM(BlueprintType)
enum class EFrontierTeam : uint8
{
	Monster = 0,
	TeamA = 1,
	TeamB = 2,
	TeamC = 3,
	None = 255
};

UCLASS()
class FRONTIER_API AFrontierBaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFrontierBaseCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

	UFUNCTION(BlueprintPure, Category="Frontier")
	UFrontierAbilitySystemComponent* GetFrontierAbilitySystemComponent() const;

	UFUNCTION(BlueprintPure, Category="Frontier")
	const UFrontierAttributeSet* GetFrontierAttributeSet() const;

	UFUNCTION(BlueprintPure, Category="Frontier")
	UFrontierAttributeSet* GetMutableFrontierAttributeSet() const;

	UFUNCTION(BlueprintPure, Category="Frontier")
	UFrontierCombatComponent* GetCombatComponent() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Inventory")
	UFrontierStorageComponent* GetStorageComponent() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Inventory")
	UFrontierRaidInventoryComponent* GetRaidInventoryComponent() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Inventory")
	UFrontierLoadoutComponent* GetLoadoutComponent() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Team")
	EFrontierTeam GetTeam() const;

	UFUNCTION(BlueprintCallable, Category="Frontier|Team")
	void SetTeam(EFrontierTeam NewTeam);

	static EFrontierTeam ResolvePlayerTeam(int32 TeamId);

	UFUNCTION(BlueprintPure, Category="Frontier|State")
	bool IsDead() const;

	/** Client-local combat UI hook. Called only for the player who damaged this character. */
	virtual void RevealCombatOverheadLocally(float HealthPercent);

	UFUNCTION(BlueprintPure, Category="Frontier|State")
	bool IsAiming() const { return bIsAiming; }

	UFUNCTION(BlueprintCallable, Category="Frontier|State")
	void SetAiming(bool bNewIsAiming);

	UFUNCTION(BlueprintCallable, Category="Frontier|State")
	virtual void Die();

	UFUNCTION(BlueprintCallable, Category="Frontier|Animation")
	virtual void HandleDamageReceived(float DamageAmount, FGameplayTag DamageTypeTag);

	UFUNCTION(BlueprintCallable, Category="Frontier|Animation")
	virtual void HandleDamageReceivedWithReaction(
		float DamageAmount,
		FGameplayTag DamageTypeTag,
		FGameplayTag HitReactionTag,
		EFrontierHitReactionLevel HitReactionLevel,
		float StaggerDuration,
		float KnockbackHorizontalStrength,
		float KnockbackVerticalStrength,
		FVector DamageOrigin);

	UFUNCTION(BlueprintPure, Category="Frontier|HitReaction")
	virtual EFrontierHitReactionLevel GetCurrentHitReactionResistance() const;

	UFUNCTION(BlueprintPure, Category="Frontier|HitReaction")
	virtual bool IsDamageReactionBlocked() const;

	UFUNCTION(BlueprintCallable, Category="Frontier|HitReaction")
	void SetMontageHitReactionResistance(UAnimMontage* Montage, EFrontierHitReactionLevel ResistanceLevel);

	UFUNCTION(BlueprintCallable, Category="Frontier|HitReaction")
	void ClearMontageHitReactionResistance(UAnimMontage* ExpectedMontage = nullptr);

	void RecordLastDamageReaction(
		FGameplayTag HitReactionTag,
		EFrontierHitReactionLevel HitReactionLevel,
		float StaggerDuration,
		float KnockbackHorizontalStrength,
		float KnockbackVerticalStrength,
		const FVector& DamageOrigin);

	void RecordLastDamageImpactPoint(const FVector& DamageImpactPoint, bool bIsValidImpactPoint);
	void RecordLastDamageSource(const AActor* DamageSource);

	UFUNCTION(BlueprintCallable, Category="Frontier|Animation")
	void PlayCosmeticHitReaction(FGameplayTag DamageTypeTag);

	UFUNCTION(BlueprintPure, Category="Frontier|State")
	float GetLastDamageReceivedTime() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Movement")
	float GetEffectiveMovementSpeedMultiplier() const;

	void SetMovementSpeedModifier(FName SourceId, float Multiplier);
	void RemoveMovementSpeedModifier(FName SourceId);
	void SetDesiredMaxWalkSpeed(float NewBaseSpeed);

	UFUNCTION(BlueprintPure, Category="Frontier|Damage")
	EFrontierElementalType GetElementalType() const;

protected:
	virtual void BeginPlay() override;

	virtual void InitializeAbilityActorInfo();

	UFUNCTION()
	virtual void OnRep_IsDead();

	UFUNCTION()
	void OnRep_IsAiming();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayHitReactMontage(UAnimMontage* HitReactMontage);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastApplyDeathRagdollImpulse(FVector Impulse);

	virtual void HandleDeathStateChanged(bool bWasDead);
	virtual void HandleAimingStateChanged();
	virtual void HandleMovementSpeedModifiersChanged();

	UFUNCTION()
	void OnRep_StatusMovementSpeedMultiplier();
	virtual AFrontierLootContainerActor* SpawnDeathLootContainer();
	virtual void CreateDeathLootSlots(TArray<FFrontierInventorySlot>& OutLootSlots) const;
	virtual void CreateDeathLoadoutSlots(TArray<FFrontierLoadoutSlot>& OutLoadoutSlots) const;
	virtual EFrontierLootContainerSourceType GetDeathLootContainerSourceType() const;
	virtual UAnimMontage* SelectHitReactMontage(FGameplayTag DamageTypeTag) const;
	const FFrontierHitReactMontageEntry* FindHitReactMontageEntry(FGameplayTag HitReactionTag) const;
	UAnimMontage* SelectHitReactionMontage(FGameplayTag HitReactionTag, FGameplayTag DamageTypeTag) const;
	void ApplyHitReactionMovement(
		EFrontierHitReactionLevel HitReactionLevel,
		float StaggerDuration,
		float KnockbackHorizontalStrength,
		float KnockbackVerticalStrength,
		const FVector& DamageOrigin);
	void ClearHitReactionStun();
	void EnableDeathRagdoll();
	FVector CalculateDeathRagdollImpulse() const;
	virtual bool UsesAlternativeDeathPresentation() const;
	virtual void ActivateAlternativeDeathPresentation();
	void HideCharacterForDeathReplacement();
	void ApplyDefaultHealthAttributes();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier")
	TObjectPtr<UFrontierAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier")
	TObjectPtr<UFrontierAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier")
	TObjectPtr<UFrontierCombatComponent> CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Loot")
	TObjectPtr<UFrontierLootComponent> LootComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Team")
	EFrontierTeam Team = EFrontierTeam::None;

	UPROPERTY(ReplicatedUsing=OnRep_StatusMovementSpeedMultiplier, VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Movement")
	float StatusMovementSpeedMultiplier = 1.0f;

	TMap<FName, float> MovementSpeedModifiers;
	float DesiredBaseMaxWalkSpeed = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Animation")
	TObjectPtr<UAnimMontage> DefaultHitReactMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Animation")
	TArray<FFrontierHitReactMontageEntry> HitReactMontagesByDamageType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Damage")
	EFrontierElementalType ElementalType = EFrontierElementalType::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Attributes", meta=(ClampMin="1.0"))
	float DefaultMaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Attributes", meta=(ClampMin="0.0"))
	float DefaultHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Loot")
	TSubclassOf<AFrontierDeathLootContainerActor> DeathLootContainerClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Death", meta=(ClampMin="0.0"))
	float DeathDestroyDelay = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Death", meta=(ClampMin="0.1"))
	float RagdollImpactWeight = 1.0f;

	UPROPERTY(ReplicatedUsing=OnRep_IsDead, VisibleAnywhere, BlueprintReadOnly, Category="Frontier|State")
	bool bIsDead = false;

	UPROPERTY(ReplicatedUsing=OnRep_IsAiming, VisibleAnywhere, BlueprintReadOnly, Category="Frontier|State")
	bool bIsAiming = false;

	UPROPERTY(Transient)
	float LastDamageReceivedTime = -1000.0f;

	UPROPERTY(Transient)
	bool bDefaultHealthAttributesApplied = false;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ResistanceMontage = nullptr;

	UPROPERTY(Transient)
	EFrontierHitReactionLevel MontageHitReactionResistance = EFrontierHitReactionLevel::None;

	UPROPERTY(Transient)
	FGameplayTag LastDamageHitReactionTag;

	UPROPERTY(Transient)
	EFrontierHitReactionLevel LastDamageHitReactionLevel = EFrontierHitReactionLevel::None;

	UPROPERTY(Transient)
	float LastDamageStaggerDuration = 0.35f;

	UPROPERTY(Transient)
	float LastDamageKnockbackHorizontalStrength = 650.0f;

	UPROPERTY(Transient)
	float LastDamageKnockbackVerticalStrength = 150.0f;

	UPROPERTY(Transient)
	FVector LastDamageOrigin = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector LastDamageImpactPoint = FVector::ZeroVector;

	UPROPERTY(Transient)
	bool bHasLastDamageImpactPoint = false;

	UPROPERTY(Transient)
	bool bLastDamageWasFromPlayer = false;

	UPROPERTY(Transient)
	bool bHasLastDamageReaction = false;

	FTimerHandle HitReactionStunTimerHandle;
};


