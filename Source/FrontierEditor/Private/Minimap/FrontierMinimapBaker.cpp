#include "Minimap/FrontierMinimapBaker.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "MeshDescription.h"
#include "Misc/Crc.h"
#include "Misc/FeedbackContext.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "Minimap/FrontierMinimapDataAsset.h"
#include "Minimap/FrontierMinimapDefinitionActor.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "StaticMeshCompiler.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FrontierMinimapBaker"

DEFINE_LOG_CATEGORY_STATIC(LogFrontierMinimapBaker, Log, All);

namespace
{
struct FMinimapBakeTriangle
{
	FVector A = FVector::ZeroVector;
	FVector B = FVector::ZeroVector;
	FVector C = FVector::ZeroVector;
};

struct FMinimapLocalMeshGeometry
{
	TArray<FMinimapBakeTriangle> Triangles;
	FBox LocalBounds = FBox(ForceInit);
	bool bUsedBoundsFallback = false;
};

struct FMinimapMeshPlacement
{
	UStaticMesh* StaticMesh = nullptr;
	FTransform WorldTransform = FTransform::Identity;
	FString SourcePath;
};

struct FMinimapFloorBakeWork
{
	const FFrontierMinimapBakeFloorDefinition* Definition = nullptr;
	float MinMeshZ = 0.0f;
	float MaxMeshZ = 0.0f;
	FBox2D WorldBounds = FBox2D(ForceInit);
	FVector2D WorldCenter = FVector2D::ZeroVector;
	float WorldWidth = 0.0f;
	TArray<uint8> Occupancy;
	TArray<uint8> FeatureMask;
	TArray<float> TopDepth;
	uint64 TextureContentHash = 0;
	FString TextureAssetName;
	FString TexturePackageName;
	UTexture2D* Texture = nullptr;
};

struct FMinimapMeshBakeStats
{
	int32 IncludedComponents = 0;
	int32 IgnoredComponents = 0;
	int32 Placements = 0;
	int64 SourceTriangles = 0;
	int64 PlacementTriangles = 0;
	int32 BoundsFallbackMeshes = 0;
	int32 SkippedSplineComponents = 0;
};

bool bMinimapBakeInProgress = false;
constexpr uint32 MinimapBakeFormatVersion = 2;
constexpr float MinimapDepthEdgeThreshold = 10.0f;
constexpr double MinimapMaximumAutomaticMeshWidth = 1000000.0;
constexpr int64 MinimapMaximumTotalFloorPixels = 64ll * 1024ll * 1024ll;
constexpr int64 MinimapMaximumSourceTrianglesPerMesh = 1ll * 1000ll * 1000ll;
constexpr int64 MinimapMaximumUniqueSourceTriangles = 5ll * 1000ll * 1000ll;
constexpr int64 MinimapMaximumPlacementTriangles = 25ll * 1000ll * 1000ll;
const FName GeneratedMetadataKey(TEXT("FrontierMinimapGenerated"));
const FName OwnerMapMetadataKey(TEXT("FrontierMinimapOwnerMap"));

bool ReportBakeError(const FText& Error)
{
	if (!IsRunningCommandlet())
	{
		FMessageDialog::Open(EAppMsgType::Ok, Error, LOCTEXT("BakeErrorTitle", "Minimap Bake Failed"));
	}
	return false;
}

bool ShouldCancelGeometryWork(const int64 ProcessedTriangleCount)
{
	return (ProcessedTriangleCount & 0x3fff) == 0
		&& GWarn
		&& GWarn->ReceivedUserCancel();
}

void ReportBakeSuccess(const FText& Message)
{
	if (!IsRunningCommandlet())
	{
		FMessageDialog::Open(EAppMsgType::Ok, Message, LOCTEXT("BakeSuccessTitle", "Minimap Bake Complete"));
	}
}

float SignedEdge(
	const FVector2f& A,
	const FVector2f& B,
	const FVector2f& Point)
{
	return (Point.X - A.X) * (B.Y - A.Y)
		- (Point.Y - A.Y) * (B.X - A.X);
}

void RasterizeTriangle(
	const FVector2f& A,
	const FVector2f& B,
	const FVector2f& C,
	const int32 Resolution,
	TArray<uint8>& Occupancy)
{
	const float SignedArea = SignedEdge(A, B, C);
	if (FMath::IsNearlyZero(SignedArea))
	{
		return;
	}

	const float MinX = FMath::Min3(A.X, B.X, C.X);
	const float MinY = FMath::Min3(A.Y, B.Y, C.Y);
	const float MaxX = FMath::Max3(A.X, B.X, C.X);
	const float MaxY = FMath::Max3(A.Y, B.Y, C.Y);
	const int32 StartX = FMath::Clamp(FMath::FloorToInt(MinX), 0, Resolution - 1);
	const int32 StartY = FMath::Clamp(FMath::FloorToInt(MinY), 0, Resolution - 1);
	const int32 EndX = FMath::Clamp(FMath::CeilToInt(MaxX) - 1, 0, Resolution - 1);
	const int32 EndY = FMath::Clamp(FMath::CeilToInt(MaxY) - 1, 0, Resolution - 1);
	const bool bPositiveWinding = SignedArea > 0.0f;
	constexpr float EdgeTolerance = 0.001f;

	for (int32 Y = StartY; Y <= EndY; ++Y)
	{
		for (int32 X = StartX; X <= EndX; ++X)
		{
			const FVector2f PixelCenter(
				static_cast<float>(X) + 0.5f,
				static_cast<float>(Y) + 0.5f);
			const float EdgeAB = SignedEdge(A, B, PixelCenter);
			const float EdgeBC = SignedEdge(B, C, PixelCenter);
			const float EdgeCA = SignedEdge(C, A, PixelCenter);
			const bool bInside = bPositiveWinding
				? EdgeAB >= -EdgeTolerance
					&& EdgeBC >= -EdgeTolerance
					&& EdgeCA >= -EdgeTolerance
				: EdgeAB <= EdgeTolerance
					&& EdgeBC <= EdgeTolerance
					&& EdgeCA <= EdgeTolerance;
			if (bInside)
			{
				Occupancy[Y * Resolution + X] = 255;
			}
		}
	}
}

void RasterizeTriangleDepth(
	const FVector2f& A,
	const FVector2f& B,
	const FVector2f& C,
	const float DepthA,
	const float DepthB,
	const float DepthC,
	const int32 Resolution,
	TArray<uint8>& Occupancy,
	TArray<float>& TopDepth)
{
	const float SignedArea = SignedEdge(A, B, C);
	if (FMath::IsNearlyZero(SignedArea))
	{
		return;
	}

	const int32 StartX = FMath::Clamp(
		FMath::FloorToInt(FMath::Min3(A.X, B.X, C.X)),
		0,
		Resolution - 1);
	const int32 StartY = FMath::Clamp(
		FMath::FloorToInt(FMath::Min3(A.Y, B.Y, C.Y)),
		0,
		Resolution - 1);
	const int32 EndX = FMath::Clamp(
		FMath::CeilToInt(FMath::Max3(A.X, B.X, C.X)) - 1,
		0,
		Resolution - 1);
	const int32 EndY = FMath::Clamp(
		FMath::CeilToInt(FMath::Max3(A.Y, B.Y, C.Y)) - 1,
		0,
		Resolution - 1);
	const bool bPositiveWinding = SignedArea > 0.0f;
	constexpr float EdgeTolerance = 0.001f;
	const float InverseArea = 1.0f / SignedArea;

	for (int32 Y = StartY; Y <= EndY; ++Y)
	{
		for (int32 X = StartX; X <= EndX; ++X)
		{
			const FVector2f PixelCenter(
				static_cast<float>(X) + 0.5f,
				static_cast<float>(Y) + 0.5f);
			const float EdgeAB = SignedEdge(A, B, PixelCenter);
			const float EdgeBC = SignedEdge(B, C, PixelCenter);
			const float EdgeCA = SignedEdge(C, A, PixelCenter);
			const bool bInside = bPositiveWinding
				? EdgeAB >= -EdgeTolerance
					&& EdgeBC >= -EdgeTolerance
					&& EdgeCA >= -EdgeTolerance
				: EdgeAB <= EdgeTolerance
					&& EdgeBC <= EdgeTolerance
					&& EdgeCA <= EdgeTolerance;
			if (!bInside)
			{
				continue;
			}

			const float WeightA =
				SignedEdge(B, C, PixelCenter) * InverseArea;
			const float WeightB =
				SignedEdge(C, A, PixelCenter) * InverseArea;
			const float WeightC = 1.0f - WeightA - WeightB;
			const float Depth =
				WeightA * DepthA
				+ WeightB * DepthB
				+ WeightC * DepthC;
			const int32 PixelIndex = Y * Resolution + X;
			if (Occupancy[PixelIndex] == 0
				|| Depth > TopDepth[PixelIndex])
			{
				Occupancy[PixelIndex] = 255;
				TopDepth[PixelIndex] = Depth;
			}
		}
	}
}

FVector2f WorldToPixel(
	const FVector& WorldPosition,
	const FVector2D& WorldCenter,
	const float WorldWidth,
	const int32 Resolution)
{
	const float U = 0.5f
		+ static_cast<float>((WorldPosition.Y - WorldCenter.Y) / WorldWidth);
	const float V = 0.5f
		- static_cast<float>((WorldPosition.X - WorldCenter.X) / WorldWidth);
	return FVector2f(U * Resolution, V * Resolution);
}

void RasterizeFeatureLine(
	const FVector2f& A,
	const FVector2f& B,
	const float LineDepth,
	const int32 Resolution,
	TArray<uint8>& FeatureSeeds,
	TArray<float>& FeatureDepth)
{
	const FVector2f AB = B - A;
	const float LengthSquared = AB.SizeSquared();
	const float Radius = 0.75f;
	const float RadiusSquared = Radius * Radius;
	const int32 StartX = FMath::Clamp(
		FMath::FloorToInt(FMath::Min(A.X, B.X) - Radius),
		0,
		Resolution - 1);
	const int32 StartY = FMath::Clamp(
		FMath::FloorToInt(FMath::Min(A.Y, B.Y) - Radius),
		0,
		Resolution - 1);
	const int32 EndX = FMath::Clamp(
		FMath::CeilToInt(FMath::Max(A.X, B.X) + Radius),
		0,
		Resolution - 1);
	const int32 EndY = FMath::Clamp(
		FMath::CeilToInt(FMath::Max(A.Y, B.Y) + Radius),
		0,
		Resolution - 1);

	for (int32 Y = StartY; Y <= EndY; ++Y)
	{
		for (int32 X = StartX; X <= EndX; ++X)
		{
			const FVector2f Point(
				static_cast<float>(X) + 0.5f,
				static_cast<float>(Y) + 0.5f);
			const float Alpha = LengthSquared > UE_SMALL_NUMBER
				? FMath::Clamp(
					FVector2f::DotProduct(Point - A, AB) / LengthSquared,
					0.0f,
					1.0f)
				: 0.0f;
			const FVector2f Closest = A + AB * Alpha;
			if ((Point - Closest).SizeSquared() <= RadiusSquared)
			{
				const int32 PixelIndex = Y * Resolution + X;
				FeatureSeeds[PixelIndex] = 255;
				FeatureDepth[PixelIndex] = FMath::Max(
					FeatureDepth[PixelIndex],
					LineDepth);
			}
		}
	}
}

bool HasEmptyNeighbor(
	const TArray<uint8>& Occupancy,
	const int32 Resolution,
	const int32 X,
	const int32 Y)
{
	for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
	{
		for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
		{
			if (OffsetX == 0 && OffsetY == 0)
			{
				continue;
			}

			const int32 NeighborX = X + OffsetX;
			const int32 NeighborY = Y + OffsetY;
			if (NeighborX < 0
				|| NeighborY < 0
				|| NeighborX >= Resolution
				|| NeighborY >= Resolution
				|| Occupancy[NeighborY * Resolution + NeighborX] == 0)
			{
				return true;
			}
		}
	}

	return false;
}

void BuildOutlineDistances(
	const TArray<uint8>& Occupancy,
	const int32 Resolution,
	const int32 OutlineThickness,
	TArray<uint8>& OutDistance)
{
	OutDistance.Init(0, Occupancy.Num());
	if (OutlineThickness <= 0)
	{
		return;
	}

	TArray<int32> Queue;
	Queue.Reserve(Occupancy.Num() / 8);
	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
		{
			const int32 PixelIndex = Y * Resolution + X;
			if (Occupancy[PixelIndex] == 0)
			{
				continue;
			}

			if (HasEmptyNeighbor(Occupancy, Resolution, X, Y))
			{
				OutDistance[PixelIndex] = 1;
				Queue.Add(PixelIndex);
			}
		}
	}

	int32 QueueHead = 0;
	while (QueueHead < Queue.Num())
	{
		const int32 PixelIndex = Queue[QueueHead++];
		const uint8 Distance = OutDistance[PixelIndex];
		if (Distance >= OutlineThickness)
		{
			continue;
		}

		const int32 X = PixelIndex % Resolution;
		const int32 Y = PixelIndex / Resolution;
		for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
		{
			for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
			{
				if (OffsetX == 0 && OffsetY == 0)
				{
					continue;
				}

				const int32 NeighborX = X + OffsetX;
				const int32 NeighborY = Y + OffsetY;
				if (NeighborX < 0
					|| NeighborY < 0
					|| NeighborX >= Resolution
					|| NeighborY >= Resolution)
				{
					continue;
				}

				const int32 NeighborIndex =
					NeighborY * Resolution + NeighborX;
				if (Occupancy[NeighborIndex] != 0
					&& OutDistance[NeighborIndex] == 0)
				{
					OutDistance[NeighborIndex] = Distance + 1;
					Queue.Add(NeighborIndex);
				}
			}
		}
	}
}

void AddDepthEdgeSeeds(
	const TArray<uint8>& Occupancy,
	const TArray<float>& TopDepth,
	const int32 Resolution,
	TArray<uint8>& FeatureSeeds)
{
	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
		{
			const int32 PixelIndex = Y * Resolution + X;
			if (Occupancy[PixelIndex] == 0)
			{
				continue;
			}

			const int32 NeighborOffsets[2] = { 1, Resolution };
			const bool NeighborValid[2] =
			{
				X + 1 < Resolution,
				Y + 1 < Resolution
			};
			for (int32 NeighborIndex = 0;
				NeighborIndex < UE_ARRAY_COUNT(NeighborOffsets);
				++NeighborIndex)
			{
				if (!NeighborValid[NeighborIndex])
				{
					continue;
				}
				const int32 OtherIndex =
					PixelIndex + NeighborOffsets[NeighborIndex];
				if (Occupancy[OtherIndex] != 0
					&& FMath::Abs(
						TopDepth[PixelIndex]
						- TopDepth[OtherIndex])
						>= MinimapDepthEdgeThreshold)
				{
					FeatureSeeds[PixelIndex] = 255;
					FeatureSeeds[OtherIndex] = 255;
				}
			}
		}
	}
}

void DilateFeatureSeeds(
	const TArray<uint8>& FeatureSeeds,
	const int32 Resolution,
	const int32 Thickness,
	TArray<uint8>& OutFeatureMask)
{
	OutFeatureMask.Init(0, FeatureSeeds.Num());
	if (Thickness <= 0)
	{
		return;
	}

	TArray<uint8> Distance;
	Distance.Init(0, FeatureSeeds.Num());
	TArray<int32> Queue;
	Queue.Reserve(FeatureSeeds.Num() / 8);
	for (int32 PixelIndex = 0;
		PixelIndex < FeatureSeeds.Num();
		++PixelIndex)
	{
		if (FeatureSeeds[PixelIndex] != 0)
		{
			Distance[PixelIndex] = 1;
			OutFeatureMask[PixelIndex] = 255;
			Queue.Add(PixelIndex);
		}
	}

	int32 QueueHead = 0;
	while (QueueHead < Queue.Num())
	{
		const int32 PixelIndex = Queue[QueueHead++];
		const uint8 PixelDistance = Distance[PixelIndex];
		if (PixelDistance >= Thickness)
		{
			continue;
		}

		const int32 X = PixelIndex % Resolution;
		const int32 Y = PixelIndex / Resolution;
		for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
		{
			for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
			{
				if (OffsetX == 0 && OffsetY == 0)
				{
					continue;
				}
				const int32 NeighborX = X + OffsetX;
				const int32 NeighborY = Y + OffsetY;
				if (NeighborX < 0
					|| NeighborY < 0
					|| NeighborX >= Resolution
					|| NeighborY >= Resolution)
				{
					continue;
				}
				const int32 NeighborIndex =
					NeighborY * Resolution + NeighborX;
				if (Distance[NeighborIndex] == 0)
				{
					Distance[NeighborIndex] = PixelDistance + 1;
					OutFeatureMask[NeighborIndex] = 255;
					Queue.Add(NeighborIndex);
				}
			}
		}
	}
}

int32 FinalizeFloorRaster(
	FMinimapFloorBakeWork& Floor,
	const int32 Resolution,
	const int32 OutlineThickness,
	TArray<uint8>& FeatureSeeds,
	const TArray<float>& FeatureDepth)
{
	if (OutlineThickness > 0)
	{
		for (int32 PixelIndex = 0;
			PixelIndex < FeatureSeeds.Num();
			++PixelIndex)
		{
			if (FeatureSeeds[PixelIndex] != 0
				&& Floor.Occupancy[PixelIndex] != 0
				&& FeatureDepth[PixelIndex]
					+ MinimapDepthEdgeThreshold
					< Floor.TopDepth[PixelIndex])
			{
				FeatureSeeds[PixelIndex] = 0;
			}
		}
		AddDepthEdgeSeeds(
			Floor.Occupancy,
			Floor.TopDepth,
			Resolution,
			FeatureSeeds);
	}
	DilateFeatureSeeds(
		FeatureSeeds,
		Resolution,
		OutlineThickness,
		Floor.FeatureMask);
	Floor.TopDepth.Reset();

	int32 ForegroundPixelCount = 0;
	for (int32 PixelIndex = 0;
		PixelIndex < Floor.Occupancy.Num();
		++PixelIndex)
	{
		ForegroundPixelCount +=
			Floor.Occupancy[PixelIndex] != 0
				|| Floor.FeatureMask[PixelIndex] != 0
			? 1
			: 0;
	}
	return ForegroundPixelCount;
}

void BuildColoredPixels(
	const FMinimapFloorBakeWork& Floor,
	const int32 Resolution,
	const int32 OutlineThickness,
	const FColor FillColor,
	const FColor OutlineColor,
	TArray<FColor>& OutPixels)
{
	TArray<uint8> OutlineDistance;
	BuildOutlineDistances(
		Floor.Occupancy,
		Resolution,
		OutlineThickness,
		OutlineDistance);

	OutPixels.Init(FColor(0, 0, 0, 0), Floor.Occupancy.Num());
	for (int32 PixelIndex = 0;
		PixelIndex < Floor.Occupancy.Num();
		++PixelIndex)
	{
		if (Floor.Occupancy[PixelIndex] == 0
			&& Floor.FeatureMask[PixelIndex] == 0)
		{
			continue;
		}

		const bool bOutline = OutlineThickness > 0
			&& (Floor.FeatureMask[PixelIndex] != 0
				|| (OutlineDistance[PixelIndex] > 0
					&& OutlineDistance[PixelIndex] <= OutlineThickness));
		OutPixels[PixelIndex] = bOutline ? OutlineColor : FillColor;
	}
}

uint64 CalculateTextureContentHash(
	const FMinimapFloorBakeWork& Floor,
	const int32 Resolution,
	const int32 OutlineThickness,
	const FColor FillColor,
	const FColor OutlineColor)
{
	auto CalculateCrc = [&](uint32 Seed)
	{
		uint32 Hash = FCrc::MemCrc32(
			Floor.Occupancy.GetData(),
			Floor.Occupancy.Num(),
			Seed);
		Hash = FCrc::MemCrc32(
			Floor.FeatureMask.GetData(),
			Floor.FeatureMask.Num(),
			Hash);
		Hash = FCrc::MemCrc32(
			&MinimapBakeFormatVersion,
			sizeof(MinimapBakeFormatVersion),
			Hash);
		Hash = FCrc::MemCrc32(
			&Resolution,
			sizeof(Resolution),
			Hash);
		Hash = FCrc::MemCrc32(
			&OutlineThickness,
			sizeof(OutlineThickness),
			Hash);
		Hash = FCrc::MemCrc32(
			&FillColor,
			sizeof(FillColor),
			Hash);
		return FCrc::MemCrc32(
			&OutlineColor,
			sizeof(OutlineColor),
			Hash);
	};

	const uint32 High = CalculateCrc(0xA5C39E27u);
	const uint32 Low = CalculateCrc(0x4D2B7F19u);
	return (static_cast<uint64>(High) << 32)
		| static_cast<uint64>(Low);
}

bool IsFiniteTriangle(const FMinimapBakeTriangle& Triangle)
{
	return !Triangle.A.ContainsNaN()
		&& !Triangle.B.ContainsNaN()
		&& !Triangle.C.ContainsNaN();
}

float TriangleArea2D(const FMinimapBakeTriangle& Triangle)
{
	const FVector2D AB(
		Triangle.B.X - Triangle.A.X,
		Triangle.B.Y - Triangle.A.Y);
	const FVector2D AC(
		Triangle.C.X - Triangle.A.X,
		Triangle.C.Y - Triangle.A.Y);
	return FMath::Abs(AB.X * AC.Y - AB.Y * AC.X) * 0.5f;
}

using FClippedPolygon = TArray<FVector, TInlineAllocator<8>>;

void ClipPolygonAtZ(
	const FClippedPolygon& Input,
	const float PlaneZ,
	const bool bKeepAbove,
	FClippedPolygon& Output)
{
	Output.Reset();
	if (Input.IsEmpty())
	{
		return;
	}

	auto IsInside = [PlaneZ, bKeepAbove](const FVector& Vertex)
	{
		return bKeepAbove
			? Vertex.Z >= PlaneZ
			: Vertex.Z <= PlaneZ;
	};

	FVector Previous = Input.Last();
	bool bPreviousInside = IsInside(Previous);
	for (const FVector& Current : Input)
	{
		const bool bCurrentInside = IsInside(Current);
		if (bCurrentInside != bPreviousInside)
		{
			const double DeltaZ = Current.Z - Previous.Z;
			if (!FMath::IsNearlyZero(DeltaZ))
			{
				const double Alpha =
					FMath::Clamp(
						(PlaneZ - Previous.Z) / DeltaZ,
						0.0,
						1.0);
				Output.Add(FMath::Lerp(Previous, Current, Alpha));
			}
		}
		if (bCurrentInside)
		{
			Output.Add(Current);
		}

		Previous = Current;
		bPreviousInside = bCurrentInside;
	}
}

bool ClipTriangleToHeightRange(
	const FMinimapBakeTriangle& Triangle,
	const float MinZ,
	const float MaxZ,
	FClippedPolygon& OutPolygon)
{
	FClippedPolygon Input;
	Input.Add(Triangle.A);
	Input.Add(Triangle.B);
	Input.Add(Triangle.C);

	FClippedPolygon AboveMinimum;
	ClipPolygonAtZ(Input, MinZ, true, AboveMinimum);
	ClipPolygonAtZ(AboveMinimum, MaxZ, false, OutPolygon);
	return OutPolygon.Num() >= 3;
}

void AddPolygonToWorldBounds(
	const FClippedPolygon& Polygon,
	FBox2D& WorldBounds)
{
	for (const FVector& Vertex : Polygon)
	{
		WorldBounds += FVector2D(Vertex.X, Vertex.Y);
	}
}

bool FindLongestProjectedSegment(
	const FClippedPolygon& Polygon,
	FVector& OutA,
	FVector& OutB)
{
	double LongestDistanceSquared = UE_DOUBLE_SMALL_NUMBER;
	for (int32 AIndex = 0; AIndex < Polygon.Num(); ++AIndex)
	{
		for (int32 BIndex = AIndex + 1;
			BIndex < Polygon.Num();
			++BIndex)
		{
			const FVector2D Delta(
				Polygon[BIndex].X - Polygon[AIndex].X,
				Polygon[BIndex].Y - Polygon[AIndex].Y);
			const double DistanceSquared = Delta.SizeSquared();
			if (DistanceSquared > LongestDistanceSquared)
			{
				LongestDistanceSquared = DistanceSquared;
				OutA = Polygon[AIndex];
				OutB = Polygon[BIndex];
			}
		}
	}
	return LongestDistanceSquared > UE_DOUBLE_SMALL_NUMBER;
}

bool IsMostlyVertical(const FMinimapBakeTriangle& Triangle)
{
	const FVector Normal = FVector::CrossProduct(
		Triangle.B - Triangle.A,
		Triangle.C - Triangle.A);
	const double NormalLength = Normal.Length();
	return NormalLength > UE_DOUBLE_SMALL_NUMBER
		&& FMath::Abs(Normal.Z) <= NormalLength * 0.35;
}

void RasterizeClippedPolygon(
	const FClippedPolygon& Polygon,
	const FMinimapBakeTriangle& SourceTriangle,
	FMinimapFloorBakeWork& Floor,
	const int32 Resolution,
	TArray<uint8>& FeatureSeeds,
	TArray<float>& FeatureDepth)
{
	bool bRasterizedSurface = false;
	for (int32 VertexIndex = 1;
		VertexIndex + 1 < Polygon.Num();
		++VertexIndex)
	{
		const FMinimapBakeTriangle Triangle
		{
			Polygon[0],
			Polygon[VertexIndex],
			Polygon[VertexIndex + 1]
		};
		if (TriangleArea2D(Triangle) <= UE_SMALL_NUMBER)
		{
			continue;
		}

		RasterizeTriangleDepth(
			WorldToPixel(
				Triangle.A,
				Floor.WorldCenter,
				Floor.WorldWidth,
				Resolution),
			WorldToPixel(
				Triangle.B,
				Floor.WorldCenter,
				Floor.WorldWidth,
				Resolution),
			WorldToPixel(
				Triangle.C,
				Floor.WorldCenter,
				Floor.WorldWidth,
				Resolution),
			static_cast<float>(Triangle.A.Z),
			static_cast<float>(Triangle.B.Z),
			static_cast<float>(Triangle.C.Z),
			Resolution,
			Floor.Occupancy,
			Floor.TopDepth);
		bRasterizedSurface = true;
	}

	if (IsMostlyVertical(SourceTriangle) || !bRasterizedSurface)
	{
		FVector SegmentA;
		FVector SegmentB;
		if (FindLongestProjectedSegment(Polygon, SegmentA, SegmentB))
		{
			float LineDepth = -TNumericLimits<float>::Max();
			for (const FVector& Vertex : Polygon)
			{
				LineDepth = FMath::Max(
					LineDepth,
					static_cast<float>(Vertex.Z));
			}
			RasterizeFeatureLine(
				WorldToPixel(
					SegmentA,
					Floor.WorldCenter,
					Floor.WorldWidth,
					Resolution),
				WorldToPixel(
					SegmentB,
					Floor.WorldCenter,
					Floor.WorldWidth,
					Resolution),
				LineDepth,
				Resolution,
				FeatureSeeds,
				FeatureDepth);
		}
	}
}

void AddBoxFallbackGeometry(
	const FBox& Box,
	FMinimapLocalMeshGeometry& OutGeometry)
{
	const FVector Vertices[8] =
	{
		FVector(Box.Min.X, Box.Min.Y, Box.Min.Z),
		FVector(Box.Max.X, Box.Min.Y, Box.Min.Z),
		FVector(Box.Max.X, Box.Max.Y, Box.Min.Z),
		FVector(Box.Min.X, Box.Max.Y, Box.Min.Z),
		FVector(Box.Min.X, Box.Min.Y, Box.Max.Z),
		FVector(Box.Max.X, Box.Min.Y, Box.Max.Z),
		FVector(Box.Max.X, Box.Max.Y, Box.Max.Z),
		FVector(Box.Min.X, Box.Max.Y, Box.Max.Z)
	};
	const int32 Indices[36] =
	{
		0, 2, 1, 0, 3, 2,
		4, 5, 6, 4, 6, 7,
		0, 1, 5, 0, 5, 4,
		1, 2, 6, 1, 6, 5,
		2, 3, 7, 2, 7, 6,
		3, 0, 4, 3, 4, 7
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Indices); Index += 3)
	{
		OutGeometry.Triangles.Add(
			FMinimapBakeTriangle
			{
				Vertices[Indices[Index]],
				Vertices[Indices[Index + 1]],
				Vertices[Indices[Index + 2]]
			});
	}
	OutGeometry.LocalBounds = Box;
	OutGeometry.bUsedBoundsFallback = true;
}

bool BuildLocalMeshGeometry(
	UStaticMesh& StaticMesh,
	FMinimapLocalMeshGeometry& OutGeometry,
	bool* bOutCancelled = nullptr)
{
	if (bOutCancelled)
	{
		*bOutCancelled = false;
	}
	const FMeshDescription* MeshDescription =
		StaticMesh.GetMeshDescription(0);
	if (MeshDescription && MeshDescription->Triangles().Num() > 0)
	{
		const TVertexAttributesConstRef<FVector3f> VertexPositions =
			MeshDescription->GetVertexPositions();
		const FVector BuildScale =
			StaticMesh.GetNumSourceModels() > 0
				? StaticMesh.GetSourceModel(0).BuildSettings.BuildScale3D
				: FVector::OneVector;
		OutGeometry.Triangles.Reserve(
			MeshDescription->Triangles().Num());
		int64 ProcessedSourceTriangleCount = 0;
		for (const FTriangleID TriangleId
			: MeshDescription->Triangles().GetElementIDs())
		{
			if (ShouldCancelGeometryWork(ProcessedSourceTriangleCount++))
			{
				if (bOutCancelled)
				{
					*bOutCancelled = true;
				}
				return false;
			}
			const TArrayView<const FVertexID> VertexIds =
				MeshDescription->GetTriangleVertices(TriangleId);
			if (VertexIds.Num() != 3)
			{
				continue;
			}

			auto GetScaledPosition =
				[&VertexPositions, &BuildScale](const FVertexID VertexId)
			{
				const FVector Position(VertexPositions[VertexId]);
				return FVector(
					Position.X * BuildScale.X,
					Position.Y * BuildScale.Y,
					Position.Z * BuildScale.Z);
			};
			const FMinimapBakeTriangle Triangle
			{
				GetScaledPosition(VertexIds[0]),
				GetScaledPosition(VertexIds[1]),
				GetScaledPosition(VertexIds[2])
			};
			if (!IsFiniteTriangle(Triangle)
				|| FVector::CrossProduct(
					Triangle.B - Triangle.A,
					Triangle.C - Triangle.A).SizeSquared()
					<= UE_DOUBLE_SMALL_NUMBER)
			{
				continue;
			}
			OutGeometry.Triangles.Add(Triangle);
			OutGeometry.LocalBounds += Triangle.A;
			OutGeometry.LocalBounds += Triangle.B;
			OutGeometry.LocalBounds += Triangle.C;
		}
	}

	if (OutGeometry.Triangles.IsEmpty())
	{
		const FBox FallbackBounds = StaticMesh.GetBoundingBox();
		if (!FallbackBounds.IsValid)
		{
			return false;
		}
		AddBoxFallbackGeometry(FallbackBounds, OutGeometry);
	}
	return !OutGeometry.Triangles.IsEmpty();
}

bool HasMinimapIgnoreTag(
	const UStaticMeshComponent& Component,
	const FName IgnoreTag)
{
	const AActor* Owner = Component.GetOwner();
	return (!IgnoreTag.IsNone() && Component.ComponentHasTag(IgnoreTag))
		|| (Owner && !IgnoreTag.IsNone() && Owner->ActorHasTag(IgnoreTag));
}

bool CollectMeshComponents(
	UWorld& World,
	const FName IgnoreTag,
	TArray<UStaticMeshComponent*>& OutComponents,
	FMinimapMeshBakeStats& OutStats)
{
	FString FirstUnsupportedSplinePath;
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor
			|| Actor->IsTemplate()
			|| Actor->IsEditorOnly()
			|| Actor->IsHidden())
		{
			continue;
		}

		TInlineComponentArray<UStaticMeshComponent*> Components;
		Actor->GetComponents(Components);
		const bool bActorIgnored =
			!IgnoreTag.IsNone() && Actor->ActorHasTag(IgnoreTag);
		for (UStaticMeshComponent* Component : Components)
		{
			if (!Component)
			{
				continue;
			}
			if (bActorIgnored || Component->ComponentHasTag(IgnoreTag))
			{
				++OutStats.IgnoredComponents;
				continue;
			}
			if (!Component->IsRegistered()
				|| Component->IsEditorOnly()
				|| Component->IsVisualizationComponent()
				|| !Component->IsVisible()
				|| Component->bHiddenInGame
				|| !Component->GetStaticMesh())
			{
				continue;
			}
			if (Component->IsA<USplineMeshComponent>())
			{
				++OutStats.SkippedSplineComponents;
				if (FirstUnsupportedSplinePath.IsEmpty())
				{
					FirstUnsupportedSplinePath = Component->GetPathName();
				}
				continue;
			}

			OutComponents.Add(Component);
			++OutStats.IncludedComponents;
		}
	}

	if (OutStats.SkippedSplineComponents > 0)
	{
		return ReportBakeError(
			FText::Format(
				LOCTEXT(
					"SplineMeshUnsupported",
					"Found {0} Spline Mesh component(s). Their deformed geometry cannot be baked safely. Add the '{1}' Component Tag to exclude them. First: {2}"),
				OutStats.SkippedSplineComponents,
				FText::FromName(IgnoreTag),
				FText::FromString(FirstUnsupportedSplinePath)));
	}
	if (OutComponents.IsEmpty())
	{
		return ReportBakeError(
			LOCTEXT(
				"NoStaticMeshComponents",
				"No eligible Static Mesh components were found in the loaded map."));
	}
	return true;
}

bool DoesBoundsOverlapFloor(
	const FBox& WorldBounds,
	const FMinimapFloorBakeWork& Floor,
	const float HeightOverlap)
{
	return WorldBounds.Max.Z >= Floor.MinMeshZ - HeightOverlap
		&& WorldBounds.Min.Z <= Floor.MaxMeshZ + HeightOverlap;
}

bool BuildMeshPlacements(
	const TArray<UStaticMeshComponent*>& Components,
	const TArray<FMinimapFloorBakeWork>& Floors,
	const float HeightOverlap,
	const FName IgnoreTag,
	TArray<FMinimapMeshPlacement>& OutPlacements,
	FMinimapMeshBakeStats& OutStats)
{
	for (UStaticMeshComponent* Component : Components)
	{
		if (!Component || HasMinimapIgnoreTag(*Component, IgnoreTag))
		{
			continue;
		}
		UStaticMesh* StaticMesh = Component->GetStaticMesh();
		if (!StaticMesh)
		{
			continue;
		}

		auto AddPlacement =
			[&](const FTransform& WorldTransform, const FString& SourcePath)
		{
				const FBox WorldBounds =
					StaticMesh->GetBoundingBox().TransformBy(WorldTransform);
				bool bOverlapsFloor = false;
				for (const FMinimapFloorBakeWork& Floor : Floors)
				{
					bOverlapsFloor |= DoesBoundsOverlapFloor(
						WorldBounds,
						Floor,
						HeightOverlap);
				}
				if (!bOverlapsFloor)
				{
					return true;
				}

				const FVector BoundsSize = WorldBounds.GetSize();
				if (FMath::Max(BoundsSize.X, BoundsSize.Y)
					> MinimapMaximumAutomaticMeshWidth)
				{
					return ReportBakeError(
						FText::Format(
							LOCTEXT(
								"OversizedMesh",
								"'{0}' is over 10 km wide and would collapse the minimap scale. Add the '{1}' tag to its Actor or Component, then bake again."),
							FText::FromString(SourcePath),
							FText::FromName(IgnoreTag)));
				}

				FMinimapMeshPlacement& Placement =
					OutPlacements.Emplace_GetRef();
				Placement.StaticMesh = StaticMesh;
				Placement.WorldTransform = WorldTransform;
				Placement.SourcePath = SourcePath;
				++OutStats.Placements;
				return true;
			};

		if (const UInstancedStaticMeshComponent* InstancedComponent =
			Cast<UInstancedStaticMeshComponent>(Component))
		{
			for (int32 InstanceIndex = 0;
				InstanceIndex < InstancedComponent->GetInstanceCount();
				++InstanceIndex)
			{
				FTransform InstanceWorldTransform;
				if (InstancedComponent->GetInstanceTransform(
						InstanceIndex,
						InstanceWorldTransform,
						true)
					&& !AddPlacement(
						InstanceWorldTransform,
						FString::Printf(
							TEXT("%s instance %d"),
							*Component->GetPathName(),
							InstanceIndex)))
				{
					return false;
				}
			}
		}
		else if (!AddPlacement(
			Component->GetComponentTransform(),
			Component->GetPathName()))
		{
			return false;
		}
	}

	if (OutPlacements.IsEmpty())
	{
		return ReportBakeError(
			LOCTEXT(
				"NoMeshPlacements",
				"No Static Mesh placements overlap the configured floor height ranges."));
	}
	return true;
}

bool BuildGeometryCache(
	const TArray<FMinimapMeshPlacement>& Placements,
	TMap<UStaticMesh*, FMinimapLocalMeshGeometry>& OutCache,
	FMinimapMeshBakeStats& OutStats)
{
	TArray<UStaticMesh*> UniqueMeshes;
	for (const FMinimapMeshPlacement& Placement : Placements)
	{
		UniqueMeshes.AddUnique(Placement.StaticMesh);
	}
	FStaticMeshCompilingManager::Get().FinishCompilation(UniqueMeshes);
	int64 DeclaredSourceTriangleCount = 0;
	for (UStaticMesh* StaticMesh : UniqueMeshes)
	{
		if (!StaticMesh)
		{
			continue;
		}
		const FMeshDescription* MeshDescription =
			StaticMesh->GetMeshDescription(0);
		const int64 SourceTriangleCount = MeshDescription
			? MeshDescription->Triangles().Num()
			: 0;
		if (SourceTriangleCount > MinimapMaximumSourceTrianglesPerMesh)
		{
			return ReportBakeError(
				FText::Format(
					LOCTEXT(
						"MeshSourceTriangleBudgetExceeded",
						"Static Mesh '{0}' contains more than the safe 1 million source-triangle cache budget. Add 'MinimapIgnore' to the high-detail mesh or use a simpler minimap proxy."),
					FText::FromString(StaticMesh->GetPathName())));
		}
		DeclaredSourceTriangleCount += SourceTriangleCount;
		if (DeclaredSourceTriangleCount > MinimapMaximumUniqueSourceTriangles)
		{
			return ReportBakeError(
				FText::Format(
					LOCTEXT(
						"UniqueMeshSourceTriangleBudgetExceeded",
						"The loaded unique Static Mesh sources exceed the safe 5 million source-triangle cache budget near '{0}'. Add 'MinimapIgnore' to high-detail decorative meshes."),
					FText::FromString(StaticMesh->GetPathName())));
		}
	}

	for (UStaticMesh* StaticMesh : UniqueMeshes)
	{
		if (!StaticMesh)
		{
			continue;
		}
		FMinimapLocalMeshGeometry& Geometry =
			OutCache.FindOrAdd(StaticMesh);
		bool bCancelled = false;
		if (!BuildLocalMeshGeometry(*StaticMesh, Geometry, &bCancelled))
		{
			if (bCancelled)
			{
				return ReportBakeError(
					LOCTEXT(
						"MeshGeometryBuildCancelled",
						"Minimap mesh geometry collection was cancelled."));
			}
			return ReportBakeError(
				FText::Format(
					LOCTEXT(
						"MeshSourceUnavailable",
						"Could not read minimap geometry from Static Mesh '{0}'."),
					FText::FromString(StaticMesh->GetPathName())));
		}
		OutStats.SourceTriangles += Geometry.Triangles.Num();
		if (Geometry.bUsedBoundsFallback)
		{
			++OutStats.BoundsFallbackMeshes;
		}
	}
	for (const FMinimapMeshPlacement& Placement : Placements)
	{
		const FMinimapLocalMeshGeometry* Geometry =
			OutCache.Find(Placement.StaticMesh);
		if (!Geometry)
		{
			continue;
		}
		OutStats.PlacementTriangles += Geometry->Triangles.Num();
		if (OutStats.PlacementTriangles > MinimapMaximumPlacementTriangles)
		{
			return ReportBakeError(
				FText::Format(
					LOCTEXT(
						"MeshTriangleBudgetExceeded",
						"The loaded mesh placements exceed the safe 25 million triangle bake budget near '{0}'. Add 'MinimapIgnore' to high-detail or repeated decorative meshes."),
					FText::FromString(Placement.SourcePath)));
		}
	}
	return true;
}

bool BuildFloorGeometryBounds(
	const TArray<FMinimapMeshPlacement>& Placements,
	const TMap<UStaticMesh*, FMinimapLocalMeshGeometry>& GeometryCache,
	TArray<FMinimapFloorBakeWork>& Floors,
	const float WorldPadding,
	const float HeightOverlap)
{
	TArray<int64> ClippedPolygonCounts;
	ClippedPolygonCounts.Init(0, Floors.Num());
	int64 ProcessedTriangleCount = 0;
	for (const FMinimapMeshPlacement& Placement : Placements)
	{
		const FMinimapLocalMeshGeometry* Geometry =
			GeometryCache.Find(Placement.StaticMesh);
		if (!Geometry)
		{
			continue;
		}
		for (const FMinimapBakeTriangle& LocalTriangle : Geometry->Triangles)
		{
			if (ShouldCancelGeometryWork(++ProcessedTriangleCount))
			{
				return false;
			}
			const FMinimapBakeTriangle WorldTriangle
			{
				Placement.WorldTransform.TransformPosition(LocalTriangle.A),
				Placement.WorldTransform.TransformPosition(LocalTriangle.B),
				Placement.WorldTransform.TransformPosition(LocalTriangle.C)
			};
			if (!IsFiniteTriangle(WorldTriangle))
			{
				continue;
			}

			const double TriangleMinZ = FMath::Min3(
				WorldTriangle.A.Z,
				WorldTriangle.B.Z,
				WorldTriangle.C.Z);
			const double TriangleMaxZ = FMath::Max3(
				WorldTriangle.A.Z,
				WorldTriangle.B.Z,
				WorldTriangle.C.Z);
			for (int32 FloorIndex = 0;
				FloorIndex < Floors.Num();
				++FloorIndex)
			{
				FMinimapFloorBakeWork& Floor = Floors[FloorIndex];
				const float MinZ = Floor.MinMeshZ - HeightOverlap;
				const float MaxZ = Floor.MaxMeshZ + HeightOverlap;
				if (TriangleMaxZ < MinZ || TriangleMinZ > MaxZ)
				{
					continue;
				}

				FClippedPolygon Polygon;
				if (ClipTriangleToHeightRange(
						WorldTriangle,
						MinZ,
						MaxZ,
						Polygon))
				{
					AddPolygonToWorldBounds(Polygon, Floor.WorldBounds);
					++ClippedPolygonCounts[FloorIndex];
				}
			}
		}
	}

	for (int32 FloorIndex = 0;
		FloorIndex < Floors.Num();
		++FloorIndex)
	{
		FMinimapFloorBakeWork& Floor = Floors[FloorIndex];
		if (ClippedPolygonCounts[FloorIndex] == 0
			|| !Floor.WorldBounds.bIsValid)
		{
			return ReportBakeError(
				FText::Format(
					LOCTEXT(
						"FloorHasNoMeshGeometry",
						"Floor '{0}' matched no Static Mesh geometry. Check its mesh bake Z range."),
					FText::FromName(Floor.Definition->FloorId)));
		}
		Floor.WorldCenter = Floor.WorldBounds.GetCenter();
		const FVector2D BoundsSize = Floor.WorldBounds.GetSize();
		const double RawWorldWidth = FMath::Max(BoundsSize.X, BoundsSize.Y);
		if (!FMath::IsFinite(RawWorldWidth)
			|| Floor.WorldCenter.ContainsNaN()
			|| RawWorldWidth > MinimapMaximumAutomaticMeshWidth)
		{
			return ReportBakeError(
				FText::Format(
					LOCTEXT(
						"FloorMeshBoundsTooLarge",
						"Floor '{0}' spans more than 10 km. A distant or global mesh is probably included; add the 'MinimapIgnore' tag to that Actor or Component."),
					FText::FromName(Floor.Definition->FloorId)));
		}
		Floor.WorldWidth =
			RawWorldWidth
			+ WorldPadding * 2.0f;
		if (!FMath::IsFinite(Floor.WorldWidth)
			|| Floor.WorldWidth <= UE_SMALL_NUMBER
			|| Floor.WorldWidth > MinimapMaximumAutomaticMeshWidth)
		{
			return ReportBakeError(
				FText::Format(
					LOCTEXT(
						"FloorMeshBoundsEmpty",
						"Floor '{0}' produced invalid or over-10-km bounds after World Padding. Reduce padding or exclude distant meshes."),
					FText::FromName(Floor.Definition->FloorId)));
		}
	}
	return true;
}

bool RasterizeFloorMeshes(
	const TArray<FMinimapMeshPlacement>& Placements,
	const TMap<UStaticMesh*, FMinimapLocalMeshGeometry>& GeometryCache,
	TArray<FMinimapFloorBakeWork>& Floors,
	const float HeightOverlap,
	const int32 Resolution,
	const int32 OutlineThickness)
{
	for (int32 FloorIndex = 0;
		FloorIndex < Floors.Num();
		++FloorIndex)
	{
		FMinimapFloorBakeWork& Floor = Floors[FloorIndex];
		int64 ProcessedTriangleCount = 0;
		Floor.Occupancy.Init(0, Resolution * Resolution);
		Floor.TopDepth.Init(
			-TNumericLimits<float>::Max(),
			Resolution * Resolution);
		TArray<uint8> FeatureSeeds;
		FeatureSeeds.Init(0, Resolution * Resolution);
		TArray<float> FeatureDepth;
		FeatureDepth.Init(
			-TNumericLimits<float>::Max(),
			Resolution * Resolution);
		const float MinZ = Floor.MinMeshZ - HeightOverlap;
		const float MaxZ = Floor.MaxMeshZ + HeightOverlap;

		for (const FMinimapMeshPlacement& Placement : Placements)
		{
			const FMinimapLocalMeshGeometry* Geometry =
				GeometryCache.Find(Placement.StaticMesh);
			if (!Geometry)
			{
				continue;
			}
			for (const FMinimapBakeTriangle& LocalTriangle
				: Geometry->Triangles)
			{
				if (ShouldCancelGeometryWork(++ProcessedTriangleCount))
				{
					return false;
				}
				const FMinimapBakeTriangle WorldTriangle
				{
					Placement.WorldTransform.TransformPosition(LocalTriangle.A),
					Placement.WorldTransform.TransformPosition(LocalTriangle.B),
					Placement.WorldTransform.TransformPosition(LocalTriangle.C)
				};
				if (!IsFiniteTriangle(WorldTriangle))
				{
					continue;
				}
				const double TriangleMinZ = FMath::Min3(
					WorldTriangle.A.Z,
					WorldTriangle.B.Z,
					WorldTriangle.C.Z);
				const double TriangleMaxZ = FMath::Max3(
					WorldTriangle.A.Z,
					WorldTriangle.B.Z,
					WorldTriangle.C.Z);
				if (TriangleMaxZ < MinZ || TriangleMinZ > MaxZ)
				{
					continue;
				}
				FClippedPolygon Polygon;
				if (ClipTriangleToHeightRange(
						WorldTriangle,
						MinZ,
						MaxZ,
						Polygon))
				{
					RasterizeClippedPolygon(
						Polygon,
						WorldTriangle,
						Floor,
						Resolution,
						FeatureSeeds,
						FeatureDepth);
				}
			}
		}
		if (FinalizeFloorRaster(
				Floor,
				Resolution,
				OutlineThickness,
				FeatureSeeds,
				FeatureDepth) == 0)
		{
			return ReportBakeError(
				FText::Format(
					LOCTEXT(
						"FloorRasterEmpty",
						"Floor '{0}' produced an empty texture. Existing assets were not changed."),
					FText::FromName(Floor.Definition->FloorId)));
		}
	}
	return true;
}

bool ValidateWorldAndActor(
	AFrontierMinimapDefinitionActor& DefinitionActor,
	UWorld*& OutWorld)
{
	OutWorld = DefinitionActor.GetWorld();
	if (!OutWorld || OutWorld->WorldType != EWorldType::Editor)
	{
		return ReportBakeError(
			LOCTEXT("EditorWorldRequired", "Bake All Floors can only run in an editor level."));
	}

	if (GEditor && GEditor->PlayWorld)
	{
		return ReportBakeError(
			LOCTEXT("PIENotAllowed", "Stop Play or Simulate before baking the minimap."));
	}

	if (DefinitionActor.GetLevel() != OutWorld->PersistentLevel)
	{
		return ReportBakeError(
			LOCTEXT(
				"PersistentLevelRequired",
				"Place FrontierMinimapDefinitionActor in the map's persistent level."));
	}

	int32 DefinitionActorCount = 0;
	for (TActorIterator<AFrontierMinimapDefinitionActor> It(OutWorld); It; ++It)
	{
		++DefinitionActorCount;
	}
	if (DefinitionActorCount != 1)
	{
		return ReportBakeError(
			FText::Format(
				LOCTEXT(
					"DefinitionActorCount",
					"The map must contain exactly one FrontierMinimapDefinitionActor. Found {0}."),
				DefinitionActorCount));
	}

	const FString WorldPackageName = OutWorld->GetOutermost()->GetName();
	if (!WorldPackageName.StartsWith(TEXT("/Game/")))
	{
		return ReportBakeError(
			LOCTEXT("SavedMapRequired", "Save the map inside /Game before baking the minimap."));
	}

	return true;
}

bool ValidateSettings(
	const AFrontierMinimapDefinitionActor& DefinitionActor,
	TArray<FMinimapFloorBakeWork>& OutFloors,
	FString& OutOutputDirectory)
{
	const int32 Resolution = DefinitionActor.GetBakeTextureResolution();
	if (Resolution < 256
		|| Resolution > 4096
		|| !FMath::IsPowerOfTwo(Resolution))
	{
		return ReportBakeError(
			LOCTEXT(
				"InvalidResolution",
				"Texture Resolution must be a power of two from 256 through 4096."));
	}

	if (DefinitionActor.GetBakeWorldPadding() < 0.0f)
	{
		return ReportBakeError(
			LOCTEXT("InvalidPadding", "World Padding cannot be negative."));
	}
	if (DefinitionActor.GetBakeFloorTransitionOverlap() < 0.0f)
	{
		return ReportBakeError(
			LOCTEXT(
				"InvalidTransitionOverlap",
				"Floor Transition Overlap cannot be negative."));
	}

	OutOutputDirectory = DefinitionActor.GetBakeOutputDirectory().Path;
	OutOutputDirectory.RemoveFromEnd(TEXT("/"));
	FText PackageNameReason;
	if (!OutOutputDirectory.StartsWith(TEXT("/Game/"))
		|| !FPackageName::IsValidLongPackageName(
			OutOutputDirectory,
			false,
			&PackageNameReason))
	{
		return ReportBakeError(
			FText::Format(
				LOCTEXT(
					"InvalidOutputDirectory",
					"Output Directory must be a valid Content Browser path below /Game. {0}"),
				PackageNameReason));
	}

	const TArray<FFrontierMinimapBakeFloorDefinition>& BakeFloors =
		DefinitionActor.GetBakeFloors();
	if (BakeFloors.IsEmpty())
	{
		return ReportBakeError(
			LOCTEXT("NoFloors", "Add at least one entry to Bake Floors."));
	}
	const int64 TotalFloorPixels =
		static_cast<int64>(Resolution)
		* static_cast<int64>(Resolution)
		* static_cast<int64>(BakeFloors.Num());
	if (TotalFloorPixels > MinimapMaximumTotalFloorPixels)
	{
		return ReportBakeError(
			LOCTEXT(
				"MinimapBakePixelBudgetExceeded",
				"The combined floor texture resolution is too large for a safe editor bake. Reduce Texture Resolution or the number of floors."));
	}

	TSet<FName> UsedFloorIds;
	TSet<FString> UsedTextureSuffixes;
	OutFloors.Reserve(BakeFloors.Num());
	for (const FFrontierMinimapBakeFloorDefinition& Floor : BakeFloors)
	{
		if (Floor.FloorId.IsNone())
		{
			return ReportBakeError(
				LOCTEXT("UnnamedFloor", "Every Bake Floors entry needs a Floor Id."));
		}
		if (UsedFloorIds.Contains(Floor.FloorId))
		{
			return ReportBakeError(
				FText::Format(
					LOCTEXT("DuplicateFloorId", "Floor Id '{0}' is duplicated."),
					FText::FromName(Floor.FloorId)));
		}
		UsedFloorIds.Add(Floor.FloorId);

		if (Floor.MinPlayerWorldZ >= Floor.MaxPlayerWorldZ)
		{
			return ReportBakeError(
				FText::Format(
					LOCTEXT(
						"InvalidPlayerHeight",
						"Floor '{0}' requires Player Min Z to be lower than Player Max Z."),
					FText::FromName(Floor.FloorId)));
		}

		FMinimapFloorBakeWork& Work = OutFloors.Emplace_GetRef();
		Work.Definition = &Floor;
		if (Floor.bOverrideNavSurfaceRange)
		{
			Work.MinMeshZ = Floor.MinNavSurfaceWorldZ;
			Work.MaxMeshZ = Floor.MaxNavSurfaceWorldZ;
		}
		else
		{
			Work.MinMeshZ =
				Floor.MinPlayerWorldZ
				- DefinitionActor.GetPawnCenterAboveNavSurface();
			Work.MaxMeshZ =
				Floor.MaxPlayerWorldZ
				- DefinitionActor.GetPawnCenterAboveNavSurface();
		}

		if (Work.MinMeshZ >= Work.MaxMeshZ)
		{
			return ReportBakeError(
				FText::Format(
					LOCTEXT(
						"InvalidMeshHeight",
						"Floor '{0}' requires Mesh Bake Min Z to be lower than Mesh Bake Max Z."),
					FText::FromName(Floor.FloorId)));
		}
		const FString TextureSuffix =
			ObjectTools::SanitizeObjectName(Floor.FloorId.ToString());
		if (TextureSuffix.IsEmpty()
			|| UsedTextureSuffixes.Contains(TextureSuffix))
		{
			return ReportBakeError(
				FText::Format(
					LOCTEXT(
						"InvalidTextureSuffix",
						"Floor Id '{0}' does not produce a unique valid asset name."),
					FText::FromName(Floor.FloorId)));
		}
		UsedTextureSuffixes.Add(TextureSuffix);
		Work.TextureAssetName = TextureSuffix;
	}

	return true;
}

UObject* LoadObjectAtPath(const FString& PackageName, const FString& AssetName)
{
	const FString ObjectPath = PackageName + TEXT(".") + AssetName;
	return LoadObject<UObject>(
		nullptr,
		*ObjectPath,
		nullptr,
		LOAD_NoWarn | LOAD_Quiet);
}

bool IsValidGeneratedPackageName(const FString& PackageName)
{
	FText Reason;
	if (FPackageName::IsValidLongPackageName(
			PackageName,
			false,
			&Reason))
	{
		return true;
	}

	return ReportBakeError(
		FText::Format(
			LOCTEXT(
				"InvalidGeneratedPackageName",
				"Generated package path '{0}' is invalid. {1}"),
			FText::FromString(PackageName),
			Reason));
}

bool IsOwnedGeneratedAsset(
	const UObject& Asset,
	const FString& OwnerMapPackageName)
{
	FMetaData& Metadata = Asset.GetOutermost()->GetMetaData();
	return Metadata.GetValue(&Asset, GeneratedMetadataKey) == TEXT("1")
		&& Metadata.GetValue(&Asset, OwnerMapMetadataKey)
			== OwnerMapPackageName;
}

void MarkOwnedGeneratedAsset(
	UObject& Asset,
	const FString& OwnerMapPackageName)
{
	FMetaData& Metadata = Asset.GetOutermost()->GetMetaData();
	Metadata.SetValue(&Asset, GeneratedMetadataKey, TEXT("1"));
	Metadata.SetValue(
		&Asset,
		OwnerMapMetadataKey,
		*OwnerMapPackageName);
}

bool PreflightOutputAssets(
	const FString& MapAssetSuffix,
	const FString& OwnerMapPackageName,
	const FString& OutputDirectory,
	TArray<FMinimapFloorBakeWork>& Floors,
	FString& OutDataAssetName,
	FString& OutDataAssetPackageName,
	UFrontierMinimapDataAsset*& OutDataAsset,
	FString& OutMapOutputDirectory)
{
	OutMapOutputDirectory = OutputDirectory / MapAssetSuffix;
	if (!IsValidGeneratedPackageName(OutMapOutputDirectory))
	{
		return false;
	}

	for (FMinimapFloorBakeWork& Floor : Floors)
	{
		Floor.TextureAssetName = FString::Printf(
			TEXT("T_Minimap_%s_%016llX"),
			*Floor.TextureAssetName,
			static_cast<unsigned long long>(
				Floor.TextureContentHash));
		Floor.TexturePackageName =
			OutMapOutputDirectory / Floor.TextureAssetName;
		if (!IsValidGeneratedPackageName(Floor.TexturePackageName))
		{
			return false;
		}

		UObject* ExistingObject =
			LoadObjectAtPath(
				Floor.TexturePackageName,
				Floor.TextureAssetName);
		if (ExistingObject && !ExistingObject->IsA<UTexture2D>())
		{
			return ReportBakeError(
				FText::Format(
					LOCTEXT(
						"TextureClassConflict",
						"'{0}' already exists but is not a Texture2D."),
					FText::FromString(Floor.TexturePackageName)));
		}
		if (ExistingObject
			&& !IsOwnedGeneratedAsset(
				*ExistingObject,
				OwnerMapPackageName))
		{
			return ReportBakeError(
				FText::Format(
					LOCTEXT(
						"TextureOwnershipConflict",
						"Refusing to overwrite '{0}' because it was not generated for this map."),
					FText::FromString(Floor.TexturePackageName)));
		}
		Floor.Texture = Cast<UTexture2D>(ExistingObject);
	}

	OutDataAssetName = TEXT("DA_Minimap");
	OutDataAssetPackageName =
		OutMapOutputDirectory / OutDataAssetName;
	if (!IsValidGeneratedPackageName(OutDataAssetPackageName))
	{
		return false;
	}
	UObject* ExistingDataObject =
		LoadObjectAtPath(OutDataAssetPackageName, OutDataAssetName);
	if (ExistingDataObject
		&& !ExistingDataObject->IsA<UFrontierMinimapDataAsset>())
	{
		return ReportBakeError(
			FText::Format(
				LOCTEXT(
					"DataAssetClassConflict",
					"'{0}' already exists but is not a FrontierMinimapDataAsset."),
				FText::FromString(OutDataAssetPackageName)));
	}
	if (ExistingDataObject
		&& !IsOwnedGeneratedAsset(
			*ExistingDataObject,
			OwnerMapPackageName))
	{
		return ReportBakeError(
			FText::Format(
				LOCTEXT(
					"DataAssetOwnershipConflict",
					"Refusing to overwrite '{0}' because it was not generated for this map."),
				FText::FromString(OutDataAssetPackageName)));
	}
	OutDataAsset = Cast<UFrontierMinimapDataAsset>(ExistingDataObject);
	return true;
}

UTexture2D* CreateOrReuseTexture(
	FMinimapFloorBakeWork& Floor,
	const int32 Resolution,
	const int32 OutlineThickness,
	const FColor FillColor,
	const FColor OutlineColor,
	const FString& OwnerMapPackageName,
	TArray<UPackage*>& OutPackagesToSave)
{
	UTexture2D* Texture = Floor.Texture;
	const bool bIsNewAsset = Texture == nullptr;
	const bool bPackageExists =
		FPackageName::DoesPackageExist(Floor.TexturePackageName);
	const bool bNeedsWrite = bIsNewAsset
		|| !bPackageExists
		|| Texture->GetOutermost()->IsDirty();
	if (!bNeedsWrite)
	{
		Floor.Occupancy.Reset();
		Floor.FeatureMask.Reset();
		return Texture;
	}

	if (bIsNewAsset)
	{
		UPackage* Package = CreatePackage(*Floor.TexturePackageName);
		Texture = NewObject<UTexture2D>(
			Package,
			*Floor.TextureAssetName,
			RF_Public | RF_Standalone | RF_Transactional);
	}
	if (!Texture)
	{
		return nullptr;
	}

	TArray<FColor> Pixels;
	BuildColoredPixels(
		Floor,
		Resolution,
		OutlineThickness,
		FillColor,
		OutlineColor,
		Pixels);
	Texture->Modify();
	Texture->PreEditChange(nullptr);
	Texture->Source.Init(
		Resolution,
		Resolution,
		1,
		1,
		TSF_BGRA8,
		reinterpret_cast<const uint8*>(Pixels.GetData()));
	Texture->SRGB = true;
	Texture->CompressionSettings = TC_Default;
	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->LODGroup = TEXTUREGROUP_UI;
	Texture->NeverStream = true;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->Filter = TF_Bilinear;
	Texture->PostEditChange();
	MarkOwnedGeneratedAsset(*Texture, OwnerMapPackageName);
	Texture->MarkPackageDirty();

	if (bIsNewAsset)
	{
		FAssetRegistryModule::AssetCreated(Texture);
	}
	OutPackagesToSave.AddUnique(Texture->GetOutermost());
	Floor.Texture = Texture;
	Floor.Occupancy.Reset();
	Floor.FeatureMask.Reset();
	return Texture;
}

UFrontierMinimapDataAsset* CreateOrUpdateDataAsset(
	UFrontierMinimapDataAsset* DataAsset,
	const FString& DataAssetName,
	const FString& DataAssetPackageName,
	const TArray<FMinimapFloorBakeWork>& Floors,
	const FString& OwnerMapPackageName,
	bool& bOutIsNewAsset,
	TArray<UPackage*>& OutPackagesToSave)
{
	bOutIsNewAsset = DataAsset == nullptr;
	if (bOutIsNewAsset)
	{
		UPackage* Package = CreatePackage(*DataAssetPackageName);
		DataAsset = NewObject<UFrontierMinimapDataAsset>(
			Package,
			*DataAssetName,
			RF_Public | RF_Standalone | RF_Transactional);
	}
	if (!DataAsset)
	{
		return nullptr;
	}

	TArray<FFrontierMinimapFloorDefinition> RuntimeFloors;
	RuntimeFloors.Reserve(Floors.Num());
	for (const FMinimapFloorBakeWork& Floor : Floors)
	{
		FFrontierMinimapFloorDefinition& RuntimeFloor =
			RuntimeFloors.Emplace_GetRef();
		RuntimeFloor.FloorId = Floor.Definition->FloorId;
		RuntimeFloor.FloorTexture = Floor.Texture;
		RuntimeFloor.WorldCenter = Floor.WorldCenter;
		RuntimeFloor.WorldWidth = Floor.WorldWidth;
		RuntimeFloor.MinWorldZ =
			Floor.Definition->MinPlayerWorldZ;
		RuntimeFloor.MaxWorldZ =
			Floor.Definition->MaxPlayerWorldZ;
	}

	DataAsset->Modify();
	DataAsset->Floors = MoveTemp(RuntimeFloors);
	MarkOwnedGeneratedAsset(*DataAsset, OwnerMapPackageName);
	DataAsset->PostEditChange();
	DataAsset->MarkPackageDirty();
	if (bOutIsNewAsset)
	{
		FAssetRegistryModule::AssetCreated(DataAsset);
	}
	OutPackagesToSave.AddUnique(DataAsset->GetOutermost());
	return DataAsset;
}

bool SaveGeneratedPackages(const TArray<UPackage*>& PackagesToSave)
{
	if (PackagesToSave.IsEmpty())
	{
		return true;
	}

	FAssetCompilingManager::Get().FinishAllCompilation();

	TArray<UPackage*> FailedPackages;
	FEditorFileUtils::FPromptForCheckoutAndSaveParams SaveParams;
	SaveParams.bCheckDirty = true;
	SaveParams.bPromptToSave = false;
	SaveParams.bCanBeDeclined = false;
	SaveParams.bIsExplicitSave = true;
	SaveParams.OutFailedPackages = &FailedPackages;
	const FEditorFileUtils::EPromptReturnCode SaveResult =
		FEditorFileUtils::PromptForCheckoutAndSave(
			PackagesToSave,
			SaveParams);
	if (SaveResult == FEditorFileUtils::PR_Success)
	{
		return true;
	}

	FString FailedPackageNames;
	for (const UPackage* FailedPackage : FailedPackages)
	{
		if (!FailedPackageNames.IsEmpty())
		{
			FailedPackageNames += TEXT(", ");
		}
		FailedPackageNames += GetNameSafe(FailedPackage);
	}
	return ReportBakeError(
		FText::Format(
			LOCTEXT(
				"PackageSaveFailed",
				"Generated minimap packages could not be saved. {0}"),
			FText::FromString(FailedPackageNames)));
}
}

bool FFrontierMinimapBaker::BakeAllFloors(
	AFrontierMinimapDefinitionActor& DefinitionActor)
{
	if (bMinimapBakeInProgress)
	{
		return ReportBakeError(
			LOCTEXT("BakeAlreadyRunning", "A minimap bake is already running."));
	}
	TGuardValue<bool> BakeGuard(bMinimapBakeInProgress, true);

	check(IsInGameThread());

	UWorld* World = nullptr;
	if (!ValidateWorldAndActor(DefinitionActor, World))
	{
		return false;
	}

	TArray<FMinimapFloorBakeWork> Floors;
	FString OutputDirectory;
	if (!ValidateSettings(DefinitionActor, Floors, OutputDirectory))
	{
		return false;
	}

	FScopedSlowTask SlowTask(
		7.0f,
		LOCTEXT("BakeProgress", "Baking minimap floors from static meshes..."));
	SlowTask.MakeDialog(true);
	SlowTask.EnterProgressFrame(
		1.0f,
		LOCTEXT("CollectingMeshes", "Collecting loaded Static Mesh components..."));
	if (World->GetWorldPartition())
	{
		return ReportBakeError(
			LOCTEXT(
				"WorldPartitionUnsupported",
				"World Partition minimap baking requires all cells to be loaded and is not supported by this button."));
	}

	FMinimapMeshBakeStats MeshStats;
	TArray<UStaticMeshComponent*> MeshComponents;
	if (!CollectMeshComponents(
			*World,
			DefinitionActor.GetBakeIgnoreTag(),
			MeshComponents,
			MeshStats))
	{
		return false;
	}

	if (SlowTask.ShouldCancel())
	{
		return false;
	}
	SlowTask.EnterProgressFrame(
		1.0f,
		LOCTEXT("CollectingInstances", "Collecting mesh instances and floor overlap..."));
	TArray<FMinimapMeshPlacement> MeshPlacements;
	if (!BuildMeshPlacements(
			MeshComponents,
			Floors,
			DefinitionActor.GetBakeFloorTransitionOverlap(),
			DefinitionActor.GetBakeIgnoreTag(),
			MeshPlacements,
			MeshStats))
	{
		return false;
	}

	if (SlowTask.ShouldCancel())
	{
		return false;
	}
	SlowTask.EnterProgressFrame(
		1.0f,
		LOCTEXT("ReadingMeshSources", "Reading unique Static Mesh source geometry..."));
	TMap<UStaticMesh*, FMinimapLocalMeshGeometry> GeometryCache;
	if (!BuildGeometryCache(
			MeshPlacements,
			GeometryCache,
			MeshStats))
	{
		return false;
	}

	if (SlowTask.ShouldCancel())
	{
		return false;
	}
	SlowTask.EnterProgressFrame(
		1.0f,
		LOCTEXT("MeasuringMeshFloors", "Clipping meshes to floor height ranges..."));
	if (!BuildFloorGeometryBounds(
			MeshPlacements,
			GeometryCache,
			Floors,
			DefinitionActor.GetBakeWorldPadding(),
			DefinitionActor.GetBakeFloorTransitionOverlap()))
	{
		return false;
	}

	const int32 Resolution = DefinitionActor.GetBakeTextureResolution();
	const int32 OutlineThickness =
		DefinitionActor.GetBakeOutlineThicknessPixels();
	const FColor FillColor =
		DefinitionActor.GetBakeFillColor().ToFColorSRGB();
	const FColor OutlineColor =
		DefinitionActor.GetBakeOutlineColor().ToFColorSRGB();
	if (SlowTask.ShouldCancel())
	{
		return false;
	}
	SlowTask.EnterProgressFrame(
		1.0f,
		LOCTEXT("RasterizingMeshes", "Rasterizing top-down mesh depth and outlines..."));
	if (!RasterizeFloorMeshes(
			MeshPlacements,
			GeometryCache,
			Floors,
			DefinitionActor.GetBakeFloorTransitionOverlap(),
			Resolution,
			OutlineThickness))
	{
		return false;
	}

	for (FMinimapFloorBakeWork& Floor : Floors)
	{
		Floor.TextureContentHash = CalculateTextureContentHash(
			Floor,
			Resolution,
			OutlineThickness,
			FillColor,
			OutlineColor);
	}
	if (SlowTask.ShouldCancel())
	{
		return false;
	}

	const FString OwnerMapPackageName =
		World->GetOutermost()->GetName();
	const FString MapShortName = ObjectTools::SanitizeObjectName(
		FPackageName::GetShortName(OwnerMapPackageName));
	const FString MapAssetSuffix = FString::Printf(
		TEXT("%s_%08X"),
		*MapShortName,
		FCrc::StrCrc32(*OwnerMapPackageName));
	FString DataAssetName;
	FString DataAssetPackageName;
	FString MapOutputDirectory;
	UFrontierMinimapDataAsset* DataAsset = nullptr;
	if (MapShortName.IsEmpty()
		|| !PreflightOutputAssets(
			MapAssetSuffix,
			OwnerMapPackageName,
			OutputDirectory,
			Floors,
			DataAssetName,
			DataAssetPackageName,
			DataAsset,
			MapOutputDirectory))
	{
		return false;
	}

	SlowTask.EnterProgressFrame(
		1.0f,
		LOCTEXT(
			"UpdatingTextures",
			"Creating content-addressed minimap textures..."));
	TArray<UPackage*> TexturePackagesToSave;
	for (FMinimapFloorBakeWork& Floor : Floors)
	{
		if (!CreateOrReuseTexture(
				Floor,
				Resolution,
				OutlineThickness,
				FillColor,
				OutlineColor,
				OwnerMapPackageName,
				TexturePackagesToSave))
		{
			return ReportBakeError(
				FText::Format(
					LOCTEXT(
						"TextureCreationFailed",
						"Could not create or update texture for floor '{0}'."),
					FText::FromName(Floor.Definition->FloorId)));
		}
	}

	if (!SaveGeneratedPackages(TexturePackagesToSave))
	{
		return false;
	}

	SlowTask.EnterProgressFrame(
		1.0f,
		LOCTEXT(
			"UpdatingDataAsset",
			"Updating and saving the minimap data asset..."));
	const bool bHadExistingDataAsset = DataAsset != nullptr;
	const TArray<FFrontierMinimapFloorDefinition> PreviousFloors =
		bHadExistingDataAsset
			? DataAsset->Floors
			: TArray<FFrontierMinimapFloorDefinition>();
	TArray<UPackage*> DataAssetPackagesToSave;
	bool bIsNewDataAsset = false;
	DataAsset = CreateOrUpdateDataAsset(
		DataAsset,
		DataAssetName,
		DataAssetPackageName,
		Floors,
		OwnerMapPackageName,
		bIsNewDataAsset,
		DataAssetPackagesToSave);
	if (!DataAsset)
	{
		return ReportBakeError(
			LOCTEXT(
				"DataAssetCreationFailed",
				"Could not create or update the minimap data asset."));
	}

	if (!SaveGeneratedPackages(DataAssetPackagesToSave))
	{
		if (bHadExistingDataAsset && !bIsNewDataAsset)
		{
			DataAsset->Floors = PreviousFloors;
			DataAsset->PostEditChange();
		}
		return false;
	}

	DefinitionActor.SetMinimapDataAsset(DataAsset);
	DefinitionActor.PostEditChange();
	ReportBakeSuccess(
		FText::Format(
			LOCTEXT(
				"BakeComplete",
				"Baked {0} minimap floor(s) to {1}.\nSave the level to keep the generated Data Asset assignment."),
			Floors.Num(),
			FText::FromString(MapOutputDirectory)));
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierMinimapRasterizerTest,
	"Frontier.Editor.Minimap.MeshRasterizer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierMinimapRasterizerTest::RunTest(const FString& Parameters)
{
	constexpr int32 Resolution = 16;
	TArray<uint8> Occupancy;
	Occupancy.Init(0, Resolution * Resolution);
	RasterizeTriangle(
		FVector2f(2.0f, 2.0f),
		FVector2f(14.0f, 2.0f),
		FVector2f(2.0f, 14.0f),
		Resolution,
		Occupancy);

	TestEqual(
		TEXT("A pixel inside the triangle is filled"),
		Occupancy[4 * Resolution + 4],
		static_cast<uint8>(255));
	TestEqual(
		TEXT("A pixel outside the triangle stays empty"),
		Occupancy[14 * Resolution + 14],
		static_cast<uint8>(0));

	TArray<uint8> DepthOccupancy;
	DepthOccupancy.Init(0, Resolution * Resolution);
	TArray<float> TopDepth;
	TopDepth.Init(-TNumericLimits<float>::Max(), Resolution * Resolution);
	RasterizeTriangleDepth(
		FVector2f(1.0f, 1.0f),
		FVector2f(15.0f, 1.0f),
		FVector2f(1.0f, 15.0f),
		0.0f,
		0.0f,
		0.0f,
		Resolution,
		DepthOccupancy,
		TopDepth);
	RasterizeTriangleDepth(
		FVector2f(4.0f, 4.0f),
		FVector2f(10.0f, 4.0f),
		FVector2f(4.0f, 10.0f),
		75.0f,
		75.0f,
		75.0f,
		Resolution,
		DepthOccupancy,
		TopDepth);
	TestEqual(
		TEXT("Furniture above a floor wins the top-down depth test"),
		TopDepth[5 * Resolution + 5],
		75.0f);
	TArray<uint8> FeatureSeeds;
	FeatureSeeds.Init(0, Resolution * Resolution);
	AddDepthEdgeSeeds(
		DepthOccupancy,
		TopDepth,
		Resolution,
		FeatureSeeds);
	TestTrue(
		TEXT("Furniture height creates an internal minimap outline"),
		FeatureSeeds.Contains(255));

	TArray<uint8> VerticalFeatureSeeds;
	VerticalFeatureSeeds.Init(0, Resolution * Resolution);
	TArray<float> VerticalFeatureDepth;
	VerticalFeatureDepth.Init(
		-TNumericLimits<float>::Max(),
		Resolution * Resolution);
	RasterizeFeatureLine(
		FVector2f(2.0f, 8.0f),
		FVector2f(14.0f, 8.0f),
		50.0f,
		Resolution,
		VerticalFeatureSeeds,
		VerticalFeatureDepth);
	TestEqual(
		TEXT("A projected vertical wall produces a feature line"),
		VerticalFeatureSeeds[8 * Resolution + 8],
		static_cast<uint8>(255));
	FMinimapFloorBakeWork OcclusionFloor;
	OcclusionFloor.Occupancy.Init(255, Resolution * Resolution);
	OcclusionFloor.TopDepth.Init(100.0f, Resolution * Resolution);
	FinalizeFloorRaster(
		OcclusionFloor,
		Resolution,
		1,
		VerticalFeatureSeeds,
		VerticalFeatureDepth);
	TestEqual(
		TEXT("A vertical feature behind a higher surface is hidden"),
		OcclusionFloor.FeatureMask[8 * Resolution + 8],
		static_cast<uint8>(0));

	TArray<uint8> SolidOccupancy;
	SolidOccupancy.Init(255, 5 * 5);
	TArray<uint8> OutlineDistance;
	BuildOutlineDistances(
		SolidOccupancy,
		5,
		2,
		OutlineDistance);
	TestEqual(
		TEXT("A solid mask edge is outline distance one"),
		OutlineDistance[0],
		static_cast<uint8>(1));
	TestEqual(
		TEXT("A two-pixel interior is outline distance two"),
		OutlineDistance[1 * 5 + 1],
		static_cast<uint8>(2));
	TestEqual(
		TEXT("A pixel deeper than the requested outline remains fill"),
		OutlineDistance[2 * 5 + 2],
		static_cast<uint8>(0));

	const FMinimapBakeTriangle SlopedTriangle
	{
		FVector(0.0, 0.0, -10.0),
		FVector(100.0, 0.0, 10.0),
		FVector(0.0, 100.0, 10.0)
	};
	FClippedPolygon ClippedPolygon;
	ClipTriangleToHeightRange(
		SlopedTriangle,
		0.0f,
		5.0f,
		ClippedPolygon);
	TestTrue(
		TEXT("A sloped mesh triangle is clipped into the floor slab"),
		!ClippedPolygon.IsEmpty());
	for (const FVector& Vertex : ClippedPolygon)
	{
		TestTrue(
			TEXT("Every clipped vertex stays inside the floor slab"),
			Vertex.Z >= 0.0 && Vertex.Z <= 5.0);
	}

	const FVector2f CenterPixel =
		WorldToPixel(
			FVector(100.0, 200.0, 0.0),
			FVector2D(100.0, 200.0),
			400.0f,
			100);
	TestEqual(TEXT("World center maps to texture center"), CenterPixel, FVector2f(50.0f, 50.0f));

	const FVector2f NorthPixel =
		WorldToPixel(
			FVector(300.0, 200.0, 0.0),
			FVector2D(100.0, 200.0),
			400.0f,
			100);
	TestEqual(TEXT("World +X maps to texture top"), NorthPixel.Y, 0.0f);

	const FVector2f EastPixel =
		WorldToPixel(
			FVector(100.0, 400.0, 0.0),
			FVector2D(100.0, 200.0),
			400.0f,
			100);
	TestEqual(TEXT("World +Y maps to texture right"), EastPixel.X, 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierRaidMinimapMeshExtractionTest,
	"Frontier.Editor.Minimap.RaidMapMeshExtraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFrontierRaidMinimapMeshExtractionTest::RunTest(
	const FString& Parameters)
{
	FAutomationEditorCommonUtils::LoadMap(
		TEXT("/Game/Levels/Raid/L_Raid_MedievalDungeon"));
	UWorld* World =
		GEditor
			? GEditor->GetEditorWorldContext().World()
			: nullptr;
	if (!TestNotNull(TEXT("Raid map editor world loads"), World))
	{
		return false;
	}

	FMinimapMeshBakeStats Stats;
	TArray<UStaticMeshComponent*> Components;
	if (!CollectMeshComponents(
			*World,
			TEXT("MinimapIgnore"),
			Components,
			Stats))
	{
		return false;
	}
	TestTrue(
		TEXT("Raid map exposes more than one thousand Static Mesh components"),
		Components.Num() > 1000);

	int32 StaticComponentCount = 0;
	int32 MovableComponentCount = 0;
	int32 InstanceCount = 0;
	TArray<UStaticMesh*> UniqueMeshes;
	for (UStaticMeshComponent* Component : Components)
	{
		if (Component->Mobility == EComponentMobility::Static)
		{
			++StaticComponentCount;
		}
		else if (Component->Mobility == EComponentMobility::Movable)
		{
			++MovableComponentCount;
		}
		if (const UInstancedStaticMeshComponent* InstancedComponent =
			Cast<UInstancedStaticMeshComponent>(Component))
		{
			InstanceCount += InstancedComponent->GetInstanceCount();
		}
		UniqueMeshes.AddUnique(Component->GetStaticMesh());
	}
	TestTrue(
		TEXT("Static components are included"),
		StaticComponentCount > 0);
	TestTrue(
		TEXT("Movable doors and props are included"),
		MovableComponentCount > 0);
	TestTrue(
		TEXT("Instanced foliage is included per instance"),
		InstanceCount > 0);

	FStaticMeshCompilingManager::Get().FinishCompilation(UniqueMeshes);
	int64 TriangleCount = 0;
	for (UStaticMesh* StaticMesh : UniqueMeshes)
	{
		FMinimapLocalMeshGeometry Geometry;
		if (StaticMesh && BuildLocalMeshGeometry(*StaticMesh, Geometry))
		{
			TriangleCount += Geometry.Triangles.Num();
		}
	}
	TestTrue(
		TEXT("Raid Static Meshes expose source triangles to the CPU baker"),
		TriangleCount > 0);

	TArray<UStaticMeshComponent*> TemporarilyIgnoredOversizedComponents;
	for (UStaticMeshComponent* Component : Components)
	{
		const FVector BoundsSize = Component->Bounds.BoxExtent * 2.0;
		if (FMath::Max(BoundsSize.X, BoundsSize.Y)
			> MinimapMaximumAutomaticMeshWidth)
		{
			Component->ComponentTags.AddUnique(TEXT("MinimapIgnore"));
			TemporarilyIgnoredOversizedComponents.Add(Component);
		}
	}
	TArray<FFrontierMinimapBakeFloorDefinition> TestFloorDefinitions;
	TestFloorDefinitions.SetNum(2);
	TestFloorDefinitions[0].FloorId = TEXT("CryptTest");
	TestFloorDefinitions[1].FloorId = TEXT("DungeonTest");
	TArray<FMinimapFloorBakeWork> TestFloors;
	TestFloors.SetNum(2);
	TestFloors[0].Definition = &TestFloorDefinitions[0];
	TestFloors[0].MinMeshZ = -550.0f;
	TestFloors[0].MaxMeshZ = -275.0f;
	TestFloors[1].Definition = &TestFloorDefinitions[1];
	TestFloors[1].MinMeshZ = -50.0f;
	TestFloors[1].MaxMeshZ = 300.0f;

	FMinimapMeshBakeStats PipelineStats;
	TArray<FMinimapMeshPlacement> TestPlacements;
	TMap<UStaticMesh*, FMinimapLocalMeshGeometry> TestGeometryCache;
	const bool bPlacementCollectionSucceeded = BuildMeshPlacements(
		Components,
		TestFloors,
		0.0f,
		TEXT("MinimapIgnore"),
		TestPlacements,
		PipelineStats);
	const bool bCacheSucceeded = bPlacementCollectionSucceeded
		&& BuildGeometryCache(
			TestPlacements,
			TestGeometryCache,
			PipelineStats);
	const bool bBoundsSucceeded = bCacheSucceeded
		&& BuildFloorGeometryBounds(
			TestPlacements,
			TestGeometryCache,
			TestFloors,
			500.0f,
			0.0f);
	const bool bRasterSucceeded = bBoundsSucceeded
		&& RasterizeFloorMeshes(
			TestPlacements,
			TestGeometryCache,
			TestFloors,
			0.0f,
			128,
			2);
	for (UStaticMeshComponent* Component
		: TemporarilyIgnoredOversizedComponents)
	{
		Component->ComponentTags.Remove(TEXT("MinimapIgnore"));
	}
	TestTrue(
		TEXT("The actual raid meshes complete the floor clipping and raster pipeline"),
		bRasterSucceeded);
	if (bRasterSucceeded)
	{
		for (const FMinimapFloorBakeWork& Floor : TestFloors)
		{
			TestTrue(
				TEXT("A raid floor contains filled mesh pixels"),
				Floor.Occupancy.Contains(255));
			TestTrue(
				TEXT("A raid floor contains furniture or wall feature outlines"),
				Floor.FeatureMask.Contains(255));
		}
	}

	AActor* TagTestActor = World->SpawnActor<AActor>();
	UStaticMeshComponent* TagTestComponent =
		NewObject<UStaticMeshComponent>(TagTestActor);
	TagTestActor->AddInstanceComponent(TagTestComponent);
	TestFalse(
		TEXT("An untagged mesh component is included"),
		HasMinimapIgnoreTag(*TagTestComponent, TEXT("MinimapIgnore")));
	TagTestActor->Tags.Add(TEXT("MinimapIgnore"));
	TestTrue(
		TEXT("An Actor tag excludes all of its mesh components"),
		HasMinimapIgnoreTag(*TagTestComponent, TEXT("MinimapIgnore")));
	TagTestActor->Tags.Reset();
	TagTestComponent->ComponentTags.Add(TEXT("MinimapIgnore"));
	TestTrue(
		TEXT("A Component tag excludes only that mesh component"),
		HasMinimapIgnoreTag(*TagTestComponent, TEXT("MinimapIgnore")));
	TagTestActor->Destroy();

	AddInfo(
		FString::Printf(
			TEXT("Raid minimap mesh source: %d components, %d unique meshes, %d instances, %lld triangles."),
			Components.Num(),
			UniqueMeshes.Num(),
			InstanceCount,
			TriangleCount));
	return true;
}

#endif

#undef LOCTEXT_NAMESPACE
