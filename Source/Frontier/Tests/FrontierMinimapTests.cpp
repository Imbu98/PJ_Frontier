#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/FrontierMinimapComponent.h"
#include "Engine/Texture2D.h"
#include "Minimap/FrontierMinimapDataAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierMinimapWorldToUVTest,
	"Frontier.UI.Minimap.WorldToUV",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierMinimapWorldToUVTest::RunTest(const FString& Parameters)
{
	const FVector2D MapCenter(1000.0f, 2000.0f);
	constexpr float MapWorldWidth = 10000.0f;

	const FVector2D CenterUV =
		UFrontierMinimapComponent::WorldLocationToMapUV(
			FVector(1000.0f, 2000.0f, 0.0f),
			MapCenter,
			MapWorldWidth);
	TestEqual(TEXT("Map center converts to center U"), CenterUV.X, 0.5);
	TestEqual(TEXT("Map center converts to center V"), CenterUV.Y, 0.5);

	const FVector2D NorthUV =
		UFrontierMinimapComponent::WorldLocationToMapUV(
			FVector(6000.0f, 2000.0f, 0.0f),
			MapCenter,
			MapWorldWidth);
	TestEqual(TEXT("World +X maps to texture top"), NorthUV.Y, 0.0);

	const FVector2D EastUV =
		UFrontierMinimapComponent::WorldLocationToMapUV(
			FVector(1000.0f, 7000.0f, 0.0f),
			MapCenter,
			MapWorldWidth);
	TestEqual(TEXT("World +Y maps to texture right"), EastUV.X, 1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierMinimapVisibleRegionTest,
	"Frontier.UI.Minimap.VisibleRegion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierMinimapVisibleRegionTest::RunTest(const FString& Parameters)
{
	FVector2D MarkerPosition;
	const FBox2f CenterRegion =
		UFrontierMinimapComponent::CalculateVisibleUVRegion(
			FVector2D(0.5f, 0.5f),
			5000.0f,
			10000.0f,
			MarkerPosition);

	TestEqual(TEXT("Centered crop minimum U"), CenterRegion.Min.X, 0.25f);
	TestEqual(TEXT("Centered crop minimum V"), CenterRegion.Min.Y, 0.25f);
	TestEqual(TEXT("Centered crop maximum U"), CenterRegion.Max.X, 0.75f);
	TestEqual(TEXT("Centered crop maximum V"), CenterRegion.Max.Y, 0.75f);
	TestEqual(TEXT("Marker remains centered in an unclamped crop"), MarkerPosition, FVector2D(0.5f, 0.5f));

	const FBox2f EdgeRegion =
		UFrontierMinimapComponent::CalculateVisibleUVRegion(
			FVector2D(0.95f, 0.5f),
			5000.0f,
			10000.0f,
			MarkerPosition);
	TestEqual(TEXT("Crop clamps to right texture edge"), EdgeRegion.Max.X, 1.0f);
	TestEqual(TEXT("Marker moves inside an edge-clamped crop"), MarkerPosition.X, 0.9);

	const FBox2f FullRegion =
		UFrontierMinimapComponent::CalculateVisibleUVRegion(
			FVector2D(0.5f, 0.5f),
			20000.0f,
			10000.0f,
			MarkerPosition);
	TestEqual(TEXT("Oversized view uses full texture minimum"), FullRegion.Min, FVector2f(0.0f, 0.0f));
	TestEqual(TEXT("Oversized view uses full texture maximum"), FullRegion.Max, FVector2f(1.0f, 1.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierMinimapFloorSelectionTest,
	"Frontier.UI.Minimap.FloorSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierMinimapFloorSelectionTest::RunTest(const FString& Parameters)
{
	FFrontierMinimapFloorDefinition LowerFloor;
	LowerFloor.MinWorldZ = -100.0f;
	LowerFloor.MaxWorldZ = 250.0f;
	LowerFloor.WorldWidth = 10000.0f;

	FFrontierMinimapFloorDefinition UpperFloor;
	UpperFloor.MinWorldZ = 200.0f;
	UpperFloor.MaxWorldZ = 600.0f;
	UpperFloor.WorldWidth = 10000.0f;

	TestTrue(TEXT("Lower floor accepts its height"), LowerFloor.ContainsHeight(0.0f));
	TestFalse(TEXT("Lower floor rejects upper height"), LowerFloor.ContainsHeight(500.0f));
	TestTrue(TEXT("Overlapping transition height belongs to lower floor"), LowerFloor.ContainsHeight(225.0f));
	TestTrue(TEXT("Overlapping transition height belongs to upper floor"), UpperFloor.ContainsHeight(225.0f));
	TestTrue(
		TEXT("Hysteresis retains the lower floor just above its range"),
		UFrontierMinimapComponent::IsHeightWithinFloorWithHysteresis(
			LowerFloor,
			275.0f,
			50.0f));
	TestFalse(
		TEXT("Hysteresis releases the lower floor beyond its margin"),
		UFrontierMinimapComponent::IsHeightWithinFloorWithHysteresis(
			LowerFloor,
			301.0f,
			50.0f));

	UTexture2D* DummyTexture = NewObject<UTexture2D>();
	LowerFloor.FloorTexture = DummyTexture;
	UpperFloor.FloorTexture = DummyTexture;

	UFrontierMinimapDataAsset* DataAsset = NewObject<UFrontierMinimapDataAsset>();
	DataAsset->Floors = { LowerFloor, UpperFloor };
	TestEqual(
		TEXT("Overlap chooses the floor with the nearest height center"),
		DataAsset->FindFloorIndex(240.0f),
		1);
	TestEqual(
		TEXT("Height outside every configured range hides the minimap"),
		DataAsset->FindFloorIndex(900.0f),
		INDEX_NONE);
	return true;
}

#endif
