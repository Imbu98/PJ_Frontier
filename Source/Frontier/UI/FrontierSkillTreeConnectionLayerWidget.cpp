#include "UI/FrontierSkillTreeConnectionLayerWidget.h"

#include "UI/FrontierSkillTreeWidget.h"
#include "Widgets/SLeafWidget.h"

class SFrontierSkillTreeConnectionLayer : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SFrontierSkillTreeConnectionLayer)
		: _OwnerSkillTreeWidget(nullptr)
	{
	}
		SLATE_ARGUMENT(TWeakObjectPtr<UFrontierSkillTreeWidget>, OwnerSkillTreeWidget)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OwnerSkillTreeWidget = InArgs._OwnerSkillTreeWidget;
		SetCanTick(false);
	}

	void SetOwnerSkillTreeWidget(UFrontierSkillTreeWidget* InOwnerSkillTreeWidget)
	{
		OwnerSkillTreeWidget = InOwnerSkillTreeWidget;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		const bool bParentEnabled) const override
	{
		if (const UFrontierSkillTreeWidget* SkillTreeWidget = OwnerSkillTreeWidget.Get())
		{
			SkillTreeWidget->PaintSkillTreeConnections(
				AllottedGeometry,
				OutDrawElements,
				LayerId);
		}
		return LayerId;
	}

	virtual FVector2D ComputeDesiredSize(const float LayoutScaleMultiplier) const override
	{
		return FVector2D::ZeroVector;
	}

private:
	TWeakObjectPtr<UFrontierSkillTreeWidget> OwnerSkillTreeWidget;
};

void UFrontierSkillTreeConnectionLayerWidget::InitializeConnectionLayer(
	UFrontierSkillTreeWidget* InOwnerSkillTreeWidget)
{
	OwnerSkillTreeWidget = InOwnerSkillTreeWidget;
	if (MyConnectionLayer.IsValid())
	{
		MyConnectionLayer->SetOwnerSkillTreeWidget(InOwnerSkillTreeWidget);
	}
}

TSharedRef<SWidget> UFrontierSkillTreeConnectionLayerWidget::RebuildWidget()
{
	return SAssignNew(MyConnectionLayer, SFrontierSkillTreeConnectionLayer)
		.OwnerSkillTreeWidget(OwnerSkillTreeWidget);
}

void UFrontierSkillTreeConnectionLayerWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyConnectionLayer.Reset();
}
