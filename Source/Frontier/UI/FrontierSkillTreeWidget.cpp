#include "UI/FrontierSkillTreeWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/FrontierSkillTreeComponent.h"
#include "Game/FrontierPlayerState.h"
#include "Game/FrontierLobbyPlayerController.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "SkillTree/FrontierSkillTreeDataAsset.h"
#include "TimerManager.h"
#include "UI/FrontierPopupSubsystem.h"
#include "UI/FrontierSkillTreeConnectionLayerWidget.h"
#include "UI/FrontierSkillTreeNodeWidget.h"
#include "UI/FrontierSkillTreeNodeDetailPopupWidget.h"

namespace
{
constexpr float SkillTreeBindRetryDelaySeconds = 0.2f;

bool GetCanvasSlotCenter(const UWidget* Widget, const FVector2D& CanvasSize, FVector2D& OutCenter)
{
	const UCanvasPanelSlot* CanvasSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
	if (!CanvasSlot)
	{
		return false;
	}

	const FMargin Offset = CanvasSlot->GetOffsets();
	const FAnchors Anchors = CanvasSlot->GetAnchors();
	const FVector2D Alignment = CanvasSlot->GetAlignment();
	const FMargin AnchorPixels(
		Anchors.Minimum.X * CanvasSize.X,
		Anchors.Minimum.Y * CanvasSize.Y,
		Anchors.Maximum.X * CanvasSize.X,
		Anchors.Maximum.Y * CanvasSize.Y);
	const bool bIsHorizontalStretch = Anchors.Minimum.X != Anchors.Maximum.X;
	const bool bIsVerticalStretch = Anchors.Minimum.Y != Anchors.Maximum.Y;
	const FVector2D SlotSize(Offset.Right, Offset.Bottom);
	const FVector2D Size = CanvasSlot->GetAutoSize() ? Widget->GetDesiredSize() : SlotSize;
	const FVector2D AlignmentOffset = Size * Alignment;

	FVector2D LocalPosition;
	FVector2D LocalSize;
	if (bIsHorizontalStretch)
	{
		LocalPosition.X = AnchorPixels.Left + Offset.Left;
		LocalSize.X = AnchorPixels.Right - LocalPosition.X - Offset.Right;
	}
	else
	{
		LocalPosition.X = AnchorPixels.Left + Offset.Left - AlignmentOffset.X;
		LocalSize.X = Size.X;
	}

	if (bIsVerticalStretch)
	{
		LocalPosition.Y = AnchorPixels.Top + Offset.Top;
		LocalSize.Y = AnchorPixels.Bottom - LocalPosition.Y - Offset.Bottom;
	}
	else
	{
		LocalPosition.Y = AnchorPixels.Top + Offset.Top - AlignmentOffset.Y;
		LocalSize.Y = Size.Y;
	}

	OutCenter = LocalPosition + LocalSize * 0.5f;
	return true;
}

FText GetSkillTreeRequestFailureMessage(const EFrontierSkillTreeRequestResult Result)
{
	switch (Result)
	{
	case EFrontierSkillTreeRequestResult::MissingPrerequisite:
		return NSLOCTEXT("FrontierSkillTreeUI", "MissingPrerequisiteWarning", "이전 노드를 먼저 해금하여 주십시오.");
	case EFrontierSkillTreeRequestResult::InsufficientPoints:
		return NSLOCTEXT("FrontierSkillTreeUI", "InsufficientPointsWarning", "스킬 포인트가 부족합니다.");
	case EFrontierSkillTreeRequestResult::AlreadyMaxRank:
		return NSLOCTEXT("FrontierSkillTreeUI", "AlreadyMaxRankWarning", "이미 최대 랭크까지 해금한 노드입니다.");
	case EFrontierSkillTreeRequestResult::InvalidContext:
		return NSLOCTEXT("FrontierSkillTreeUI", "InvalidContextWarning", "스킬트리는 로비에서만 변경할 수 있습니다.");
	case EFrontierSkillTreeRequestResult::NotAuthority:
		return NSLOCTEXT("FrontierSkillTreeUI", "NotAuthorityWarning", "서버에서 스킬트리 변경 권한을 확인하지 못했습니다.");
	case EFrontierSkillTreeRequestResult::InvalidData:
		return NSLOCTEXT("FrontierSkillTreeUI", "InvalidDataWarning", "스킬트리 정보를 불러올 수 없습니다.");
	case EFrontierSkillTreeRequestResult::InvalidNode:
	default:
		return NSLOCTEXT("FrontierSkillTreeUI", "InvalidNodeWarning", "현재 이 노드를 해금할 수 없습니다.");
	}
}
}

void UFrontierSkillTreeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (ResetButton)
	{
		ResetButton->OnClicked.AddUniqueDynamic(this, &UFrontierSkillTreeWidget::HandleResetButtonClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UFrontierSkillTreeWidget::HandleCloseButtonClicked);
	}
	EnsurePopupSubsystem();
	HideNodeDetailPopup();
	ResetTreeView();

	DiscoverNodeWidgets();
	EnsureConnectionLayer();
	AttemptBindSkillTree();
}

void UFrontierSkillTreeWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimerHandle);
	}
	if (SkillTreeComponent)
	{
		SkillTreeComponent->OnSkillTreeChanged.RemoveDynamic(this, &UFrontierSkillTreeWidget::HandleSkillTreeChanged);
		SkillTreeComponent->OnSkillTreeRequestCompleted.RemoveDynamic(
			this,
			&UFrontierSkillTreeWidget::HandleSkillTreeRequestCompleted);
	}
	if (ResetButton)
	{
		ResetButton->OnClicked.RemoveDynamic(this, &UFrontierSkillTreeWidget::HandleResetButtonClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UFrontierSkillTreeWidget::HandleCloseButtonClicked);
	}
	if (PopupSubsystem)
	{
		PopupSubsystem->OnPopupResolved.RemoveDynamic(this, &UFrontierSkillTreeWidget::HandleCommonPopupResolved);
		const int32 PendingUnlockRequestId = UnlockPopupRequestId;
		const int32 PendingResetRequestId = ResetPopupRequestId;
		UnlockPopupRequestId = INDEX_NONE;
		ResetPopupRequestId = INDEX_NONE;
		PopupSubsystem->CancelPopup(PendingUnlockRequestId);
		PopupSubsystem->CancelPopup(PendingResetRequestId);
	}
	bIsPanning = false;
	Super::NativeDestruct();
}

void UFrontierSkillTreeWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (ActiveDetailNodeTag.IsValid())
	{
		UpdateNodeDetailPopupPosition();
	}
}

FReply UFrontierSkillTreeWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	const FKey EffectingButton = InMouseEvent.GetEffectingButton();
	const bool bPanButton = EffectingButton == EKeys::LeftMouseButton
		|| EffectingButton == EKeys::MiddleMouseButton;
	if (bEnablePan && TreeContentCanvas && bPanButton && !IsModalPopupVisible()
		&& IsPointerInsideTreeViewport(InMouseEvent.GetScreenSpacePosition()))
	{
		bIsPanning = true;
		LastPanScreenPosition = InMouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UFrontierSkillTreeWidget::NativeOnMouseButtonUp(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (bIsPanning)
	{
		bIsPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UFrontierSkillTreeWidget::NativeOnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (bIsPanning && TreeContentCanvas)
	{
		const FVector2D CurrentScreenPosition = InMouseEvent.GetScreenSpacePosition();
		const FGeometry& ReferenceGeometry = TreeViewport
			? TreeViewport->GetCachedGeometry()
			: InGeometry;
		const FVector2D LocalDelta = ReferenceGeometry.AbsoluteToLocal(CurrentScreenPosition)
			- ReferenceGeometry.AbsoluteToLocal(LastPanScreenPosition);
		LastPanScreenPosition = CurrentScreenPosition;
		CurrentPanOffset += LocalDelta;
		ClampPanOffset();
		ApplyTreeViewTransform();
		return FReply::Handled();
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UFrontierSkillTreeWidget::NativeOnMouseWheel(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!bEnableZoom || !TreeContentCanvas || IsModalPopupVisible()
		|| !IsPointerInsideTreeViewport(InMouseEvent.GetScreenSpacePosition()))
	{
		return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	}

	const float SafeMinimumZoom = FMath::Max(0.01f, MinimumZoom);
	const float SafeMaximumZoom = FMath::Max(SafeMinimumZoom, MaximumZoom);
	const float PreviousZoom = FMath::Clamp(CurrentZoom, SafeMinimumZoom, SafeMaximumZoom);
	const float NewZoom = FMath::Clamp(
		PreviousZoom + InMouseEvent.GetWheelDelta() * FMath::Max(0.01f, ZoomStep),
		SafeMinimumZoom,
		SafeMaximumZoom);
	if (FMath::IsNearlyEqual(PreviousZoom, NewZoom))
	{
		return FReply::Handled();
	}

	// Offset the pan so zooming remains centered near the mouse cursor rather than the screen center.
	const FGeometry& ViewportGeometry = TreeViewport
		? TreeViewport->GetCachedGeometry()
		: InGeometry;
	const FGeometry& ContentGeometry = TreeContentCanvas->GetCachedGeometry();
	const FVector2D CursorInViewport = ViewportGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D ContentPivotAbsolute = ContentGeometry.LocalToAbsolute(ContentGeometry.GetLocalSize() * 0.5f);
	const FVector2D ContentPivotInViewport = ViewportGeometry.AbsoluteToLocal(ContentPivotAbsolute);
	CurrentPanOffset += (1.0f - NewZoom / PreviousZoom) * (CursorInViewport - ContentPivotInViewport);
	CurrentZoom = NewZoom;
	ClampPanOffset();
	ApplyTreeViewTransform();
	return FReply::Handled();
}

FReply UFrontierSkillTreeWidget::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
#if !UE_BUILD_SHIPPING
	if (!InKeyEvent.IsRepeat() && InKeyEvent.GetKey() == EKeys::P)
	{
		if (AFrontierLobbyPlayerController* LobbyController = Cast<AFrontierLobbyPlayerController>(GetOwningPlayer()))
		{
			LobbyController->RequestDebugSkillPoints();
			return FReply::Handled();
		}
	}
#endif
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UFrontierSkillTreeWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	bIsPanning = false;
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

void UFrontierSkillTreeWidget::ResetTreeView()
{
	const float SafeMinimumZoom = FMath::Max(0.01f, MinimumZoom);
	const float SafeMaximumZoom = FMath::Max(SafeMinimumZoom, MaximumZoom);
	CurrentZoom = FMath::Clamp(InitialZoom, SafeMinimumZoom, SafeMaximumZoom);
	CurrentPanOffset = InitialPanOffset;
	ClampPanOffset();
	ApplyTreeViewTransform();
}

void UFrontierSkillTreeWidget::ApplyTreeViewTransform()
{
	if (!TreeContentCanvas)
	{
		return;
	}

	FWidgetTransform Transform = TreeContentCanvas->GetRenderTransform();
	Transform.Translation = CurrentPanOffset;
	Transform.Scale = FVector2D(CurrentZoom, CurrentZoom);
	TreeContentCanvas->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	TreeContentCanvas->SetRenderTransform(Transform);
	InvalidateLayoutAndVolatility();
}

void UFrontierSkillTreeWidget::ClampPanOffset()
{
	const FVector2D SafeLimit(FMath::Max(0.0f, PanLimit.X), FMath::Max(0.0f, PanLimit.Y));
	CurrentPanOffset.X = FMath::Clamp(
		CurrentPanOffset.X,
		InitialPanOffset.X - SafeLimit.X,
		InitialPanOffset.X + SafeLimit.X);
	CurrentPanOffset.Y = FMath::Clamp(
		CurrentPanOffset.Y,
		InitialPanOffset.Y - SafeLimit.Y,
		InitialPanOffset.Y + SafeLimit.Y);
}

bool UFrontierSkillTreeWidget::IsPointerInsideTreeViewport(const FVector2D& ScreenSpacePosition) const
{
	const UWidget* HitArea = TreeViewport ? TreeViewport.Get() : TreeContentCanvas.Get();
	return HitArea && HitArea->GetCachedGeometry().IsUnderLocation(ScreenSpacePosition);
}

bool UFrontierSkillTreeWidget::IsModalPopupVisible() const
{
	return PopupSubsystem && PopupSubsystem->IsPopupVisible();
}

bool UFrontierSkillTreeWidget::EnsurePopupSubsystem()
{
	if (!PopupSubsystem)
	{
		PopupSubsystem = UFrontierPopupSubsystem::Get(this);
	}
	if (PopupSubsystem)
	{
		PopupSubsystem->OnPopupResolved.AddUniqueDynamic(this, &UFrontierSkillTreeWidget::HandleCommonPopupResolved);
		return true;
	}
	return false;
}

void UFrontierSkillTreeWidget::AttemptBindSkillTree()
{
	APlayerController* OwnerController = GetOwningPlayer();
	AFrontierPlayerState* PlayerState = OwnerController
		? OwnerController->GetPlayerState<AFrontierPlayerState>()
		: nullptr;
	UFrontierSkillTreeComponent* NewSkillTreeComponent = PlayerState
		? PlayerState->GetSkillTreeComponent()
		: nullptr;
	const UFrontierSkillTreeDataAsset* NewSkillTreeData = NewSkillTreeComponent
		? NewSkillTreeComponent->GetSkillTreeData()
		: nullptr;
	if (!NewSkillTreeComponent || !NewSkillTreeData)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				BindRetryTimerHandle,
				this,
				&UFrontierSkillTreeWidget::AttemptBindSkillTree,
				SkillTreeBindRetryDelaySeconds,
				false);
		}
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimerHandle);
	}
	SkillTreeComponent = NewSkillTreeComponent;
	SkillTreeData = NewSkillTreeData;
	SkillTreeComponent->OnSkillTreeChanged.AddUniqueDynamic(this, &UFrontierSkillTreeWidget::HandleSkillTreeChanged);
	SkillTreeComponent->OnSkillTreeRequestCompleted.AddUniqueDynamic(
		this,
		&UFrontierSkillTreeWidget::HandleSkillTreeRequestCompleted);
	RefreshSkillTree();
}

void UFrontierSkillTreeWidget::DiscoverNodeWidgets()
{
	NodeWidgets.Reset();
	if (!WidgetTree)
	{
		return;
	}

	TArray<UWidget*> AllWidgets;
	WidgetTree->GetAllWidgets(AllWidgets);
	for (UWidget* Widget : AllWidgets)
	{
		if (UFrontierSkillTreeNodeWidget* NodeWidget = Cast<UFrontierSkillTreeNodeWidget>(Widget))
		{
			NodeWidget->OnNodeDoubleClicked.AddUniqueDynamic(this, &UFrontierSkillTreeWidget::HandleNodeDoubleClicked);
			NodeWidget->OnNodeDetailsRequested.AddUniqueDynamic(this, &UFrontierSkillTreeWidget::HandleNodeDetailsRequested);
			NodeWidget->OnNodeHoverEnded.AddUniqueDynamic(this, &UFrontierSkillTreeWidget::HandleNodeHoverEnded);
			NodeWidgets.Add(NodeWidget);
		}
	}
}

void UFrontierSkillTreeWidget::EnsureConnectionLayer()
{
	UCanvasPanel* ContentCanvas = Cast<UCanvasPanel>(TreeContentCanvas);
	if (!ContentCanvas || !WidgetTree)
	{
		return;
	}

	if (!ConnectionLayerWidget)
	{
		ConnectionLayerWidget = WidgetTree->ConstructWidget<UFrontierSkillTreeConnectionLayerWidget>(
			UFrontierSkillTreeConnectionLayerWidget::StaticClass(),
			TEXT("SkillTreeConnectionLayer"));
	}
	if (!ConnectionLayerWidget)
	{
		return;
	}

	ConnectionLayerWidget->InitializeConnectionLayer(this);
	ConnectionLayerWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (ConnectionLayerWidget->GetParent() == ContentCanvas)
	{
		return;
	}

	ConnectionLayerWidget->RemoveFromParent();
	if (UCanvasPanelSlot* LayerSlot = ContentCanvas->AddChildToCanvas(ConnectionLayerWidget))
	{
		LayerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		LayerSlot->SetOffsets(FMargin(0.0f));
		LayerSlot->SetAlignment(FVector2D::ZeroVector);
		LayerSlot->SetAutoSize(false);
		LayerSlot->SetZOrder(MIN_int32 / 2);
	}
}

void UFrontierSkillTreeWidget::RefreshSkillTree()
{
	if (!SkillTreeComponent || !SkillTreeData)
	{
		AttemptBindSkillTree();
		return;
	}

	if (AvailableSkillPointsText)
	{
		AvailableSkillPointsText->SetText(FText::AsNumber(SkillTreeComponent->GetAvailableSkillPoints()));
	}
	for (UFrontierSkillTreeNodeWidget* NodeWidget : NodeWidgets)
	{
		if (NodeWidget)
		{
			NodeWidget->SetVisualPalette(VisualPalette);
			NodeWidget->InitializeSkillTreeNode(SkillTreeComponent, SkillTreeData);
		}
	}
	InvalidateLayoutAndVolatility();
}

void UFrontierSkillTreeWidget::HandleSkillTreeChanged()
{
	RefreshSkillTree();
}

void UFrontierSkillTreeWidget::HandleSkillTreeRequestCompleted(
	const FGameplayTag NodeTag,
	const EFrontierSkillTreeRequestResult Result)
{
	RefreshSkillTree();
	if (Result != EFrontierSkillTreeRequestResult::Success)
	{
		ShowWarning(GetSkillTreeRequestFailureMessage(Result));
	}
	BP_OnSkillTreeRequestCompleted(NodeTag, Result);
}

void UFrontierSkillTreeWidget::HandleResetButtonClicked()
{
	HideNodeDetailPopup();
	if (!EnsurePopupSubsystem())
	{
		return;
	}

	FFrontierPopupRequest Request;
	Request.Type = EFrontierPopupType::Confirmation;
	Request.Title = NSLOCTEXT("FrontierSkillTreeUI", "ResetConfirmTitle", "스킬트리 초기화");
	Request.Message = NSLOCTEXT(
		"FrontierSkillTreeUI",
		"ResetConfirmMessage",
		"투자한 모든 스킬을 초기화하고 포인트를 환불하시겠습니까?");
	Request.bShowCancelButton = true;
	ResetPopupRequestId = PopupSubsystem->ShowPopup(Request);
}

void UFrontierSkillTreeWidget::HandleCloseButtonClicked()
{
	OnCloseRequested.Broadcast();
}

void UFrontierSkillTreeWidget::HandleNodeDoubleClicked(const FGameplayTag NodeTag)
{
	HideNodeDetailPopup();
	if (!SkillTreeComponent || !SkillTreeData)
	{
		ShowWarning(NSLOCTEXT("FrontierSkillTreeUI", "InvalidTreeWarning", "스킬트리 정보를 불러올 수 없습니다."));
		return;
	}

	int32 PointCost = 0;
	const EFrontierSkillTreeRequestResult Result = SkillTreeComponent->CanUnlockNode(NodeTag, PointCost);
	if (Result == EFrontierSkillTreeRequestResult::Success)
	{
		if (!bConfirmNodeUnlock)
		{
			SkillTreeComponent->RequestUnlockNode(NodeTag);
			return;
		}

		PendingUnlockNodeTag = NodeTag;
		FFrontierSkillTreeNodeDefinition Definition;
		const FText NodeName = SkillTreeData->GetNodeDefinition(NodeTag, Definition)
			? Definition.DisplayName
			: FText::FromString(NodeTag.ToString());
		if (!EnsurePopupSubsystem())
		{
			PendingUnlockNodeTag = FGameplayTag();
			return;
		}
		FFrontierPopupRequest Request;
		Request.Type = EFrontierPopupType::Confirmation;
		Request.Title = NSLOCTEXT("FrontierSkillTreeUI", "UnlockConfirmTitle", "스킬 해금");
		Request.Message = FText::Format(
			NSLOCTEXT("FrontierSkillTreeUI", "UnlockConfirmFormat", "{0}을(를) 해금하시겠습니까?"),
			NodeName);
		Request.bShowCancelButton = true;
		UnlockPopupRequestId = PopupSubsystem->ShowPopup(Request);
		if (UnlockPopupRequestId == INDEX_NONE)
		{
			PendingUnlockNodeTag = FGameplayTag();
		}
		return;
	}

	switch (Result)
	{
	case EFrontierSkillTreeRequestResult::MissingPrerequisite:
		ShowWarning(NSLOCTEXT("FrontierSkillTreeUI", "MissingPrerequisiteWarning", "이전 노드를 먼저 해금하여 주십시오."));
		break;
	case EFrontierSkillTreeRequestResult::InsufficientPoints:
		ShowWarning(NSLOCTEXT("FrontierSkillTreeUI", "InsufficientPointsWarning", "스킬 포인트가 부족합니다."));
		break;
	case EFrontierSkillTreeRequestResult::AlreadyMaxRank:
		ShowWarning(NSLOCTEXT("FrontierSkillTreeUI", "AlreadyMaxRankWarning", "이미 최대 랭크까지 해금한 노드입니다."));
		break;
	case EFrontierSkillTreeRequestResult::InvalidContext:
		ShowWarning(NSLOCTEXT("FrontierSkillTreeUI", "InvalidContextWarning", "스킬트리는 로비에서만 변경할 수 있습니다."));
		break;
	default:
		ShowWarning(NSLOCTEXT("FrontierSkillTreeUI", "InvalidNodeWarning", "현재 이 노드를 해금할 수 없습니다."));
		break;
	}
}

void UFrontierSkillTreeWidget::HandleNodeDetailsRequested(const FGameplayTag NodeTag)
{
	if (!SkillTreeComponent || !SkillTreeData || !NodeDetailPopupWidget)
	{
		return;
	}

	if (NodeDetailPopupWidget->ShowNodeDetails(SkillTreeComponent, SkillTreeData, NodeTag))
	{
		ActiveDetailNodeTag = NodeTag;
		UpdateNodeDetailPopupPosition();
		BP_OnNodeDetailPopupShown(NodeTag);
	}
}

void UFrontierSkillTreeWidget::HandleNodeHoverEnded()
{
	HideNodeDetailPopup();
}

void UFrontierSkillTreeWidget::HandleCommonPopupResolved(
	const int32 RequestId,
	const EFrontierPopupResult Result)
{
	if (RequestId == UnlockPopupRequestId)
	{
		UnlockPopupRequestId = INDEX_NONE;
		if (Result == EFrontierPopupResult::Confirmed)
		{
			ExecutePendingNodeUnlock();
		}
		else
		{
			PendingUnlockNodeTag = FGameplayTag();
		}
		return;
	}
	if (RequestId == ResetPopupRequestId)
	{
		ResetPopupRequestId = INDEX_NONE;
		if (Result == EFrontierPopupResult::Confirmed && SkillTreeComponent)
		{
			SkillTreeComponent->RequestResetTree();
		}
	}
}

void UFrontierSkillTreeWidget::ExecutePendingNodeUnlock()
{
	const FGameplayTag RequestedNodeTag = PendingUnlockNodeTag;
	PendingUnlockNodeTag = FGameplayTag();
	if (!SkillTreeComponent || !RequestedNodeTag.IsValid())
	{
		return;
	}

	int32 PointCost = 0;
	const EFrontierSkillTreeRequestResult Eligibility =
		SkillTreeComponent->CanUnlockNode(RequestedNodeTag, PointCost);
	if (Eligibility == EFrontierSkillTreeRequestResult::Success)
	{
		SkillTreeComponent->RequestUnlockNode(RequestedNodeTag);
	}
	else
	{
		HandleNodeDoubleClicked(RequestedNodeTag);
	}
}

void UFrontierSkillTreeWidget::ShowWarning(const FText& Message)
{
	if (EnsurePopupSubsystem())
	{
		PopupSubsystem->ShowMessage(EFrontierPopupType::Warning, FText::GetEmpty(), Message);
	}
	BP_OnWarningPopupShown(Message);
}

void UFrontierSkillTreeWidget::HideNodeDetailPopup()
{
	ActiveDetailNodeTag = FGameplayTag();
	if (NodeDetailPopupWidget)
	{
		NodeDetailPopupWidget->HidePopup();
	}
}

UFrontierSkillTreeNodeWidget* UFrontierSkillTreeWidget::FindNodeWidget(const FGameplayTag NodeTag) const
{
	for (UFrontierSkillTreeNodeWidget* NodeWidget : NodeWidgets)
	{
		if (NodeWidget && NodeWidget->NodeTag == NodeTag)
		{
			return NodeWidget;
		}
	}
	return nullptr;
}

void UFrontierSkillTreeWidget::UpdateNodeDetailPopupPosition()
{
	UWidget* Popup = NodeDetailPopupWidget;
	UFrontierSkillTreeNodeWidget* NodeWidget = FindNodeWidget(ActiveDetailNodeTag);
	if (!Popup || !NodeWidget || !Popup->IsVisible())
	{
		return;
	}

	UPanelWidget* PopupParent = Popup->GetParent();
	const FGeometry& ParentGeometry = PopupParent ? PopupParent->GetCachedGeometry() : GetCachedGeometry();
	const FGeometry& NodeGeometry = NodeWidget->GetCachedGeometry();
	Popup->ForceLayoutPrepass();
	FVector2D PopupSize = Popup->GetDesiredSize();
	if (PopupSize.IsNearlyZero())
	{
		PopupSize = Popup->GetCachedGeometry().GetLocalSize();
	}

	const FVector2D NodeSize = NodeGeometry.GetLocalSize();
	const FVector2D NodeRightCenter = ParentGeometry.AbsoluteToLocal(
		NodeGeometry.LocalToAbsolute(FVector2D(NodeSize.X, NodeSize.Y * 0.5f)));
	const FVector2D NodeLeftCenter = ParentGeometry.AbsoluteToLocal(
		NodeGeometry.LocalToAbsolute(FVector2D(0.0f, NodeSize.Y * 0.5f)));
	FVector2D DesiredPosition(
		NodeRightCenter.X + FMath::Abs(NodeDetailPopupOffset.X),
		NodeRightCenter.Y - PopupSize.Y * 0.5f + NodeDetailPopupOffset.Y);

	if (bKeepNodeDetailPopupInsideViewport)
	{
		const FGeometry& BoundsGeometry = TreeViewport
			? TreeViewport->GetCachedGeometry()
			: GetCachedGeometry();
		const FVector2D BoundsMinimum = ParentGeometry.AbsoluteToLocal(
			BoundsGeometry.LocalToAbsolute(FVector2D::ZeroVector));
		const FVector2D BoundsMaximum = ParentGeometry.AbsoluteToLocal(
			BoundsGeometry.LocalToAbsolute(BoundsGeometry.GetLocalSize()));
		const float EdgePadding = FMath::Max(0.0f, NodeDetailPopupEdgePadding);

		if (DesiredPosition.X + PopupSize.X > BoundsMaximum.X - EdgePadding)
		{
			DesiredPosition.X = NodeLeftCenter.X - PopupSize.X - FMath::Abs(NodeDetailPopupOffset.X);
		}
		DesiredPosition.X = FMath::Clamp(
			DesiredPosition.X,
			BoundsMinimum.X + EdgePadding,
			FMath::Max(BoundsMinimum.X + EdgePadding, BoundsMaximum.X - PopupSize.X - EdgePadding));
		DesiredPosition.Y = FMath::Clamp(
			DesiredPosition.Y,
			BoundsMinimum.Y + EdgePadding,
			FMath::Max(BoundsMinimum.Y + EdgePadding, BoundsMaximum.Y - PopupSize.Y - EdgePadding));
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Popup->Slot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D::ZeroVector);
		CanvasSlot->SetPosition(DesiredPosition);
		return;
	}

	const FVector2D CurrentPosition = ParentGeometry.AbsoluteToLocal(
		Popup->GetCachedGeometry().LocalToAbsolute(FVector2D::ZeroVector));
	FWidgetTransform Transform = Popup->GetRenderTransform();
	Transform.Translation += DesiredPosition - CurrentPosition;
	Popup->SetRenderTransform(Transform);
}

void UFrontierSkillTreeWidget::PaintSkillTreeConnections(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId) const
{
	if (!bDrawConnections || !SkillTreeComponent || !SkillTreeData)
	{
		return;
	}

	const bool bClipConnectionsToViewport = TreeViewport != nullptr;
	if (bClipConnectionsToViewport)
	{
		OutDrawElements.PushClip(FSlateClippingZone(TreeViewport->GetPaintSpaceGeometry()));
	}

	TMap<FGameplayTag, const UFrontierSkillTreeNodeWidget*> WidgetsByTag;
	for (const UFrontierSkillTreeNodeWidget* NodeWidget : NodeWidgets)
	{
		if (NodeWidget && NodeWidget->NodeTag.IsValid())
		{
			WidgetsByTag.FindOrAdd(NodeWidget->NodeTag) = NodeWidget;
		}
	}

	const FVector2D CanvasSize = AllottedGeometry.GetLocalSize();
	for (const FFrontierSkillTreeConnection& Connection : SkillTreeData->GetAllConnections())
	{
		const UFrontierSkillTreeNodeWidget* const* SourceWidgetPtr = WidgetsByTag.Find(Connection.SourceNodeTag);
		const UFrontierSkillTreeNodeWidget* const* TargetWidgetPtr = WidgetsByTag.Find(Connection.TargetNodeTag);
		if (!SourceWidgetPtr || !TargetWidgetPtr || !*SourceWidgetPtr || !*TargetWidgetPtr)
		{
			continue;
		}

		FVector2D SourcePoint;
		FVector2D TargetPoint;
		if (!GetCanvasSlotCenter(*SourceWidgetPtr, CanvasSize, SourcePoint)
			|| !GetCanvasSlotCenter(*TargetWidgetPtr, CanvasSize, TargetPoint))
		{
			continue;
		}

		const FVector2D Delta = TargetPoint - SourcePoint;
		const float Length = Delta.Size();
		if (Length > KINDA_SMALL_NUMBER)
		{
			const FVector2D Direction = Delta / Length;
			const float EndpointPadding = FMath::Min(
				FMath::Max(0.0f, ConnectionEndPadding),
				Length * 0.45f);
			SourcePoint += Direction * EndpointPadding;
			TargetPoint -= Direction * EndpointPadding;
		}

		const bool bBothNodesUnlocked = SkillTreeComponent->GetNodeRank(Connection.SourceNodeTag) > 0
			&& SkillTreeComponent->GetNodeRank(Connection.TargetNodeTag) > 0;
		FLinearColor SourcePrimary;
		FLinearColor SourceAccent;
		FLinearColor TargetPrimary;
		FLinearColor TargetAccent;
		if (bBothNodesUnlocked)
		{
			VisualPalette.ResolveCategoryColors(
				SkillTreeData->GetNodeVisualCategoryTag(Connection.SourceNodeTag),
				SourcePrimary,
				SourceAccent);
			VisualPalette.ResolveCategoryColors(
				SkillTreeData->GetNodeVisualCategoryTag(Connection.TargetNodeTag),
				TargetPrimary,
				TargetAccent);
		}
		else
		{
			SourcePrimary = VisualPalette.LockedPrimary;
			SourceAccent = VisualPalette.LockedAccent;
			TargetPrimary = VisualPalette.LockedPrimary;
			TargetAccent = VisualPalette.LockedAccent;
		}

		const FVector2D MiddlePoint = FMath::Lerp(SourcePoint, TargetPoint, 0.5f);
		const TArray<FVector2D> LinePoints = {SourcePoint, MiddlePoint, TargetPoint};
		const TArray<FLinearColor> PointColors = {
			SourcePrimary,
			FMath::Lerp(SourceAccent, TargetAccent, 0.5f),
			TargetPrimary};
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			PointColors,
			ESlateDrawEffect::None,
			FLinearColor::White,
			true,
			FMath::Max(0.5f, ConnectionThickness));
	}

	if (bClipConnectionsToViewport)
	{
		OutDrawElements.PopClip();
	}

}
