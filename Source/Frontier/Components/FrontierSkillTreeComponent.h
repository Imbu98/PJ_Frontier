#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "SkillTree/FrontierSkillTreeTypes.h"
#include "FrontierSkillTreeComponent.generated.h"

class UFrontierAbilitySystemComponent;
class UFrontierSkillTreeComponent;
class UFrontierSkillTreeDataAsset;
class UGameplayEffect;

struct FFrontierWeaponElementAttackKey
{
	FGameplayTag WeaponFamilyTag;
	EFrontierElementalType ElementalType = EFrontierElementalType::Normal;

	bool operator==(const FFrontierWeaponElementAttackKey& Other) const
	{
		return WeaponFamilyTag.MatchesTagExact(Other.WeaponFamilyTag)
			&& ElementalType == Other.ElementalType;
	}

	friend uint32 GetTypeHash(const FFrontierWeaponElementAttackKey& Key)
	{
		return HashCombine(
			GetTypeHash(Key.WeaponFamilyTag),
			GetTypeHash(static_cast<uint8>(Key.ElementalType)));
	}
};

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierSkillTreeNodeState : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Skill Tree")
	FGameplayTag NodeTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Skill Tree")
	int32 Rank = 0;
};

USTRUCT()
struct FRONTIER_API FFrontierReplicatedSkillTreeState : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FFrontierSkillTreeNodeState> Items;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParameters)
	{
		return FastArrayDeltaSerialize<FFrontierSkillTreeNodeState, FFrontierReplicatedSkillTreeState>(Items, DeltaParameters, *this);
	}

	void SetOwner(UFrontierSkillTreeComponent* InOwner);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	void PostReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);

private:
	TWeakObjectPtr<UFrontierSkillTreeComponent> Owner;
};

template<>
struct TStructOpsTypeTraits<FFrontierReplicatedSkillTreeState> : public TStructOpsTypeTraitsBase2<FFrontierReplicatedSkillTreeState>
{
	enum { WithNetDeltaSerializer = true };
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFrontierSkillTreeChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FFrontierSkillTreeRequestCompletedSignature,
	FGameplayTag, NodeTag,
	EFrontierSkillTreeRequestResult, Result);

UCLASS(ClassGroup=(Frontier), meta=(BlueprintSpawnableComponent))
class FRONTIER_API UFrontierSkillTreeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFrontierSkillTreeComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree")
	int32 GetAvailableSkillPoints() const { return AvailableSkillPoints; }

	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree")
	int32 GetTotalSkillPointsEarned() const { return TotalSkillPointsEarned; }

	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree")
	int32 GetNodeRank(FGameplayTag NodeTag) const;

	/** Server-authoritative bonus for the exact weapon-family and attack-element combination. */
	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree|Combat")
	float GetWeaponElementAttackPowerBonus(
		FGameplayTag CurrentWeaponTypeTag,
		EFrontierElementalType ElementalType) const;

	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree")
	EFrontierSkillTreeRequestResult CanUnlockNode(FGameplayTag NodeTag, int32& OutPointCost) const;

	UFUNCTION(BlueprintCallable, Category="Frontier|Skill Tree")
	void RequestUnlockNode(FGameplayTag NodeTag);

	UFUNCTION(BlueprintCallable, Category="Frontier|Skill Tree")
	void RequestResetTree();

	/** Server-only generic reward/debug entry point. Positive values are persisted as earned points. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Frontier|Skill Tree")
	bool GrantSkillPoints(int32 PointDelta);

	/** Call this from any authoritative level-up flow, regardless of the current map. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Frontier|Skill Tree")
	bool GrantSkillPointsForLevelUps(int32 LevelsGained = 1);

	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree|Persistence")
	void BuildPersistentSnapshot(FFrontierSkillTreeProgressionSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Frontier|Skill Tree|Persistence")
	bool ApplyPersistentSnapshot(const FFrontierSkillTreeProgressionSnapshot& Snapshot);

	/** Applies only the saved allocation; all point totals are rebuilt from a verified Backend budget. */
	bool ApplyPersistentSnapshotWithPointBudget(
		const FFrontierSkillTreeProgressionSnapshot& Snapshot,
		int32 TotalPointBudget,
		bool& bOutAllocationReset);

	/** Pure reconciliation used by the authoritative apply path and automation tests. */
	static bool ReconcilePersistentSnapshotToPointBudget(
		const UFrontierSkillTreeDataAsset* Data,
		const FFrontierSkillTreeProgressionSnapshot& Snapshot,
		int32 TotalPointBudget,
		FFrontierSkillTreeProgressionSnapshot& OutSnapshot,
		bool& bOutAllocationReset);

	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree")
	const UFrontierSkillTreeDataAsset* GetSkillTreeData() const { return SkillTreeData; }

	UPROPERTY(BlueprintAssignable, Category="Frontier|Skill Tree")
	FFrontierSkillTreeChangedSignature OnSkillTreeChanged;

	UPROPERTY(BlueprintAssignable, Category="Frontier|Skill Tree")
	FFrontierSkillTreeRequestCompletedSignature OnSkillTreeRequestCompleted;

protected:
	UFUNCTION(Server, Reliable)
	void ServerUnlockNode(FGameplayTag NodeTag);

	UFUNCTION(Server, Reliable)
	void ServerResetTree();

	UFUNCTION(Client, Reliable)
	void ClientNotifyRequestCompleted(FGameplayTag NodeTag, EFrontierSkillTreeRequestResult Result);

	UFUNCTION()
	void OnRep_AvailableSkillPoints();

	UFUNCTION()
	void OnRep_TotalSkillPointsEarned();

	EFrontierSkillTreeRequestResult UnlockNodeAuthoritative(FGameplayTag NodeTag);
	EFrontierSkillTreeRequestResult ResetTreeAuthoritative();
	bool CanMutateSkillTreeInCurrentContext() const;
	void RefreshAppliedEffects();
	UFrontierAbilitySystemComponent* GetFrontierAbilitySystemComponent() const;
	void HandleReplicatedTreeChanged();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill Tree")
	TObjectPtr<UFrontierSkillTreeDataAsset> SkillTreeData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill Tree")
	TSubclassOf<UGameplayEffect> AggregatedStatsEffectClass;

	UPROPERTY(ReplicatedUsing=OnRep_AvailableSkillPoints, VisibleInstanceOnly, BlueprintReadOnly, Category="Skill Tree")
	int32 AvailableSkillPoints = 0;

	UPROPERTY(ReplicatedUsing=OnRep_TotalSkillPointsEarned, VisibleInstanceOnly, BlueprintReadOnly, Category="Skill Tree")
	int32 TotalSkillPointsEarned = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill Tree|Progression", meta=(ClampMin="1"))
	int32 SkillPointsPerLevel = 1;

	UPROPERTY(Replicated)
	FFrontierReplicatedSkillTreeState ReplicatedNodeStates;

	UPROPERTY(Transient)
	FActiveGameplayEffectHandle ActiveStatsEffectHandle;

	UPROPERTY(Transient)
	TArray<FActiveGameplayEffectHandle> ActiveGrantedEffectHandles;

	UPROPERTY(Transient)
	FGameplayTagContainer AppliedLooseTags;

	/** Derived from replicated/saved node ranks; it is never separately saved or replicated. */
	TMap<FFrontierWeaponElementAttackKey, float> CachedWeaponElementAttackBonuses;

private:
	friend struct FFrontierReplicatedSkillTreeState;
};
