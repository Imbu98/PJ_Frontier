#include "Minimap/FrontierMinimapDataAsset.h"

bool FFrontierMinimapFloorDefinition::ContainsHeight(const float WorldZ) const
{
	const float LowerHeight = FMath::Min(MinWorldZ, MaxWorldZ);
	const float UpperHeight = FMath::Max(MinWorldZ, MaxWorldZ);
	return WorldZ >= LowerHeight && WorldZ <= UpperHeight;
}

bool FFrontierMinimapFloorDefinition::IsValid() const
{
	return !FloorTexture.IsNull() && WorldWidth > UE_SMALL_NUMBER;
}

int32 UFrontierMinimapDataAsset::FindFloorIndex(const float WorldZ) const
{
	int32 BestFloorIndex = INDEX_NONE;
	float BestDistanceFromCenter = TNumericLimits<float>::Max();

	for (int32 FloorIndex = 0; FloorIndex < Floors.Num(); ++FloorIndex)
	{
		const FFrontierMinimapFloorDefinition& Floor = Floors[FloorIndex];
		if (!Floor.IsValid() || !Floor.ContainsHeight(WorldZ))
		{
			continue;
		}

		const float FloorCenterZ = (Floor.MinWorldZ + Floor.MaxWorldZ) * 0.5f;
		const float DistanceFromCenter = FMath::Abs(WorldZ - FloorCenterZ);
		if (DistanceFromCenter < BestDistanceFromCenter)
		{
			BestFloorIndex = FloorIndex;
			BestDistanceFromCenter = DistanceFromCenter;
		}
	}

	return BestFloorIndex;
}

const FFrontierMinimapFloorDefinition* UFrontierMinimapDataAsset::GetFloor(
	const int32 FloorIndex) const
{
	return Floors.IsValidIndex(FloorIndex) ? &Floors[FloorIndex] : nullptr;
}
