#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FrontierMinimapDataAsset.generated.h"

class UTexture2D;

/**
 * One pre-baked, north-up minimap floor.
 *
 * The texture must be square. Its top edge represents world +X and its right
 * edge represents world +Y. WorldCenter and WorldWidth describe the square
 * world area baked into the complete texture.
 */
USTRUCT(BlueprintType)
struct FFrontierMinimapFloorDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Minimap")
	FName FloorId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Minimap")
	TSoftObjectPtr<UTexture2D> FloorTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Minimap")
	FVector2D WorldCenter = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Minimap", meta=(ClampMin="1.0"))
	float WorldWidth = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Minimap|Height")
	float MinWorldZ = -100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Minimap|Height")
	float MaxWorldZ = 100.0f;

	bool ContainsHeight(float WorldZ) const;
	bool IsValid() const;
};

/** Runtime minimap configuration containing all pre-baked floors for one map. */
UCLASS(BlueprintType)
class FRONTIER_API UFrontierMinimapDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	int32 FindFloorIndex(float WorldZ) const;
	const FFrontierMinimapFloorDefinition* GetFloor(int32 FloorIndex) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Minimap")
	TArray<FFrontierMinimapFloorDefinition> Floors;
};
