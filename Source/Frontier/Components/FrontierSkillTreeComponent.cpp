#include "Components/FrontierSkillTreeComponent.h"

#include "AbilitySystem/Effects/FrontierAggregatedStatsGameplayEffect.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "Components/FrontierStorageComponent.h"
#include "Components/FrontierRaidInventoryComponent.h"
#include "Frontier.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierLobbyPlayerController.h"
#include "Game/FrontierPlayerState.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "SkillTree/FrontierSkillTreeDataAsset.h"
#include "Tags/FrontierGameplayTags.h"

namespace
{
EFrontierElementalType NormalizeSkillTreeElement(const EFrontierElementalType ElementalType)
{
	return ElementalType == EFrontierElementalType::None
		? EFrontierElementalType::Normal
		: ElementalType;
}

bool IsRemovedLegacyAttackNode(const FGameplayTag NodeTag)
{
	const FName TagName = NodeTag.GetTagName();
	return TagName == TEXT("SkillTree.Node.Weapon.SwordMastery")
		|| TagName == TEXT("SkillTree.Node.Weapon.AxeMastery")
		|| TagName == TEXT("SkillTree.Node.Fire.AttackMastery")
		|| TagName == TEXT("SkillTree.Node.Ice.AttackMastery")
		|| TagName == TEXT("SkillTree.Node.Poison.AttackMastery")
		|| TagName == TEXT("SkillTree.Node.Lightning.AttackMastery");
}

void BuildSanitizedCandidateRanks(
	const UFrontierSkillTreeDataAsset* Data,
	const FFrontierSkillTreeProgressionSnapshot& Snapshot,
	TMap<FGameplayTag, int32>& OutCandidateRanks,
	int64& OutRefundedLegacyOrInvalidPoints)
{
	OutCandidateRanks.Reset();
	OutRefundedLegacyOrInvalidPoints = 0;
	for (const FFrontierSavedSkillTreeNode& SavedNode : Snapshot.Nodes)
	{
		const FFrontierSkillTreeNodeDefinition* Node = Data ? Data->FindNode(SavedNode.NodeTag) : nullptr;
		if (Node && SavedNode.Rank > 0)
		{
			const int32 SanitizedRank = FMath::Clamp(SavedNode.Rank, 1, FMath::Max(1, Node->MaxRank));
			int32& ExistingRank = OutCandidateRanks.FindOrAdd(Node->NodeTag);
			ExistingRank = FMath::Max(ExistingRank, SanitizedRank);
		}
		else if (SavedNode.Rank > 0 && IsRemovedLegacyAttackNode(SavedNode.NodeTag))
		{
			static const int32 LegacyRankCosts[] = {1, 1, 2, 2, 3};
			for (int32 RankIndex = 0;
				RankIndex < FMath::Min(SavedNode.Rank, static_cast<int32>(UE_ARRAY_COUNT(LegacyRankCosts)));
				++RankIndex)
			{
				OutRefundedLegacyOrInvalidPoints += LegacyRankCosts[RankIndex];
			}
		}
	}

	bool bRemovedInvalidDependency = true;
	while (bRemovedInvalidDependency)
	{
		bRemovedInvalidDependency = false;
		TArray<FGameplayTag> InvalidNodes;
		for (const TPair<FGameplayTag, int32>& Pair : OutCandidateRanks)
		{
			const FFrontierSkillTreeNodeDefinition* Node = Data->FindNode(Pair.Key);
			if (!Node)
			{
				InvalidNodes.Add(Pair.Key);
				continue;
			}

			bool bAnyPrerequisiteSatisfied = Node->Prerequisites.IsEmpty();
			bool bAllPrerequisitesSatisfied = true;
			for (const FFrontierSkillTreePrerequisite& Prerequisite : Node->Prerequisites)
			{
				const bool bSatisfied = Prerequisite.NodeTag.IsValid()
					&& Data->FindNode(Prerequisite.NodeTag)
					&& OutCandidateRanks.FindRef(Prerequisite.NodeTag) >= FMath::Max(1, Prerequisite.RequiredRank);
				bAnyPrerequisiteSatisfied |= bSatisfied;
				bAllPrerequisitesSatisfied &= bSatisfied;
			}

			const bool bPrerequisitesSatisfied = Node->PrerequisitePolicy == EFrontierSkillTreePrerequisitePolicy::All
				? bAllPrerequisitesSatisfied
				: bAnyPrerequisiteSatisfied;
			if (!bPrerequisitesSatisfied)
			{
				InvalidNodes.Add(Pair.Key);
			}
		}

		for (const FGameplayTag InvalidNode : InvalidNodes)
		{
			if (const FFrontierSkillTreeNodeDefinition* Node = Data->FindNode(InvalidNode))
			{
				const int32 RemovedRank = OutCandidateRanks.FindRef(InvalidNode);
				for (int32 RankIndex = 0; RankIndex < RemovedRank; ++RankIndex)
				{
					OutRefundedLegacyOrInvalidPoints += FMath::Max(0, Data->GetRankCost(*Node, RankIndex));
				}
			}
			bRemovedInvalidDependency |= OutCandidateRanks.Remove(InvalidNode) > 0;
		}
	}
}

int64 CalculateSpentPoints(
	const UFrontierSkillTreeDataAsset* Data,
	const TMap<FGameplayTag, int32>& CandidateRanks)
{
	int64 SpentPoints = 0;
	for (const TPair<FGameplayTag, int32>& Pair : CandidateRanks)
	{
		const FFrontierSkillTreeNodeDefinition* Node = Data ? Data->FindNode(Pair.Key) : nullptr;
		if (!Node)
		{
			continue;
		}
		for (int32 RankIndex = 0; RankIndex < Pair.Value; ++RankIndex)
		{
			SpentPoints += FMath::Max(0, Data->GetRankCost(*Node, RankIndex));
		}
	}
	return SpentPoints;
}

void AppendCandidateNodes(
	const TMap<FGameplayTag, int32>& CandidateRanks,
	TArray<FFrontierSavedSkillTreeNode>& OutNodes)
{
	TArray<FGameplayTag> SortedTags;
	CandidateRanks.GetKeys(SortedTags);
	SortedTags.Sort([](const FGameplayTag& Left, const FGameplayTag& Right)
	{
		return Left.ToString() < Right.ToString();
	});
	for (const FGameplayTag NodeTag : SortedTags)
	{
		OutNodes.Add({NodeTag, CandidateRanks.FindRef(NodeTag)});
	}
}
}

void FFrontierReplicatedSkillTreeState::SetOwner(UFrontierSkillTreeComponent* InOwner)
{
	Owner = InOwner;
}

void FFrontierReplicatedSkillTreeState::PostReplicatedAdd(
	const TArrayView<int32> AddedIndices,
	const int32 FinalSize)
{
	if (UFrontierSkillTreeComponent* Component = Owner.Get())
	{
		Component->HandleReplicatedTreeChanged();
	}
}

void FFrontierReplicatedSkillTreeState::PostReplicatedChange(
	const TArrayView<int32> ChangedIndices,
	const int32 FinalSize)
{
	if (UFrontierSkillTreeComponent* Component = Owner.Get())
	{
		Component->HandleReplicatedTreeChanged();
	}
}

void FFrontierReplicatedSkillTreeState::PostReplicatedRemove(
	const TArrayView<int32> RemovedIndices,
	const int32 FinalSize)
{
	if (UFrontierSkillTreeComponent* Component = Owner.Get())
	{
		Component->HandleReplicatedTreeChanged();
	}
}

UFrontierSkillTreeComponent::UFrontierSkillTreeComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
	ReplicatedNodeStates.SetOwner(this);
	AggregatedStatsEffectClass = UFrontierAggregatedStatsGameplayEffect::StaticClass();
}

void UFrontierSkillTreeComponent::BeginPlay()
{
	Super::BeginPlay();
	ReplicatedNodeStates.SetOwner(this);
	if (!SkillTreeData)
	{
		// The native CDO provides a usable starter tree. Projects can assign a
		// dedicated DataAsset on the PlayerState component to replace it.
		SkillTreeData = GetMutableDefault<UFrontierSkillTreeDataAsset>();
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		RefreshAppliedEffects();
	}
}

void UFrontierSkillTreeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UFrontierSkillTreeComponent, AvailableSkillPoints, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UFrontierSkillTreeComponent, TotalSkillPointsEarned, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UFrontierSkillTreeComponent, ReplicatedNodeStates, COND_OwnerOnly);
}

int32 UFrontierSkillTreeComponent::GetNodeRank(const FGameplayTag NodeTag) const
{
	const FFrontierSkillTreeNodeState* State = ReplicatedNodeStates.Items.FindByPredicate(
		[NodeTag](const FFrontierSkillTreeNodeState& Entry)
		{
			return Entry.NodeTag.MatchesTagExact(NodeTag);
		});
	return State ? FMath::Max(0, State->Rank) : 0;
}

float UFrontierSkillTreeComponent::GetWeaponElementAttackPowerBonus(
	FGameplayTag CurrentWeaponTypeTag,
	const EFrontierElementalType ElementalType) const
{
	if (!CurrentWeaponTypeTag.IsValid())
	{
		return 0.0f;
	}

	const EFrontierElementalType NormalizedElement = NormalizeSkillTreeElement(ElementalType);
	float TotalBonus = 0.0f;
	while (CurrentWeaponTypeTag.IsValid())
	{
		const FFrontierWeaponElementAttackKey Key{CurrentWeaponTypeTag, NormalizedElement};
		if (const float* Bonus = CachedWeaponElementAttackBonuses.Find(Key))
		{
			TotalBonus += *Bonus;
		}
		CurrentWeaponTypeTag = CurrentWeaponTypeTag.RequestDirectParent();
	}
	return TotalBonus;
}

EFrontierSkillTreeRequestResult UFrontierSkillTreeComponent::CanUnlockNode(
	const FGameplayTag NodeTag,
	int32& OutPointCost) const
{
	OutPointCost = 0;
	if (!SkillTreeData)
	{
		return EFrontierSkillTreeRequestResult::InvalidData;
	}

	const FFrontierSkillTreeNodeDefinition* Node = SkillTreeData->FindNode(NodeTag);
	if (!Node || !Node->NodeTag.IsValid())
	{
		return EFrontierSkillTreeRequestResult::InvalidNode;
	}

	const int32 CurrentRank = GetNodeRank(NodeTag);
	if (CurrentRank >= FMath::Max(1, Node->MaxRank))
	{
		return EFrontierSkillTreeRequestResult::AlreadyMaxRank;
	}

	bool bAnyPrerequisiteSatisfied = Node->Prerequisites.IsEmpty();
	for (const FFrontierSkillTreePrerequisite& Prerequisite : Node->Prerequisites)
	{
		if (!Prerequisite.NodeTag.IsValid() || !SkillTreeData->FindNode(Prerequisite.NodeTag))
		{
			return EFrontierSkillTreeRequestResult::InvalidData;
		}

		const bool bSatisfied = GetNodeRank(Prerequisite.NodeTag) >= FMath::Max(1, Prerequisite.RequiredRank);
		bAnyPrerequisiteSatisfied |= bSatisfied;
		if (Node->PrerequisitePolicy == EFrontierSkillTreePrerequisitePolicy::All && !bSatisfied)
		{
			return EFrontierSkillTreeRequestResult::MissingPrerequisite;
		}
	}
	if (Node->PrerequisitePolicy == EFrontierSkillTreePrerequisitePolicy::Any && !bAnyPrerequisiteSatisfied)
	{
		return EFrontierSkillTreeRequestResult::MissingPrerequisite;
	}

	OutPointCost = SkillTreeData->GetRankCost(*Node, CurrentRank);
	return AvailableSkillPoints >= OutPointCost
		? EFrontierSkillTreeRequestResult::Success
		: EFrontierSkillTreeRequestResult::InsufficientPoints;
}

void UFrontierSkillTreeComponent::RequestUnlockNode(const FGameplayTag NodeTag)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		const EFrontierSkillTreeRequestResult Result = UnlockNodeAuthoritative(NodeTag);
		OnSkillTreeRequestCompleted.Broadcast(NodeTag, Result);
		return;
	}

	ServerUnlockNode(NodeTag);
}

void UFrontierSkillTreeComponent::RequestResetTree()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		const EFrontierSkillTreeRequestResult Result = ResetTreeAuthoritative();
		OnSkillTreeRequestCompleted.Broadcast(FGameplayTag(), Result);
		return;
	}

	ServerResetTree();
}

bool UFrontierSkillTreeComponent::GrantSkillPoints(const int32 PointDelta)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || PointDelta == 0)
	{
		return false;
	}

	const int64 NewPointTotal = static_cast<int64>(AvailableSkillPoints) + PointDelta;
	if (NewPointTotal < 0 || NewPointTotal > MAX_int32)
	{
		return false;
	}

	AvailableSkillPoints = static_cast<int32>(NewPointTotal);
	if (PointDelta > 0)
	{
		TotalSkillPointsEarned = static_cast<int32>(FMath::Min<int64>(
			MAX_int32,
			static_cast<int64>(TotalSkillPointsEarned) + PointDelta));
		OnRep_TotalSkillPointsEarned();
	}
	OnRep_AvailableSkillPoints();
	GetOwner()->ForceNetUpdate();
	return true;
}

bool UFrontierSkillTreeComponent::GrantSkillPointsForLevelUps(const int32 LevelsGained)
{
	if (LevelsGained <= 0 || SkillPointsPerLevel <= 0)
	{
		return false;
	}

	const int64 Reward = static_cast<int64>(LevelsGained) * SkillPointsPerLevel;
	return Reward <= MAX_int32 && GrantSkillPoints(static_cast<int32>(Reward));
}

void UFrontierSkillTreeComponent::BuildPersistentSnapshot(FFrontierSkillTreeProgressionSnapshot& OutSnapshot) const
{
	OutSnapshot = FFrontierSkillTreeProgressionSnapshot();
	OutSnapshot.AvailableSkillPoints = FMath::Max(0, AvailableSkillPoints);
	OutSnapshot.TotalSkillPointsEarned = FMath::Max(0, TotalSkillPointsEarned);
	OutSnapshot.Nodes.Reserve(ReplicatedNodeStates.Items.Num());
	for (const FFrontierSkillTreeNodeState& State : ReplicatedNodeStates.Items)
	{
		if (State.NodeTag.IsValid() && State.Rank > 0)
		{
			OutSnapshot.Nodes.Add({State.NodeTag, State.Rank});
		}
	}
}

bool UFrontierSkillTreeComponent::ApplyPersistentSnapshot(const FFrontierSkillTreeProgressionSnapshot& Snapshot)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !SkillTreeData)
	{
		return false;
	}

	TMap<FGameplayTag, int32> CandidateRanks;
	int64 RefundedPoints = 0;
	BuildSanitizedCandidateRanks(SkillTreeData, Snapshot, CandidateRanks, RefundedPoints);

	ReplicatedNodeStates.Items.Reset();
	TArray<FFrontierSavedSkillTreeNode> SanitizedNodes;
	AppendCandidateNodes(CandidateRanks, SanitizedNodes);
	for (const FFrontierSavedSkillTreeNode& SavedNode : SanitizedNodes)
	{
		FFrontierSkillTreeNodeState& State = ReplicatedNodeStates.Items.AddDefaulted_GetRef();
		State.NodeTag = SavedNode.NodeTag;
		State.Rank = SavedNode.Rank;
	}

	const int64 SpentPoints = CalculateSpentPoints(SkillTreeData, CandidateRanks);
	AvailableSkillPoints = static_cast<int32>(FMath::Min<int64>(
		MAX_int32,
		static_cast<int64>(FMath::Max(0, Snapshot.AvailableSkillPoints)) + RefundedPoints));
	TotalSkillPointsEarned = static_cast<int32>(FMath::Min<int64>(
		MAX_int32,
		FMath::Max<int64>(Snapshot.TotalSkillPointsEarned, SpentPoints + AvailableSkillPoints)));
	ReplicatedNodeStates.MarkArrayDirty();
	RefreshAppliedEffects();
	OnSkillTreeChanged.Broadcast();
	GetOwner()->ForceNetUpdate();
	return true;
}

bool UFrontierSkillTreeComponent::ReconcilePersistentSnapshotToPointBudget(
	const UFrontierSkillTreeDataAsset* Data,
	const FFrontierSkillTreeProgressionSnapshot& Snapshot,
	const int32 TotalPointBudget,
	FFrontierSkillTreeProgressionSnapshot& OutSnapshot,
	bool& bOutAllocationReset)
{
	OutSnapshot = FFrontierSkillTreeProgressionSnapshot();
	bOutAllocationReset = false;
	if (!Data || TotalPointBudget < 0)
	{
		return false;
	}

	TMap<FGameplayTag, int32> CandidateRanks;
	int64 IgnoredRefund = 0;
	BuildSanitizedCandidateRanks(Data, Snapshot, CandidateRanks, IgnoredRefund);
	int64 SpentPoints = CalculateSpentPoints(Data, CandidateRanks);
	if (SpentPoints > TotalPointBudget)
	{
		// There is no purchase order in the local format, so partial pruning would be arbitrary.
		// Resetting the allocation preserves the full verified budget for the player to re-spend.
		CandidateRanks.Reset();
		SpentPoints = 0;
		bOutAllocationReset = true;
	}

	OutSnapshot.TotalSkillPointsEarned = TotalPointBudget;
	OutSnapshot.AvailableSkillPoints = TotalPointBudget - static_cast<int32>(SpentPoints);
	AppendCandidateNodes(CandidateRanks, OutSnapshot.Nodes);
	return true;
}

bool UFrontierSkillTreeComponent::ApplyPersistentSnapshotWithPointBudget(
	const FFrontierSkillTreeProgressionSnapshot& Snapshot,
	const int32 TotalPointBudget,
	bool& bOutAllocationReset)
{
	bOutAllocationReset = false;
	if (!GetOwner() || !GetOwner()->HasAuthority() || !SkillTreeData)
	{
		return false;
	}

	FFrontierSkillTreeProgressionSnapshot ReconciledSnapshot;
	if (!ReconcilePersistentSnapshotToPointBudget(
		SkillTreeData,
		Snapshot,
		TotalPointBudget,
		ReconciledSnapshot,
		bOutAllocationReset))
	{
		return false;
	}

	ReplicatedNodeStates.Items.Reset();
	for (const FFrontierSavedSkillTreeNode& SavedNode : ReconciledSnapshot.Nodes)
	{
		FFrontierSkillTreeNodeState& State = ReplicatedNodeStates.Items.AddDefaulted_GetRef();
		State.NodeTag = SavedNode.NodeTag;
		State.Rank = SavedNode.Rank;
	}
	AvailableSkillPoints = ReconciledSnapshot.AvailableSkillPoints;
	TotalSkillPointsEarned = ReconciledSnapshot.TotalSkillPointsEarned;
	ReplicatedNodeStates.MarkArrayDirty();
	RefreshAppliedEffects();
	OnSkillTreeChanged.Broadcast();
	GetOwner()->ForceNetUpdate();
	return true;
}

void UFrontierSkillTreeComponent::ServerUnlockNode_Implementation(const FGameplayTag NodeTag)
{
	ClientNotifyRequestCompleted(NodeTag, UnlockNodeAuthoritative(NodeTag));
}

void UFrontierSkillTreeComponent::ServerResetTree_Implementation()
{
	ClientNotifyRequestCompleted(FGameplayTag(), ResetTreeAuthoritative());
}

void UFrontierSkillTreeComponent::ClientNotifyRequestCompleted_Implementation(
	const FGameplayTag NodeTag,
	const EFrontierSkillTreeRequestResult Result)
{
	OnSkillTreeRequestCompleted.Broadcast(NodeTag, Result);
}

void UFrontierSkillTreeComponent::OnRep_AvailableSkillPoints()
{
	OnSkillTreeChanged.Broadcast();
}

void UFrontierSkillTreeComponent::OnRep_TotalSkillPointsEarned()
{
	OnSkillTreeChanged.Broadcast();
}

EFrontierSkillTreeRequestResult UFrontierSkillTreeComponent::UnlockNodeAuthoritative(const FGameplayTag NodeTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return EFrontierSkillTreeRequestResult::NotAuthority;
	}
	if (!CanMutateSkillTreeInCurrentContext())
	{
		FRONTIER_LOG(Warning, TEXT("Skill tree unlock rejected outside the lobby. Owner=%s Node=%s"),
			*GetNameSafe(GetOwner()), *NodeTag.ToString());
		return EFrontierSkillTreeRequestResult::InvalidContext;
	}
	int32 PointCost = 0;
	const EFrontierSkillTreeRequestResult ValidationResult = CanUnlockNode(NodeTag, PointCost);
	if (ValidationResult != EFrontierSkillTreeRequestResult::Success)
	{
		return ValidationResult;
	}

	FFrontierSkillTreeNodeState* State = ReplicatedNodeStates.Items.FindByPredicate(
		[NodeTag](const FFrontierSkillTreeNodeState& Entry)
		{
			return Entry.NodeTag.MatchesTagExact(NodeTag);
		});
	if (!State)
	{
		State = &ReplicatedNodeStates.Items.AddDefaulted_GetRef();
		State->NodeTag = NodeTag;
	}

	++State->Rank;
	AvailableSkillPoints -= PointCost;
	ReplicatedNodeStates.MarkItemDirty(*State);
	RefreshAppliedEffects();
	OnSkillTreeChanged.Broadcast();
	GetOwner()->ForceNetUpdate();
	return EFrontierSkillTreeRequestResult::Success;
}

EFrontierSkillTreeRequestResult UFrontierSkillTreeComponent::ResetTreeAuthoritative()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return EFrontierSkillTreeRequestResult::NotAuthority;
	}
	if (!CanMutateSkillTreeInCurrentContext())
	{
		FRONTIER_LOG(Warning, TEXT("Skill tree reset rejected outside the lobby. Owner=%s"), *GetNameSafe(GetOwner()));
		return EFrontierSkillTreeRequestResult::InvalidContext;
	}
	if (!SkillTreeData)
	{
		return EFrontierSkillTreeRequestResult::InvalidData;
	}

	int64 Refund = 0;
	for (const FFrontierSkillTreeNodeState& State : ReplicatedNodeStates.Items)
	{
		const FFrontierSkillTreeNodeDefinition* Node = SkillTreeData->FindNode(State.NodeTag);
		if (!Node)
		{
			continue;
		}

		for (int32 RankIndex = 0; RankIndex < State.Rank; ++RankIndex)
		{
			Refund += SkillTreeData->GetRankCost(*Node, RankIndex);
		}
	}

	AvailableSkillPoints = static_cast<int32>(FMath::Min<int64>(MAX_int32, AvailableSkillPoints + Refund));
	ReplicatedNodeStates.Items.Reset();
	ReplicatedNodeStates.MarkArrayDirty();
	RefreshAppliedEffects();
	OnSkillTreeChanged.Broadcast();
	GetOwner()->ForceNetUpdate();
	return EFrontierSkillTreeRequestResult::Success;
}

bool UFrontierSkillTreeComponent::CanMutateSkillTreeInCurrentContext() const
{
	const AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(GetOwner());
	const APlayerController* PlayerController = PlayerState ? PlayerState->GetPlayerController() : nullptr;
	if (PlayerController && PlayerController->IsA<AFrontierLobbyPlayerController>())
	{
		return true;
	}

	const AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(PlayerController);
	return FrontierPlayerController && FrontierPlayerController->IsInLobbyContext();
}

void UFrontierSkillTreeComponent::RefreshAppliedEffects()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	CachedWeaponElementAttackBonuses.Reset();
	TMap<FGameplayTag, float> AggregatedStats;
	TSet<TSubclassOf<UGameplayEffect>> DesiredEffects;
	FGameplayTagContainer DesiredTags;

	if (SkillTreeData)
	{
		for (const FFrontierSkillTreeNodeState& State : ReplicatedNodeStates.Items)
		{
			const FFrontierSkillTreeNodeDefinition* Node = SkillTreeData->FindNode(State.NodeTag);
			if (!Node)
			{
				continue;
			}

			const int32 AppliedRankCount = FMath::Clamp(State.Rank, 0, FMath::Max(1, Node->MaxRank));
			for (int32 RankIndex = 0; RankIndex < AppliedRankCount; ++RankIndex)
			{
				const FFrontierSkillTreeRankDefinition* Rank = SkillTreeData->GetRankDefinition(*Node, RankIndex);
				if (!Rank)
				{
					continue;
				}

				for (const FFrontierSkillTreeStatModifier& Modifier : Rank->StatModifiers)
				{
					if (Modifier.StatTag.IsValid())
					{
						AggregatedStats.FindOrAdd(Modifier.StatTag) += Modifier.Magnitude;
					}
				}
				for (const FFrontierSkillTreeConditionalAttackModifier& Modifier : Rank->ConditionalAttackModifiers)
				{
					if (Modifier.WeaponFamilyTag.IsValid() && !FMath::IsNearlyZero(Modifier.Magnitude))
					{
						const FFrontierWeaponElementAttackKey Key{
							Modifier.WeaponFamilyTag,
							NormalizeSkillTreeElement(Modifier.ElementalType)};
						CachedWeaponElementAttackBonuses.FindOrAdd(Key) += Modifier.Magnitude;
					}
				}
				for (const TSubclassOf<UGameplayEffect> EffectClass : Rank->GrantedEffects)
				{
					if (EffectClass)
					{
						DesiredEffects.Add(EffectClass);
					}
				}
				DesiredTags.AppendTags(Rank->GrantedTags);
			}
		}
	}

	UFrontierAbilitySystemComponent* ASC = GetFrontierAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	if (AggregatedStatsEffectClass && !AggregatedStats.IsEmpty())
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);
		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(AggregatedStatsEffectClass, 1.0f, Context);
		if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
		{
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataAttackPower, AggregatedStats.FindRef(Tags.StatAttackPower));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataDefense, AggregatedStats.FindRef(Tags.StatDefense));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataFireAttackPower, AggregatedStats.FindRef(Tags.StatFireAttackPower));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataIceAttackPower, AggregatedStats.FindRef(Tags.StatIceAttackPower));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataLightningAttackPower, AggregatedStats.FindRef(Tags.StatLightningAttackPower));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataPoisonAttackPower, AggregatedStats.FindRef(Tags.StatPoisonAttackPower));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataFireResistance, AggregatedStats.FindRef(Tags.StatFireResistance));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataIceResistance, AggregatedStats.FindRef(Tags.StatIceResistance));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataLightningResistance, AggregatedStats.FindRef(Tags.StatLightningResistance));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataPoisonResistance, AggregatedStats.FindRef(Tags.StatPoisonResistance));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataSwordAttackPower, AggregatedStats.FindRef(Tags.StatSwordAttackPower));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataAxeAttackPower, AggregatedStats.FindRef(Tags.StatAxeAttackPower));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataMaxHealthBonus, AggregatedStats.FindRef(Tags.StatMaxHealthBonus));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataMaxStaminaBonus, AggregatedStats.FindRef(Tags.StatMaxStaminaBonus));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataSwordAttackSpeedBonus, AggregatedStats.FindRef(Tags.StatSwordAttackSpeedBonus));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataAxeAttackSpeedBonus, AggregatedStats.FindRef(Tags.StatAxeAttackSpeedBonus));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataMoveSpeedBonus, AggregatedStats.FindRef(Tags.StatMoveSpeedBonus));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataJumpPowerBonus, AggregatedStats.FindRef(Tags.StatJumpPowerBonus));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataLobbyStorageSlotBonus, AggregatedStats.FindRef(Tags.StatLobbyStorageSlotBonus));
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.DataRaidInventorySlotBonus, AggregatedStats.FindRef(Tags.StatRaidInventorySlotBonus));
			const FActiveGameplayEffectHandle NewStatsEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			if (NewStatsEffectHandle.IsValid())
			{
				// Apply the replacement first so MaxHealth/MaxStamina never dip to their base values
				// while the aggregated passive effect is being refreshed.
				if (ActiveStatsEffectHandle.IsValid())
				{
					ASC->RemoveActiveGameplayEffect(ActiveStatsEffectHandle);
				}
				ActiveStatsEffectHandle = NewStatsEffectHandle;
			}
		}
	}
	else if (ActiveStatsEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(ActiveStatsEffectHandle);
		ActiveStatsEffectHandle.Invalidate();
	}

	for (const FActiveGameplayEffectHandle Handle : ActiveGrantedEffectHandles)
	{
		if (Handle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}
	ActiveGrantedEffectHandles.Reset();

	for (const TSubclassOf<UGameplayEffect> EffectClass : DesiredEffects)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);
		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1.0f, Context);
		if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
		{
			ActiveGrantedEffectHandles.Add(ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get()));
		}
	}

	if (!AppliedLooseTags.IsEmpty())
	{
		ASC->RemoveLooseGameplayTags(AppliedLooseTags);
	}
	AppliedLooseTags = DesiredTags;
	if (!AppliedLooseTags.IsEmpty())
	{
		ASC->AddLooseGameplayTags(AppliedLooseTags);
	}

	if (AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(GetOwner()))
	{
		const int32 LobbySlotBonus = FMath::Max(0, FMath::RoundToInt(AggregatedStats.FindRef(Tags.StatLobbyStorageSlotBonus)));
		const int32 RaidSlotBonus = FMath::Max(0, FMath::RoundToInt(AggregatedStats.FindRef(Tags.StatRaidInventorySlotBonus)));
		if (UFrontierStorageComponent* Storage = PlayerState->GetStorageComponent())
		{
			Storage->SetBonusSlotCount(LobbySlotBonus);
		}
		if (UFrontierRaidInventoryComponent* RaidInventory = PlayerState->GetRaidInventoryComponent())
		{
			RaidInventory->SetBonusSlotCount(RaidSlotBonus);
		}
	}
}

UFrontierAbilitySystemComponent* UFrontierSkillTreeComponent::GetFrontierAbilitySystemComponent() const
{
	const AFrontierPlayerState* PlayerState = Cast<AFrontierPlayerState>(GetOwner());
	return PlayerState ? PlayerState->GetFrontierAbilitySystemComponent() : nullptr;
}

void UFrontierSkillTreeComponent::HandleReplicatedTreeChanged()
{
	OnSkillTreeChanged.Broadcast();
}
