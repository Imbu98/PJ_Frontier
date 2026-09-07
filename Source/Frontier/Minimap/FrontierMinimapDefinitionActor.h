#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "FrontierMinimapDefinitionActor.generated.h"

class UFrontierMinimapDataAsset;

/**
 * Authoring input for one floor of the automatic editor bake.
 *
 * Player Z is the capsule-center height used by the runtime floor selector.
 * By default the baker converts that range to a world-space mesh slice using
 * the actor's PawnCenterAboveNavSurface setting. Override the mesh range when
 * a ceiling, basement, bridge, or stacked room needs a tighter bake slice.
 */
USTRUCT(BlueprintType)
struct FFrontierMinimapBakeFloorDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Frontier|Minimap|Bake")
	FName FloorId = TEXT("Floor_0");

	UPROPERTY(EditAnywhere, Category="Frontier|Minimap|Bake|Player Height")
	float MinPlayerWorldZ = -100.0f;

	UPROPERTY(EditAnywhere, Category="Frontier|Minimap|Bake|Player Height")
	float MaxPlayerWorldZ = 100.0f;

	UPROPERTY(
		EditAnywhere,
		Category="Frontier|Minimap|Bake|Mesh Height",
		meta=(DisplayName="Override Mesh Bake Z Range"))
	bool bOverrideNavSurfaceRange = false;

	UPROPERTY(
		EditAnywhere,
		Category="Frontier|Minimap|Bake|Mesh Height",
		meta=(
			EditCondition="bOverrideNavSurfaceRange",
			DisplayName="Min Mesh Bake World Z"))
	float MinNavSurfaceWorldZ = -196.0f;

	UPROPERTY(
		EditAnywhere,
		Category="Frontier|Minimap|Bake|Mesh Height",
		meta=(
			EditCondition="bOverrideNavSurfaceRange",
			DisplayName="Max Mesh Bake World Z"))
	float MaxNavSurfaceWorldZ = 4.0f;
};

/**
 * Place exactly one instance in a raid's persistent level.
 * The soft reference keeps minimap textures out of dedicated-server memory.
 */
UCLASS(Blueprintable)
class FRONTIER_API AFrontierMinimapDefinitionActor : public AInfo
{
	GENERATED_BODY()

public:
	AFrontierMinimapDefinitionActor();

	/** Generates or updates every configured floor from loaded static meshes. */
	UFUNCTION(
		CallInEditor,
		Category="Frontier|Minimap|Bake",
		meta=(DisplayName="Bake All Floors"))
	void BakeAllFloors();

	const TSoftObjectPtr<UFrontierMinimapDataAsset>& GetMinimapDataAsset() const
	{
		return MinimapDataAsset;
	}

#if WITH_EDITOR
	const TArray<FFrontierMinimapBakeFloorDefinition>& GetBakeFloors() const
	{
		return BakeFloors;
	}

	const FDirectoryPath& GetBakeOutputDirectory() const
	{
		return BakeOutputDirectory;
	}

	int32 GetBakeTextureResolution() const
	{
		return BakeTextureResolution;
	}

	float GetBakeWorldPadding() const
	{
		return BakeWorldPadding;
	}

	float GetPawnCenterAboveNavSurface() const
	{
		return PawnCenterAboveNavSurface;
	}

	float GetBakeFloorTransitionOverlap() const
	{
		return BakeFloorTransitionOverlap;
	}

	FName GetBakeIgnoreTag() const
	{
		return BakeIgnoreTag;
	}

	int32 GetBakeOutlineThicknessPixels() const
	{
		return BakeOutlineThicknessPixels;
	}

	const FLinearColor& GetBakeFillColor() const
	{
		return BakeFillColor;
	}

	const FLinearColor& GetBakeOutlineColor() const
	{
		return BakeOutlineColor;
	}

	void SetMinimapDataAsset(UFrontierMinimapDataAsset* InMinimapDataAsset);
#endif

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Minimap")
	TSoftObjectPtr<UFrontierMinimapDataAsset> MinimapDataAsset;

#if WITH_EDITORONLY_DATA
	/** One row per runtime floor. Keep player height ranges non-overlapping. */
	UPROPERTY(EditAnywhere, Category="Frontier|Minimap|Bake")
	TArray<FFrontierMinimapBakeFloorDefinition> BakeFloors;

	/** Content Browser directory for generated textures and the data asset. */
	UPROPERTY(
		EditAnywhere,
		Category="Frontier|Minimap|Bake|Output",
		meta=(ContentDir))
	FDirectoryPath BakeOutputDirectory;

	/** 1024 is normally enough for a 240 px minimap and keeps memory low. */
	UPROPERTY(
		EditAnywhere,
		Category="Frontier|Minimap|Bake|Output",
		meta=(ClampMin="256", ClampMax="4096", UIMin="256", UIMax="4096"))
	int32 BakeTextureResolution = 1024;

	/**
	 * Transparent border around each floor's complete baked geometry bounds.
	 * Keep this at least half of the runtime VisibleWorldWidth.
	 */
	UPROPERTY(
		EditAnywhere,
		Category="Frontier|Minimap|Bake|Output",
		meta=(ClampMin="0.0"))
	float BakeWorldPadding = 2500.0f;

	/** Current Frontier player capsule center is 96 uu above the floor surface. */
	UPROPERTY(
		EditAnywhere,
		Category="Frontier|Minimap|Bake|Height",
		meta=(
			ClampMin="0.0",
			DisplayName="Pawn Center Above Floor Surface"))
	float PawnCenterAboveNavSurface = 96.0f;

	/**
	 * Extra mesh Z included on both sides of a floor boundary.
	 * Keep this equal to the runtime minimap component's FloorHysteresis.
	 */
	UPROPERTY(
		EditAnywhere,
		Category="Frontier|Minimap|Bake|Height",
		meta=(ClampMin="0.0"))
	float BakeFloorTransitionOverlap = 50.0f;

	/**
	 * Add this tag to an Actor to exclude all of its static meshes, or to a
	 * component's Component Tags to exclude only that component.
	 */
	UPROPERTY(
		VisibleAnywhere,
		Category="Frontier|Minimap|Bake|Filtering",
		meta=(DisplayName="Ignore Actor / Component Tag"))
	FName BakeIgnoreTag = TEXT("MinimapIgnore");

	UPROPERTY(
		EditAnywhere,
		Category="Frontier|Minimap|Bake|Style",
		meta=(ClampMin="0", ClampMax="16"))
	int32 BakeOutlineThicknessPixels = 2;

	UPROPERTY(EditAnywhere, Category="Frontier|Minimap|Bake|Style")
	FLinearColor BakeFillColor = FLinearColor(0.12f, 0.15f, 0.18f, 0.9f);

	UPROPERTY(EditAnywhere, Category="Frontier|Minimap|Bake|Style")
	FLinearColor BakeOutlineColor = FLinearColor(0.8f, 0.85f, 0.9f, 1.0f);
#endif
};
