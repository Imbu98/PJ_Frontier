#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierMinimapWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UTexture;
class UWidget;

/**
 * WBP base for the local minimap.
 *
 * The WBP must provide an Image named MapImage and a centered marker widget
 * named PlayerMarkerRoot. MapImage's brush must use a User Interface material
 * with a Texture parameter named MapTexture and a Vector parameter named
 * MapUVRect. All layout and visual styling remain in Blueprint.
 */
UCLASS(Abstract, Blueprintable)
class FRONTIER_API UFrontierMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void SetMapTexture(UTexture* InMapTexture);
	void SetMapView(
		const FBox2f& InUVRegion,
		const FVector2D& InNormalizedPlayerPosition,
		float InHeadingDegrees);
	void SetPlayerHeading(float InHeadingDegrees);
	void SetPlayerMarkerVisible(bool bVisible);

private:
	void ApplyMapTextureParameter();
	void ApplyMapViewParameter();
	void ApplyPlayerMarkerTransform();

	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UImage> MapImage;

	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UWidget> PlayerMarkerRoot;

	UPROPERTY(Transient)
	TObjectPtr<UTexture> MapTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MapMaterial;

	FBox2f MapUVRegion =
		FBox2f(FVector2f(0.0f, 0.0f), FVector2f(1.0f, 1.0f));
	FVector2D NormalizedPlayerPosition = FVector2D(0.5f, 0.5f);
	float PlayerHeadingDegrees = 0.0f;
};
