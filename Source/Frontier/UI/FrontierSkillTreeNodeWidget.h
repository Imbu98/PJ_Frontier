#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SkillTree/FrontierSkillTreeTypes.h"
#include "FrontierSkillTreeNodeWidget.generated.h"

class UButton;
class UBorder;
class UFrontierSkillTreeComponent;
class UFrontierSkillTreeDataAsset;
class UImage;
class UTextBlock;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FFrontierSkillTreeNodeInteractionSignature,
	FGameplayTag,
	NodeTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFrontierSkillTreeNodeHoverEndedSignature);

UENUM(BlueprintType)
enum class EFrontierSkillTreeNodeVisualState : uint8
{
	Invalid,
	Locked,
	Available,
	Invested,
	InsufficientPoints,
	MaxRank
};

/** Parent class for a manually placed skill-tree node WBP. */
UCLASS(Abstract, Blueprintable)
class FRONTIER_API UFrontierSkillTreeNodeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Set this per node instance in the parent WBP designer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree", meta=(ExposeOnSpawn=true))
	FGameplayTag NodeTag;

	UFUNCTION(BlueprintCallable, Category="Frontier|Skill Tree")
	void InitializeSkillTreeNode(
		UFrontierSkillTreeComponent* InSkillTreeComponent,
		const UFrontierSkillTreeDataAsset* InSkillTreeData);

	UFUNCTION(BlueprintCallable, Category="Frontier|Skill Tree")
	void RefreshNode();

	/** Called by the owning skill-tree screen so nodes and lines always share one palette. */
	UFUNCTION(BlueprintCallable, Category="Frontier|Skill Tree|Visuals")
	void SetVisualPalette(const FFrontierSkillTreeVisualPalette& InVisualPalette);

	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree")
	int32 GetCurrentRank() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree")
	EFrontierSkillTreeNodeVisualState GetVisualState() const { return VisualState; }

	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree")
	EFrontierSkillTreeRequestResult GetLastEligibilityResult() const { return LastEligibilityResult; }

	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree|Visuals")
	FLinearColor GetResolvedBorderPrimaryColor() const { return ResolvedBorderPrimaryColor; }

	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree|Visuals")
	FLinearColor GetResolvedBorderAccentColor() const { return ResolvedBorderAccentColor; }

	/** Emitted only after two NodeButton clicks inside DoubleClickInterval. */
	UPROPERTY(BlueprintAssignable, Category="Frontier|Skill Tree|Interaction")
	FFrontierSkillTreeNodeInteractionSignature OnNodeDoubleClicked;

	/** Emitted after the pointer remains over this node for HoverDetailDelay. */
	UPROPERTY(BlueprintAssignable, Category="Frontier|Skill Tree|Interaction")
	FFrontierSkillTreeNodeInteractionSignature OnNodeDetailsRequested;

	UPROPERTY(BlueprintAssignable, Category="Frontier|Skill Tree|Interaction")
	FFrontierSkillTreeNodeHoverEndedSignature OnNodeHoverEnded;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Interaction", meta=(ClampMin="0.1"))
	float DoubleClickInterval = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Interaction", meta=(ClampMin="0.0"))
	float HoverDetailDelay = 0.3f;

	/** Implement in WBP only when custom colors/animations are needed. */
	UFUNCTION(BlueprintImplementableEvent, Category="Frontier|Skill Tree", meta=(DisplayName="On Node Visual State Changed"))
	void BP_OnNodeVisualStateChanged(
		EFrontierSkillTreeNodeVisualState NewState,
		int32 CurrentRank,
		int32 MaxRank,
		EFrontierSkillTreeRequestResult EligibilityResult);

	/**
	 * Use this event to feed Primary/Accent into an authored border material.
	 * C++ never creates or replaces the WBP border artwork.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Frontier|Skill Tree|Visuals", meta=(DisplayName="On Node Border Style Changed"))
	void BP_OnNodeBorderStyleChanged(
		FGameplayTag TreeCategoryTag,
		FLinearColor PrimaryColor,
		FLinearColor AccentColor,
		bool bUnlocked);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UFUNCTION()
	void HandleNodeButtonClicked();

	void HandleHoverDetailDelayElapsed();

	/** Required only if clicking the node should invest a point. */
	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> NodeButton;

	/** Optional existing WBP border. Name it NodeBorder to receive the primary tint directly. */
	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UBorder> NodeBorder;

	/** Optional existing WBP border image. Name it NodeBorderImage to receive the primary tint. */
	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> NodeBorderImage;

	/** Optional second authored border layer for the accent tint. */
	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> NodeBorderAccentImage;

	/** Primary icon layer. In WBP, place it above ElementImage. */
	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	/** Optional outer effect layer, shown only for weapon attack nodes with an Element Icon. */
	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> ElementImage;

	/** Backward-compatible primary icon name used by existing node WBPs. */
	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> NodeIconImage;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> NodeNameText;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> NodeDescriptionText;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> NodeRankText;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> NodeCostText;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> LockedOverlay;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> AvailableIndicator;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> InvestedIndicator;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> MaxRankIndicator;

private:
	void ApplyBorderStyle(FGameplayTag TreeCategoryTag, bool bUnlocked);

	UPROPERTY(Transient)
	TObjectPtr<UFrontierSkillTreeComponent> SkillTreeComponent;

	/** Referenced by SkillTreeComponent for at least the lifetime of this widget binding. */
	const UFrontierSkillTreeDataAsset* SkillTreeData = nullptr;

	UPROPERTY(VisibleInstanceOnly, Category="Frontier|Skill Tree")
	EFrontierSkillTreeNodeVisualState VisualState = EFrontierSkillTreeNodeVisualState::Invalid;

	UPROPERTY(VisibleInstanceOnly, Category="Frontier|Skill Tree")
	EFrontierSkillTreeRequestResult LastEligibilityResult = EFrontierSkillTreeRequestResult::InvalidData;

	UPROPERTY(Transient)
	FFrontierSkillTreeVisualPalette VisualPalette;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Frontier|Skill Tree|Visuals", meta=(AllowPrivateAccess="true"))
	FLinearColor ResolvedBorderPrimaryColor = FLinearColor::Transparent;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Frontier|Skill Tree|Visuals", meta=(AllowPrivateAccess="true"))
	FLinearColor ResolvedBorderAccentColor = FLinearColor::Transparent;

	FTimerHandle HoverDetailTimerHandle;
	double LastNodeClickTimeSeconds = -1.0;
	bool bPointerInsideNode = false;
};
