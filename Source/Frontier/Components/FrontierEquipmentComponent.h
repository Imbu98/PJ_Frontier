#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayAbilitySpec.h"
#include "Components/ActorComponent.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "FrontierEquipmentComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFrontierCurrentWeaponChangedSignature, AActor*, NewWeapon, int32, NewWeaponIndex);

USTRUCT(BlueprintType)
struct FFrontierReplicatedArmorVisual
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment|Armor")
	EFrontierEquipmentSlot SlotType = EFrontierEquipmentSlot::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment|Armor")
	EFrontierCharacterType CharacterType = EFrontierCharacterType::DarkKnight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment|Armor")
	FName ItemTemplateId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment|Armor")
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment|Armor")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment|Armor")
	FName AttachSocketName = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment|Armor")
	FTransform RelativeTransform = FTransform::Identity;
};

class AFrontierBaseCharacter;
class AFrontierWeaponBase;
class UAnimInstance;
class UAnimMontage;
class UFrontierWeaponDataAsset;
class UFrontierLoadoutComponent;
class UFrontierWeaponItemDataAsset;
class UGameplayEffect;
class UGameplayAbility;
struct FStreamableHandle;

USTRUCT()
struct FFrontierPreloadedAttackAssets
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> AttackMontage = nullptr;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(Transient)
	TArray<TSubclassOf<UGameplayEffect>> AdditionalHitEffectClasses;
};

UCLASS(ClassGroup=(Frontier), meta=(BlueprintSpawnableComponent))
class FRONTIER_API UFrontierEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFrontierEquipmentComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	

	UFUNCTION(BlueprintPure, Category="Equipment")
	AFrontierWeaponBase* GetCurrentWeapon() const;

	UFUNCTION(BlueprintPure, Category="Equipment")
	UFrontierWeaponDataAsset* GetCurrentWeaponData() const;

	FGameplayAbilitySpecHandle GetCurrentWeaponAttackAbilityHandle() const;

	UFUNCTION(BlueprintPure, Category="Equipment")
	EFrontierElementalType GetCurrentWeaponElementalType() const;

	UFUNCTION(BlueprintPure, Category="Equipment")
	int32 GetCurrentWeaponIndex() const;

	UFUNCTION(BlueprintPure, Category="Equipment")
	UFrontierWeaponItemDataAsset* GetCurrentWeaponItemData() const;

	UFUNCTION(BlueprintPure, Category="Equipment")
	float GetCurrentWeaponAttackPowerBonus() const;

	UFUNCTION(BlueprintPure, Category="Equipment")
	float GetTotalArmorDefenseBonus() const;

	UFUNCTION(BlueprintCallable, Category="Equipment")
	void RefreshFromLoadout();

	UFUNCTION(BlueprintCallable, Category="Equipment")
	bool SelectWeaponByLoadoutSlot(EFrontierEquipmentSlot SlotType);

	UFUNCTION(BlueprintCallable, Category="Equipment")
	void RefreshArmorVisuals();

	/** Reconnects the Animation Layer Interface implementation for the active weapon. */
	UFUNCTION(BlueprintCallable, Category="Equipment|Animation")
	void RefreshWeaponAnimationLayer();

	bool AreCurrentWeaponAttackAssetsReady() const;
	TSubclassOf<UGameplayAbility> GetPreloadedAttackAbilityClass() const;
	bool GetPreloadedAttackActionAssets(
		int32 ComboIndex,
		TObjectPtr<UAnimMontage>& OutAttackMontage,
		TSubclassOf<UGameplayEffect>& OutDamageEffectClass,
		TArray<TSubclassOf<UGameplayEffect>>& OutAdditionalHitEffectClasses) const;

	/** Refreshes local presentation after the current weapon's replicated data arrives. */
	void RefreshCurrentWeaponPresentation();

	UFUNCTION(BlueprintCallable, Category="Equipment")
	void GrantItemSkillsFromServerData(const FFrontierItemInstance& EquippedItem);

	UPROPERTY(BlueprintAssignable, Category="Equipment")
	FFrontierCurrentWeaponChangedSignature OnCurrentWeaponChanged;

protected:
	virtual void BeginPlay() override;
	
	UFUNCTION()
	void OnRep_OwnedWeapons();

	UFUNCTION()
	void OnRep_CurrentWeapon();

	UFUNCTION()
	void OnRep_ReplicatedArmorVisuals();

	UFUNCTION()
	void HandleLoadoutChanged(const TArray<FFrontierLoadoutSlot>& InSlots);

	void InitializeDefaultWeapons();
	void RebuildWeaponsFromLoadout();
	void DestroyOwnedWeapons();
	void EquipWeaponByIndex(int32 NewWeaponIndex);
	void RefreshWeaponVisualState();
	void RefreshWeaponAttackAbility();
	void PreloadCurrentWeaponAttackAssets();
	void CompletePreloadCurrentWeaponAttackAssets();
	void ApplyWeaponAnimationLayer();
	void RefreshWeaponAttackPowerEffect();
	void RefreshArmorDefenseEffect();
	void RefreshEquipmentStatsEffect();
	void RebuildReplicatedArmorVisuals();
	void RebuildArmorComponentsFromReplicatedVisuals();
	void ClearArmorVisuals();
	USkeletalMeshComponent* ResolveArmorAttachMesh(EFrontierEquipmentSlot SlotType) const;
	EFrontierCharacterType ResolveOwnerCharacterType() const;
	float GetCurrentWeaponStatValue(FGameplayTag StatTag) const;
	float GetTotalDefensiveStatValue(FGameplayTag StatTag) const;
	bool GetCurrentWeaponLoadoutSlot(FFrontierLoadoutSlot& OutLoadoutSlot) const;
	void AttachWeaponToCharacterMesh(AFrontierWeaponBase* Weapon, USkeletalMeshComponent* CharacterMesh) const;
	void BindLoadoutComponent();
	void UnbindLoadoutComponent();
	AFrontierWeaponBase* SpawnWeaponFromLoadoutSlot(const FFrontierLoadoutSlot& LoadoutSlot) const;
	bool HasUnarmedState() const;
	int32 GetNextWeaponIndex() const;
	AFrontierBaseCharacter* GetOwnerCharacter() const;
	UFrontierLoadoutComponent* GetLoadoutComponent() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment")
	TArray<TSubclassOf<AFrontierWeaponBase>> DefaultWeaponClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment")
	TObjectPtr<UFrontierWeaponDataAsset> UnarmedWeaponData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment")
	bool bStartUnarmed = true;

	UPROPERTY(ReplicatedUsing=OnRep_OwnedWeapons, VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment")
	TArray<TObjectPtr<AFrontierWeaponBase>> OwnedWeapons;

	UPROPERTY(ReplicatedUsing=OnRep_CurrentWeapon, VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment")
	TObjectPtr<AFrontierWeaponBase> CurrentWeapon;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment")
	int32 CurrentWeaponIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierLoadoutComponent> BoundLoadoutComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> AttachedArmorComponents;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> LinkedWeaponAnimLayerClass;

	bool bWeaponAnimationLayerRefreshPending = false;

	UPROPERTY(Transient)
	FGameplayAbilitySpecHandle CurrentWeaponAttackAbilityHandle;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayAbility> CurrentWeaponAttackAbilityClass;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierWeaponDataAsset> PreloadedAttackWeaponData;

	UPROPERTY(Transient)
	TArray<FFrontierPreloadedAttackAssets> PreloadedAttackAssets;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayAbility> PreloadedAttackAbilityClass;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> PreloadedAttackAnimLayerClass;

	TSharedPtr<FStreamableHandle> AttackAssetPreloadHandle;
	bool bCurrentWeaponAttackAssetsReady = false;

	/** Public cosmetic snapshot. Private item instances remain owner-only in LoadoutComponent. */
	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedArmorVisuals, VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Armor")
	TArray<FFrontierReplicatedArmorVisual> ReplicatedArmorVisuals;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment")
	TSubclassOf<UGameplayEffect> ArmorDefenseEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment")
	TSubclassOf<UGameplayEffect> WeaponAttackPowerEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment")
	TSubclassOf<UGameplayEffect> AggregatedStatsEffectClass;

	UPROPERTY(Transient)
	FActiveGameplayEffectHandle ActiveArmorDefenseEffectHandle;

	UPROPERTY(Transient)
	FActiveGameplayEffectHandle ActiveWeaponAttackPowerEffectHandle;

	UPROPERTY(Transient)
	FActiveGameplayEffectHandle ActiveAggregatedStatsEffectHandle;
};
