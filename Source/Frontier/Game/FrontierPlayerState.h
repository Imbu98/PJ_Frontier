#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "FrontierPlayerState.generated.h"

struct FOnAttributeChangeData;
class UFrontierAbilitySystemComponent;
class UFrontierAttributeSet;
class UGameplayAbility;
class UFrontierStorageComponent;
class UFrontierRaidInventoryComponent;
class UFrontierLoadoutComponent;
class UFrontierEquipmentSkillComponent;
class UFrontierSkillTreeComponent;
class UFrontierQuickSlotComponent;
class UFrontierItemDataAsset;

DECLARE_MULTICAST_DELEGATE_TwoParams(FFrontierPlayerTeamChangedSignature, AFrontierPlayerState* /*PlayerState*/, int32 /*TeamId*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FFrontierPlayerVitalsChangedSignature, AFrontierPlayerState* /*PlayerState*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FFrontierPlayerReadyChangedSignature, AFrontierPlayerState* /*PlayerState*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FFrontierPlayerNameChangedSignature, AFrontierPlayerState* /*PlayerState*/);

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierPublicVitalsSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Team")
	float Health = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Team")
	float MaxHealth = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Team")
	float Stamina = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Team")
	float MaxStamina = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Team")
	bool bIsDead = false;

	bool IsEquivalentTo(const FFrontierPublicVitalsSnapshot& Other) const
	{
		return FMath::IsNearlyEqual(Health, Other.Health)
			&& FMath::IsNearlyEqual(MaxHealth, Other.MaxHealth)
			&& FMath::IsNearlyEqual(Stamina, Other.Stamina)
			&& FMath::IsNearlyEqual(MaxStamina, Other.MaxStamina)
			&& bIsDead == Other.bIsDead;
	}
};

UCLASS()
class FRONTIER_API AFrontierPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFrontierPlayerState();
	virtual void SetPlayerName(const FString& S) override;
	virtual void CopyProperties(APlayerState* PlayerState) override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category="Frontier|AbilitySystem")
	UFrontierAbilitySystemComponent* GetFrontierAbilitySystemComponent() const;

	UFUNCTION(BlueprintPure, Category="Frontier|AbilitySystem")
	const UFrontierAttributeSet* GetFrontierAttributeSet() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Inventory")
	UFrontierStorageComponent* GetStorageComponent() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Inventory")
	UFrontierRaidInventoryComponent* GetRaidInventoryComponent() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Inventory")
	UFrontierLoadoutComponent* GetLoadoutComponent() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Skill")
	UFrontierEquipmentSkillComponent* GetEquipmentSkillComponent() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree")
	UFrontierSkillTreeComponent* GetSkillTreeComponent() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Quick Slot")
	UFrontierQuickSlotComponent* GetQuickSlotComponent() const;

	/** Authoritative hook for any level system. Each gained level awards the configured skill points. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Frontier|Progression")
	bool GrantSkillPointsForLevelUps(int32 LevelsGained = 1);

	/** Server-only identity established by Backend auth/join authorization; never inferred from display names. */
	void SetBackendIdentity(const FString& InBackendPlayerId, const FString& InSteamId);
	const FString& GetBackendPlayerId() const { return BackendPlayerId; }
	int64 GetBackendUserId() const;
	const FString& GetVerifiedSteamId() const { return VerifiedSteamId; }

	UFUNCTION(BlueprintPure, Category="Frontier|Team")
	int32 GetTeamId() const;

	UFUNCTION(BlueprintCallable, Category="Frontier|Team")
	void SetTeamId(int32 NewTeamId);

	UFUNCTION(BlueprintPure, Category="Frontier|Team")
	FFrontierPublicVitalsSnapshot GetPublicVitalsSnapshot() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Team")
	float GetDisplayHealth() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Team")
	float GetDisplayMaxHealth() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Team")
	float GetDisplayStamina() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Team")
	float GetDisplayMaxStamina() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Team")
	bool IsDisplayDead() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Lobby")
	bool IsRaidReady() const;

	UFUNCTION(BlueprintCallable, Category="Frontier|Lobby")
	void SetRaidReady(bool bNewRaidReady);

	void ApplyPredictedRaidReady(bool bNewRaidReady);

	UFUNCTION(BlueprintCallable, Category="Frontier|Persistence")
	void LoseRaidItemsOnDeath();

	FFrontierPlayerTeamChangedSignature OnTeamIdChanged;
	FFrontierPlayerVitalsChangedSignature OnVitalsChanged;
	FFrontierPlayerReadyChangedSignature OnReadyChanged;
	FFrontierPlayerNameChangedSignature OnPlayerNameChanged;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRep_PlayerName() override;

	void GrantStartupAbilities();
	void GrantDebugStartingItems();
	void BindAttributeDelegates();
	void SyncVitalsFromAttributes();
	void SetDisplayVitals(float NewHealth, float NewMaxHealth, float NewStamina, float NewMaxStamina, bool bNewDead);
	void UpdateLegacyVitalsMirrors();
	void HandleAttributeChanged(const FOnAttributeChangeData& ChangeData);

	UFUNCTION()
	void OnRep_TeamId(int32 PreviousTeamId);

	UFUNCTION()
	void OnRep_PublicVitalsSnapshot();

	UFUNCTION()
	void OnRep_RaidReady();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|AbilitySystem")
	TObjectPtr<UFrontierAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|AbilitySystem")
	TObjectPtr<UFrontierAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Inventory")
	TObjectPtr<UFrontierStorageComponent> StorageComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Inventory")
	TObjectPtr<UFrontierRaidInventoryComponent> RaidInventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Inventory")
	TObjectPtr<UFrontierLoadoutComponent> LoadoutComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Skill")
	TObjectPtr<UFrontierEquipmentSkillComponent> EquipmentSkillComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Skill Tree")
	TObjectPtr<UFrontierSkillTreeComponent> SkillTreeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Quick Slot")
	TObjectPtr<UFrontierQuickSlotComponent> QuickSlotComponent;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Frontier|Backend")
	FString BackendPlayerId;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Frontier|Backend")
	FString VerifiedSteamId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|AbilitySystem")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Inventory|Debug")
	TArray<TObjectPtr<UFrontierItemDataAsset>> DebugStartingRaidItemTemplates;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Inventory|Debug")
	TArray<TObjectPtr<UFrontierItemDataAsset>> DebugStartingLoadoutItemTemplates;

	UPROPERTY(ReplicatedUsing=OnRep_TeamId, VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Team")
	int32 TeamId = 0;

	UPROPERTY(ReplicatedUsing=OnRep_PublicVitalsSnapshot, VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Team")
	FFrontierPublicVitalsSnapshot PublicVitalsSnapshot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Team", meta=(DeprecatedProperty, DeprecationMessage="Use PublicVitalsSnapshot.Health instead."))
	float DisplayHealth = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Team", meta=(DeprecatedProperty, DeprecationMessage="Use PublicVitalsSnapshot.MaxHealth instead."))
	float DisplayMaxHealth = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Team", meta=(DeprecatedProperty, DeprecationMessage="Use PublicVitalsSnapshot.Stamina instead."))
	float DisplayStamina = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Team", meta=(DeprecatedProperty, DeprecationMessage="Use PublicVitalsSnapshot.MaxStamina instead."))
	float DisplayMaxStamina = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Team", meta=(DeprecatedProperty, DeprecationMessage="Use PublicVitalsSnapshot.bIsDead instead."))
	bool bDisplayDead = false;

	UPROPERTY(ReplicatedUsing=OnRep_RaidReady, VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Lobby")
	bool bRaidReady = false;

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FDelegateHandle StaminaChangedHandle;
	FDelegateHandle MaxStaminaChangedHandle;
};
