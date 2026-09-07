#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkillTree/FrontierSkillTreeTypes.h"
#include "UI/FrontierCommonPopupTypes.h"
#include "FrontierSkillTreeWidget.generated.h"

class UButton;
class UFrontierSkillTreeComponent;
class UFrontierSkillTreeConnectionLayerWidget;
class UFrontierSkillTreeDataAsset;
class UFrontierSkillTreeNodeDetailPopupWidget;
class UFrontierSkillTreeNodeWidget;
class UFrontierPopupSubsystem;
class UTextBlock;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFrontierSkillTreeCloseRequestedSignature);

/** Parent class for the manually arranged skill-tree screen WBP. */
UCLASS(Abstract, Blueprintable)
class FRONTIER_API UFrontierSkillTreeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Frontier|Skill Tree")
	void RefreshSkillTree();

	/** Restores the authored starting position and zoom of the tree content. */
	UFUNCTION(BlueprintCallable, Category="Frontier|Skill Tree|Navigation")
	void ResetTreeView();

	UFUNCTION(BlueprintPure, Category="Frontier|Skill Tree")
	UFrontierSkillTreeComponent* GetBoundSkillTreeComponent() const { return SkillTreeComponent; }

	UPROPERTY(BlueprintAssignable, Category="Frontier|Skill Tree")
	FFrontierSkillTreeCloseRequestedSignature OnCloseRequested;

	/** Optional automatic lines between manually positioned node widgets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Connections")
	bool bDrawConnections = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Connections", meta=(ClampMin="0.5"))
	float ConnectionThickness = 3.0f;

	/** Keeps lines from crossing over circular node artwork. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Connections", meta=(ClampMin="0.0"))
	float ConnectionEndPadding = 24.0f;

	/** One editable palette drives both authored WBP node borders and automatic lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Visuals")
	FFrontierSkillTreeVisualPalette VisualPalette;

	/** Enables dragging empty space inside TreeViewport to move TreeContentCanvas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Navigation")
	bool bEnablePan = true;

	/** Enables mouse-wheel zoom while the pointer is inside TreeViewport. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Navigation")
	bool bEnableZoom = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Navigation")
	FVector2D InitialPanOffset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Navigation", meta=(ClampMin="0.01"))
	float InitialZoom = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Navigation", meta=(ClampMin="0.01"))
	float MinimumZoom = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Navigation", meta=(ClampMin="0.01"))
	float MaximumZoom = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Navigation", meta=(ClampMin="0.01"))
	float ZoomStep = 0.1f;

	/** Maximum authored-space movement from InitialPanOffset on each axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Navigation", meta=(ClampMin="0.0"))
	FVector2D PanLimit = FVector2D(2000.0f, 2000.0f);

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Skill Tree|Navigation")
	FVector2D CurrentPanOffset = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Frontier|Skill Tree|Navigation")
	float CurrentZoom = 1.0f;

	/** Gap between the hovered node and the separately-authored detail popup WBP. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Details")
	FVector2D NodeDetailPopupOffset = FVector2D(18.0f, 0.0f);

	/** Keeps the detail popup inside TreeViewport and flips it to the node's left near the right edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Details")
	bool bKeepNodeDetailPopupInsideViewport = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Details", meta=(ClampMin="0.0"))
	float NodeDetailPopupEdgePadding = 12.0f;

	/**
	 * Double-click is already an intentional action, so unlock directly by default.
	 * Enable this only after a usable common confirmation popup WBP is configured.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Skill Tree|Interaction")
	bool bConfirmNodeUnlock = false;

	UFUNCTION(BlueprintImplementableEvent, Category="Frontier|Skill Tree", meta=(DisplayName="On Skill Tree Request Completed"))
	void BP_OnSkillTreeRequestCompleted(FGameplayTag NodeTag, EFrontierSkillTreeRequestResult Result);

	UFUNCTION(BlueprintImplementableEvent, Category="Frontier|Skill Tree", meta=(DisplayName="On Warning Popup Shown"))
	void BP_OnWarningPopupShown(const FText& Message);

	UFUNCTION(BlueprintImplementableEvent, Category="Frontier|Skill Tree", meta=(DisplayName="On Node Detail Popup Shown"))
	void BP_OnNodeDetailPopupShown(FGameplayTag NodeTag);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	UFUNCTION()
	void HandleSkillTreeChanged();

	UFUNCTION()
	void HandleSkillTreeRequestCompleted(FGameplayTag NodeTag, EFrontierSkillTreeRequestResult Result);

	UFUNCTION()
	void HandleResetButtonClicked();

	UFUNCTION()
	void HandleCloseButtonClicked();

	UFUNCTION()
	void HandleNodeDoubleClicked(FGameplayTag NodeTag);

	UFUNCTION()
	void HandleNodeDetailsRequested(FGameplayTag NodeTag);

	UFUNCTION()
	void HandleNodeHoverEnded();

	UFUNCTION()
	void HandleCommonPopupResolved(int32 RequestId, EFrontierPopupResult Result);

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> AvailableSkillPointsText;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> ResetButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	/** Clips the movable tree. Use a Border with Clipping set to Clip to Bounds. */
	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> TreeViewport;

	/** Container that owns every node and receives the pan/zoom render transform. */
	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> TreeContentCanvas;

	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierSkillTreeNodeDetailPopupWidget> NodeDetailPopupWidget;

private:
	void AttemptBindSkillTree();
	bool EnsurePopupSubsystem();
	void EnsureConnectionLayer();
	void DiscoverNodeWidgets();
	void ShowWarning(const FText& Message);
	void ExecutePendingNodeUnlock();
	void HideNodeDetailPopup();
	void ApplyTreeViewTransform();
	void ClampPanOffset();
	void UpdateNodeDetailPopupPosition();
	UFrontierSkillTreeNodeWidget* FindNodeWidget(FGameplayTag NodeTag) const;
	bool IsPointerInsideTreeViewport(const FVector2D& ScreenSpacePosition) const;
	bool IsModalPopupVisible() const;
	void PaintSkillTreeConnections(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId) const;

	friend class SFrontierSkillTreeConnectionLayer;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierSkillTreeComponent> SkillTreeComponent;

	/** Referenced by SkillTreeComponent for at least the lifetime of this widget binding. */
	const UFrontierSkillTreeDataAsset* SkillTreeData = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFrontierSkillTreeNodeWidget>> NodeWidgets;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierSkillTreeConnectionLayerWidget> ConnectionLayerWidget;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierPopupSubsystem> PopupSubsystem;

	FGameplayTag PendingUnlockNodeTag;
	FGameplayTag ActiveDetailNodeTag;
	int32 UnlockPopupRequestId = INDEX_NONE;
	int32 ResetPopupRequestId = INDEX_NONE;

	FTimerHandle BindRetryTimerHandle;
	FVector2D LastPanScreenPosition = FVector2D::ZeroVector;
	bool bIsPanning = false;
};
