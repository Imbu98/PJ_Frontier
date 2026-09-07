#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Skill/FrontierSkillTypes.h"
#include "FrontierEquipmentSkillComponent.generated.h"

class UFrontierSkillDataAsset;
class UGameplayAbility;

DECLARE_MULTICAST_DELEGATE(FFrontierEquipmentSkillsChangedSignature);

UCLASS(ClassGroup=(Frontier), meta=(BlueprintSpawnableComponent))
class FRONTIER_API UFrontierEquipmentSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFrontierEquipmentSkillComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category="Skill")
	void RefreshFromLoadout();

	UFUNCTION(BlueprintCallable, Category="Skill")
	void GrantItemSkillsFromServerData(const FFrontierItemInstance& EquippedItem);

	UFUNCTION(BlueprintCallable, Category="Skill")
	void RequestUseSkillSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerUseSkillSlot(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category="Skill")
	FGameplayTag GetSkillTagAtSlot(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category="Skill")
	bool GetGeneratedSkillAtSlot(int32 SlotIndex, FFrontierGeneratedWeaponSkill& OutGeneratedSkill) const;

	UFUNCTION(BlueprintPure, Category="Skill")
	bool GetRuntimeSkillAtSlot(int32 SlotIndex, FFrontierRuntimeSkillData& OutRuntimeSkill) const;

	UFUNCTION(BlueprintPure, Category="Skill")
	int32 GetSkillLevel(FGameplayTag SkillTag) const;

	UFUNCTION(BlueprintPure, Category="Skill")
	float GetCooldownRemaining(FGameplayTag SkillTag) const;

	UFUNCTION(BlueprintPure, Category="Skill")
	float GetCooldownDuration(FGameplayTag SkillTag) const;

	UFUNCTION(BlueprintPure, Category="Skill")
	bool IsSkillOnCooldown(FGameplayTag SkillTag) const;

	bool CanCommitPendingAreaSkill(TSubclassOf<UGameplayAbility> AbilityClass) const;
	bool CommitPendingAreaSkill(TSubclassOf<UGameplayAbility> AbilityClass);
	void ClearPendingAreaSkill(TSubclassOf<UGameplayAbility> AbilityClass);

	const FFrontierSkillInfo* FindSkillInfo(FGameplayTag SkillTag) const;
	const FFrontierSkillTableRow* FindSkillTableRow(FGameplayTag SkillTag) const;
	const FFrontierSkillTableRow* FindSkillTableRowById(FName SkillId) const;
	FName FindSkillIdByTag(FGameplayTag SkillTag) const;

	FFrontierEquipmentSkillsChangedSignature OnEquipmentSkillsChanged;

protected:
	UFUNCTION()
	void OnRep_GeneratedSkills();

	UFUNCTION()
	void OnRep_SkillLevels();

	UFUNCTION()
	void OnRep_SkillCooldowns();

	UFUNCTION()
	void OnRep_CurrentSkillDataAsset();

	UFUNCTION()
	void HandleLoadoutChanged(const TArray<FFrontierLoadoutSlot>& Slots);

	bool CanUseSkillSlot(int32 SlotIndex, FGameplayTag& OutSkillTag, const FFrontierSkillInfo*& OutSkillInfo) const;
	const FFrontierSkillTableRow* ResolveSkillTableRow(const FFrontierRuntimeSkillData& RuntimeSkill) const;
	bool HasEnoughStamina(const FFrontierSkillInfo& SkillInfo) const;
	void ConsumeStamina(const FFrontierSkillInfo& SkillInfo);
	void RebuildSkillDataFromSlots(const TArray<FFrontierLoadoutSlot>& Slots);
	UFrontierSkillDataAsset* ResolveSkillDataAssetForItem(const FFrontierItemInstance& ItemInstance) const;
	void SyncGrantedEquipmentAbilities();
	void RemoveGrantedEquipmentAbilitiesNotIn(const TSet<TSubclassOf<UGameplayAbility>>& CurrentAbilityClasses);
	void RemoveGrantedEquipmentAbility(TSubclassOf<UGameplayAbility> AbilityClass);
	FGameplayAbilitySpec* FindGrantedEquipmentAbilitySpec(TSubclassOf<UGameplayAbility> AbilityClass) const;
	void SetSkillCooldown(FGameplayTag SkillTag, float CooldownDuration);
	void RebuildActiveSkillCache();
	FFrontierSkillCooldownEntry* FindMutableCooldownEntry(FGameplayTag SkillTag);
	const FFrontierSkillCooldownEntry* FindCooldownEntry(FGameplayTag SkillTag) const;
	float GetCooldownTimeSeconds() const;
	void BroadcastSkillDataChanged();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill", meta=(ClampMin="1", ClampMax="8"))
	int32 MaxSkillSlots = 3;

	UPROPERTY(ReplicatedUsing=OnRep_CurrentSkillDataAsset, VisibleInstanceOnly, BlueprintReadOnly, Category="Skill")
	TObjectPtr<UFrontierSkillDataAsset> CurrentSkillDataAsset;

	UPROPERTY(ReplicatedUsing=OnRep_GeneratedSkills, VisibleInstanceOnly, BlueprintReadOnly, Category="Skill")
	TArray<FFrontierGeneratedWeaponSkill> GeneratedSkills;

	UPROPERTY(ReplicatedUsing=OnRep_GeneratedSkills, VisibleInstanceOnly, BlueprintReadOnly, Category="Skill")
	TArray<FFrontierRuntimeSkillData> RuntimeGeneratedSkills;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Skill")
	TArray<FGameplayTag> ActiveSkillTags;

	UPROPERTY(ReplicatedUsing=OnRep_SkillLevels, VisibleInstanceOnly, BlueprintReadOnly, Category="Skill")
	TArray<FFrontierSkillLevelEntry> SkillLevels;

	UPROPERTY(ReplicatedUsing=OnRep_SkillCooldowns, VisibleInstanceOnly, BlueprintReadOnly, Category="Skill")
	TArray<FFrontierSkillCooldownEntry> SkillCooldowns;

	UPROPERTY(Transient)
	FGameplayTag PendingAreaSkillTag;

	UPROPERTY(Transient)
	FFrontierSkillInfo PendingAreaSkillInfo;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayAbility> PendingAreaSkillAbilityClass;

	UPROPERTY(Transient)
	TMap<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle> GrantedEquipmentAbilityHandles;

	UPROPERTY(Transient)
	FString CurrentGrantedItemInstanceId;

	TMap<FString, TArray<FGameplayAbilitySpecHandle>> GrantedEquipmentAbilityHandlesByItemInstanceId;

	UPROPERTY(Transient)
	TMap<FGameplayTag, FFrontierSkillInfo> RuntimeSkillInfoCache;

	mutable FFrontierSkillInfo ResolvedSkillInfoScratch;
};
