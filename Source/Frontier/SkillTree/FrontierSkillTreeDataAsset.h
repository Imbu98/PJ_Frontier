#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SkillTree/FrontierSkillTreeTypes.h"
#include "FrontierSkillTreeDataAsset.generated.h"

UCLASS(BlueprintType)
class FRONTIER_API UFrontierSkillTreeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFrontierSkillTreeDataAsset();

	/** Replaces this asset's node list with the 20 weapon/element attacks plus shared/passive nodes. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Frontier|Skill Tree")
	void ResetToDefaultNodes();

	/** Replaces legacy broad attack nodes and preserves all unrelated authored nodes. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Frontier|Skill Tree")
	void MigrateToWeaponElementAttackNodes();

	/** Clears designer-authored connections without changing node names, ranks, costs, or effects. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Frontier|Skill Tree")
	void ClearAllPrerequisites();

	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree")
	bool GetNodeDefinition(FGameplayTag NodeTag, FFrontierSkillTreeNodeDefinition& OutNode) const;

	/** Returns the authored visual category, including migration for legacy sword/axe nodes. */
	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree")
	FGameplayTag GetNodeVisualCategoryTag(FGameplayTag NodeTag) const;

	/** Returns every prerequisite as a directed Source -> Target line for dynamic tree UIs. */
	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree")
	TArray<FFrontierSkillTreeConnection> GetAllConnections() const;

	/** Returns nodes that do not require any other node. */
	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree")
	TArray<FGameplayTag> GetRootNodeTags() const;

	/** Returns every node directly unlocked from SourceNodeTag. */
	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree")
	TArray<FGameplayTag> GetChildNodeTags(FGameplayTag SourceNodeTag) const;

	const FFrontierSkillTreeNodeDefinition* FindNode(FGameplayTag NodeTag) const;
	int32 GetRankCost(const FFrontierSkillTreeNodeDefinition& Node, int32 RankIndex) const;
	const FFrontierSkillTreeRankDefinition* GetRankDefinition(const FFrontierSkillTreeNodeDefinition& Node, int32 RankIndex) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Tree", meta=(TitleProperty="NodeTag"))
	TArray<FFrontierSkillTreeNodeDefinition> Nodes;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
