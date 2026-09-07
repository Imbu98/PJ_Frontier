#include "UI/FrontierMinimapWidget.h"

#include "Components/Image.h"
#include "Components/Widget.h"
#include "Frontier.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
const FName MapTextureParameterName(TEXT("MapTexture"));
const FName MapUVRectParameterName(TEXT("MapUVRect"));
}

void UFrontierMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	MapMaterial = MapImage ? MapImage->GetDynamicMaterial() : nullptr;
	if (!MapMaterial)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("MapImage must use a User Interface material with MapTexture and MapUVRect parameters."));
	}

	ApplyMapTextureParameter();
	ApplyMapViewParameter();
	ApplyPlayerMarkerTransform();
}

void UFrontierMinimapWidget::SetMapTexture(UTexture* InMapTexture)
{
	MapTexture = InMapTexture;
	ApplyMapTextureParameter();
}

void UFrontierMinimapWidget::SetMapView(
	const FBox2f& InUVRegion,
	const FVector2D& InNormalizedPlayerPosition,
	const float InHeadingDegrees)
{
	MapUVRegion = InUVRegion;
	NormalizedPlayerPosition = FVector2D(
		FMath::Clamp(InNormalizedPlayerPosition.X, 0.0f, 1.0f),
		FMath::Clamp(InNormalizedPlayerPosition.Y, 0.0f, 1.0f));
	PlayerHeadingDegrees = InHeadingDegrees;

	ApplyMapViewParameter();
	ApplyPlayerMarkerTransform();
}

void UFrontierMinimapWidget::SetPlayerHeading(const float InHeadingDegrees)
{
	PlayerHeadingDegrees = InHeadingDegrees;
	ApplyPlayerMarkerTransform();
}

void UFrontierMinimapWidget::SetPlayerMarkerVisible(const bool bVisible)
{
	if (PlayerMarkerRoot)
	{
		PlayerMarkerRoot->SetVisibility(
			bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UFrontierMinimapWidget::ApplyMapTextureParameter()
{
	if (!MapMaterial)
	{
		return;
	}

	MapMaterial->SetTextureParameterValue(MapTextureParameterName, MapTexture);
}

void UFrontierMinimapWidget::ApplyMapViewParameter()
{
	if (!MapMaterial)
	{
		return;
	}

	MapMaterial->SetVectorParameterValue(
		MapUVRectParameterName,
		FLinearColor(
			MapUVRegion.Min.X,
			MapUVRegion.Min.Y,
			MapUVRegion.Max.X,
			MapUVRegion.Max.Y));
}

void UFrontierMinimapWidget::ApplyPlayerMarkerTransform()
{
	if (!MapImage || !PlayerMarkerRoot)
	{
		return;
	}

	const FVector2D MapLocalSize = MapImage->GetCachedGeometry().GetLocalSize();
	const FVector2D OffsetFromCenter =
		(NormalizedPlayerPosition - FVector2D(0.5f, 0.5f)) * MapLocalSize;
	PlayerMarkerRoot->SetRenderTranslation(OffsetFromCenter);
	PlayerMarkerRoot->SetRenderTransformAngle(PlayerHeadingDegrees);
}
