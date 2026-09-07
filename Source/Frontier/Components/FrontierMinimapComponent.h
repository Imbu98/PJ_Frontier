#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FrontierMinimapComponent.generated.h"

class AFrontierPlayerController;
class APawn;
class UFrontierMinimapDataAsset;
class UFrontierMinimapWidget;
class UTexture2D;
struct FFrontierMinimapFloorDefinition;
struct FStreamableHandle;

/**
 * Local-only runtime controller for the baked minimap.
 *
 * It loads one map-specific data asset and all floor textures asynchronously,
 * then updates only UI material parameters. It never captures or scans level
 * geometry at runtime.
 */
UCLASS(ClassGroup=(Frontier), meta=(BlueprintSpawnableComponent))
class FRONTIER_API UFrontierMinimapComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFrontierMinimapComponent();

	/** Converts world XY to a north-up baked texture UV. */
	static FVector2D WorldLocationToMapUV(
		const FVector& WorldLocation,
		const FVector2D& MapWorldCenter,
		float MapWorldWidth);

	/**
	 * Calculates the clamped texture crop and marker position for a local view.
	 * The marker remains centered until the crop reaches a texture edge.
	 */
	static FBox2f CalculateVisibleUVRegion(
		const FVector2D& PlayerMapUV,
		float VisibleWorldWidth,
		float MapWorldWidth,
		FVector2D& OutMarkerNormalizedPosition);

	/** Returns whether the current floor should be retained near a Z boundary. */
	static bool IsHeightWithinFloorWithHysteresis(
		const FFrontierMinimapFloorDefinition& Floor,
		float WorldZ,
		float Hysteresis);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool ShouldDisplayMinimap() const;
	void TryInitializeMinimap();
	bool ResolveMinimapDataAsset();
	void BeginDataAssetLoad();
	void HandleDataAssetLoaded();
	void BeginFloorTexturePreload();
	void HandleFloorTexturesLoaded();
	void CancelPendingLoads();

	void EnsureMinimapWidget();
	void ReleaseMinimapWidget();
	void UpdateMinimap(const APawn* Pawn);
	int32 ResolveFloorIndex(float WorldZ) const;
	bool SetActiveFloor(
		int32 FloorIndex,
		const FFrontierMinimapFloorDefinition& Floor);
	bool IsFloorTextureReady(int32 FloorIndex) const;

	/** User-authored WBP derived from UFrontierMinimapWidget. */
	UPROPERTY(EditDefaultsOnly, Category="Frontier|Minimap")
	TSubclassOf<UFrontierMinimapWidget> MinimapWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="Frontier|Minimap", meta=(ClampMin="1.0"))
	float MinimapSize = 240.0f;

	/** Square world-space width shown around the local player. */
	UPROPERTY(EditDefaultsOnly, Category="Frontier|Minimap", meta=(ClampMin="1.0"))
	float VisibleWorldWidth = 5000.0f;

	/** UI-only update interval. 30 Hz is smooth without rendering the world. */
	UPROPERTY(EditDefaultsOnly, Category="Frontier|Minimap", meta=(ClampMin="0.016"))
	float UpdateInterval = 0.033333f;

	/**
	 * Extra Z distance for retaining the active floor near stairs and ramps.
	 * This prevents rapid floor switching at an overlapping boundary.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Frontier|Minimap", meta=(ClampMin="0.0"))
	float FloorHysteresis = 50.0f;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierPlayerController> OwningController;

	UPROPERTY(Transient)
	TSoftObjectPtr<UFrontierMinimapDataAsset> ResolvedMinimapDataAsset;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierMinimapDataAsset> LoadedMinimapData;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierMinimapWidget> MinimapWidget;

	/** Strong references keep preloaded textures resident during the raid. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> LoadedFloorTextures;

	TSharedPtr<FStreamableHandle> DataAssetLoadHandle;
	TSharedPtr<FStreamableHandle> FloorTextureLoadHandle;

	int32 ActiveFloorIndex = INDEX_NONE;
	bool bInitializationStarted = false;
	bool bMissingDefinitionWarningLogged = false;
};
