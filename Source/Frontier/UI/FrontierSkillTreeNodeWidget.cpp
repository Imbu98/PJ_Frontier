#include "UI/FrontierSkillTreeNodeWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/FrontierSkillTreeComponent.h"
#include "SkillTree/FrontierSkillTreeDataAsset.h"
#include "TimerManager.h"

namespace
{
const FFrontierSkillTreeConditionalAttackModifier* FindWeaponAttackModifier(
	const FFrontierSkillTreeNodeDefinition& Definition)
{
	for (const FFrontierSkillTreeRankDefinition& Rank : Definition.Ranks)
	{
		if (const FFrontierSkillTreeConditionalAttackModifier* Modifier =
			Rank.ConditionalAttackModifiers.FindByPredicate(
			[](const FFrontierSkillTreeConditionalAttackModifier& Modifier)
			{
				return Modifier.WeaponFamilyTag.IsValid();
			}))
		{
			return Modifier;
		}
	}

	return nullptr;
}
}

void UFrontierSkillTreeNodeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (NodeButton)
	{
		NodeButton->OnClicked.AddUniqueDynamic(this, &UFrontierSkillTreeNodeWidget::HandleNodeButtonClicked);
	}
	RefreshNode();
}

void UFrontierSkillTreeNodeWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoverDetailTimerHandle);
	}
	if (NodeButton)
	{
		NodeButton->OnClicked.RemoveDynamic(this, &UFrontierSkillTreeNodeWidget::HandleNodeButtonClicked);
	}
	Super::NativeDestruct();
}

void UFrontierSkillTreeNodeWidget::NativeOnMouseEnter(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bPointerInsideNode = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			HoverDetailTimerHandle,
			this,
			&UFrontierSkillTreeNodeWidget::HandleHoverDetailDelayElapsed,
			FMath::Max(0.0f, HoverDetailDelay),
			false);
	}
}

void UFrontierSkillTreeNodeWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	bPointerInsideNode = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoverDetailTimerHandle);
	}
	OnNodeHoverEnded.Broadcast();
	Super::NativeOnMouseLeave(InMouseEvent);
}

void UFrontierSkillTreeNodeWidget::InitializeSkillTreeNode(
	UFrontierSkillTreeComponent* InSkillTreeComponent,
	const UFrontierSkillTreeDataAsset* InSkillTreeData)
{
	SkillTreeComponent = InSkillTreeComponent;
	SkillTreeData = InSkillTreeData;
	RefreshNode();
}

int32 UFrontierSkillTreeNodeWidget::GetCurrentRank() const
{
	return SkillTreeComponent && NodeTag.IsValid()
		? SkillTreeComponent->GetNodeRank(NodeTag)
		: 0;
}

void UFrontierSkillTreeNodeWidget::SetVisualPalette(
	const FFrontierSkillTreeVisualPalette& InVisualPalette)
{
	VisualPalette = InVisualPalette;
}

void UFrontierSkillTreeNodeWidget::RefreshNode()
{
	FFrontierSkillTreeNodeDefinition Definition;
	if (!SkillTreeComponent || !SkillTreeData || !NodeTag.IsValid()
		|| !SkillTreeData->GetNodeDefinition(NodeTag, Definition))
	{
		VisualState = EFrontierSkillTreeNodeVisualState::Invalid;
		LastEligibilityResult = EFrontierSkillTreeRequestResult::InvalidData;
		if (NodeButton)
		{
			NodeButton->SetIsEnabled(false);
		}
		ApplyBorderStyle(FGameplayTag(), false);
		BP_OnNodeVisualStateChanged(VisualState, 0, 0, LastEligibilityResult);
		return;
	}

	const int32 CurrentRank = SkillTreeComponent->GetNodeRank(NodeTag);
	const int32 MaxRank = FMath::Max(1, Definition.MaxRank);
	int32 EligibilityPointCost = 0;
	LastEligibilityResult = SkillTreeComponent->CanUnlockNode(NodeTag, EligibilityPointCost);
	const int32 DisplayPointCost = SkillTreeData->GetRankCost(Definition, CurrentRank);

	if (CurrentRank >= MaxRank)
	{
		VisualState = EFrontierSkillTreeNodeVisualState::MaxRank;
	}
	else if (LastEligibilityResult == EFrontierSkillTreeRequestResult::Success)
	{
		VisualState = CurrentRank > 0
			? EFrontierSkillTreeNodeVisualState::Invested
			: EFrontierSkillTreeNodeVisualState::Available;
	}
	else if (LastEligibilityResult == EFrontierSkillTreeRequestResult::InsufficientPoints)
	{
		VisualState = EFrontierSkillTreeNodeVisualState::InsufficientPoints;
	}
	else
	{
		VisualState = EFrontierSkillTreeNodeVisualState::Locked;
	}

	UImage* PrimaryIconImage = IconImage ? IconImage.Get() : NodeIconImage.Get();
	if (PrimaryIconImage)
	{
		PrimaryIconImage->SetBrushFromTexture(Definition.Icon, true);
		PrimaryIconImage->SetVisibility(
			Definition.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (ElementImage)
	{
		const FFrontierSkillTreeConditionalAttackModifier* WeaponAttackModifier =
			FindWeaponAttackModifier(Definition);
		const bool bShowElementImage = WeaponAttackModifier && Definition.ElementIcon;
		ElementImage->SetBrushFromTexture(Definition.ElementIcon, true);
		ElementImage->SetColorAndOpacity(
			WeaponAttackModifier
				? VisualPalette.ResolveElementTint(WeaponAttackModifier->ElementalType)
				: FLinearColor::White);
		ElementImage->SetVisibility(
			bShowElementImage ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (NodeNameText)
	{
		NodeNameText->SetText(Definition.DisplayName);
	}
	if (NodeDescriptionText)
	{
		NodeDescriptionText->SetText(Definition.Description);
	}
	if (NodeRankText)
	{
		NodeRankText->SetText(FText::Format(
			NSLOCTEXT("FrontierSkillTreeUI", "NodeRankFormat", "{0}/{1}"),
			FText::AsNumber(CurrentRank),
			FText::AsNumber(MaxRank)));
	}
	if (NodeCostText)
	{
		NodeCostText->SetText(CurrentRank >= MaxRank ? FText::GetEmpty() : FText::AsNumber(DisplayPointCost));
	}
	if (NodeButton)
	{
		// Locked nodes must remain clickable so the parent can explain the missing prerequisite.
		NodeButton->SetIsEnabled(true);
	}

	const FGameplayTag VisualCategoryTag = SkillTreeData->GetNodeVisualCategoryTag(NodeTag);
	ApplyBorderStyle(VisualCategoryTag, CurrentRank > 0);

	const bool bLocked = VisualState == EFrontierSkillTreeNodeVisualState::Locked
		|| VisualState == EFrontierSkillTreeNodeVisualState::Invalid;
	if (LockedOverlay)
	{
		LockedOverlay->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (AvailableIndicator)
	{
		AvailableIndicator->SetVisibility(
			VisualState == EFrontierSkillTreeNodeVisualState::Available
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	if (InvestedIndicator)
	{
		InvestedIndicator->SetVisibility(
			VisualState == EFrontierSkillTreeNodeVisualState::Invested
				|| (CurrentRank > 0 && VisualState == EFrontierSkillTreeNodeVisualState::InsufficientPoints)
					? ESlateVisibility::HitTestInvisible
					: ESlateVisibility::Collapsed);
	}
	if (MaxRankIndicator)
	{
		MaxRankIndicator->SetVisibility(
			VisualState == EFrontierSkillTreeNodeVisualState::MaxRank
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}

	BP_OnNodeVisualStateChanged(VisualState, CurrentRank, MaxRank, LastEligibilityResult);
}

void UFrontierSkillTreeNodeWidget::ApplyBorderStyle(
	const FGameplayTag TreeCategoryTag,
	const bool bUnlocked)
{
	if (bUnlocked)
	{
		VisualPalette.ResolveCategoryColors(
			TreeCategoryTag,
			ResolvedBorderPrimaryColor,
			ResolvedBorderAccentColor);
	}
	else
	{
		ResolvedBorderPrimaryColor = VisualPalette.LockedPrimary;
		ResolvedBorderAccentColor = VisualPalette.LockedAccent;
	}

	if (NodeBorder)
	{
		NodeBorder->SetBrushColor(ResolvedBorderPrimaryColor);
	}
	if (NodeBorderImage)
	{
		NodeBorderImage->SetColorAndOpacity(ResolvedBorderPrimaryColor);
	}
	if (NodeBorderAccentImage)
	{
		NodeBorderAccentImage->SetColorAndOpacity(ResolvedBorderAccentColor);
	}
	BP_OnNodeBorderStyleChanged(
		TreeCategoryTag,
		ResolvedBorderPrimaryColor,
		ResolvedBorderAccentColor,
		bUnlocked);
}

void UFrontierSkillTreeNodeWidget::HandleNodeButtonClicked()
{
	if (!SkillTreeComponent || !NodeTag.IsValid())
	{
		return;
	}

	const double CurrentTimeSeconds = FPlatformTime::Seconds();
	if (LastNodeClickTimeSeconds >= 0.0
		&& CurrentTimeSeconds - LastNodeClickTimeSeconds <= FMath::Max(0.1f, DoubleClickInterval))
	{
		LastNodeClickTimeSeconds = -1.0;
		OnNodeDoubleClicked.Broadcast(NodeTag);
		return;
	}
	LastNodeClickTimeSeconds = CurrentTimeSeconds;
}

void UFrontierSkillTreeNodeWidget::HandleHoverDetailDelayElapsed()
{
	if (bPointerInsideNode && NodeTag.IsValid())
	{
		OnNodeDetailsRequested.Broadcast(NodeTag);
	}
}
