#include "Components/FrontierMinimapComponent.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Frontier.h"
#include "FrontierPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Minimap/FrontierMinimapDataAsset.h"
#include "Minimap/FrontierMinimapDefinitionActor.h"
#include "UI/FrontierMinimapWidget.h"

namespace
{
constexpr float InitializationPollInterval = 0.25f;
constexpr float MinimapViewportMargin = 24.0f;
constexpr int32 MinimapViewportZOrder = 1;
}

UFrontierMinimapComponent::UFrontierMinimapComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

FVector2D UFrontierMinimapComponent::WorldLocationToMapUV(
	const FVector& WorldLocation,
	const FVector2D& MapWorldCenter,
	const float MapWorldWidth)
{
	const float SafeWorldWidth = FMath::Max(MapWorldWidth, UE_SMALL_NUMBER);

	// A north-up texture has world +X at the top and world +Y at the right.
	return FVector2D(
		0.5f + ((WorldLocation.Y - MapWorldCenter.Y) / SafeWorldWidth),
		0.5f - ((WorldLocation.X - MapWorldCenter.X) / SafeWorldWidth));
}

FBox2f UFrontierMinimapComponent::CalculateVisibleUVRegion(
	const FVector2D& PlayerMapUV,
	const float VisibleWorldWidth,
	const float MapWorldWidth,
	FVector2D& OutMarkerNormalizedPosition)
{
	const float SafeMapWorldWidth = FMath::Max(MapWorldWidth, UE_SMALL_NUMBER);
	const float VisibleFraction = FMath::Clamp(
		VisibleWorldWidth / SafeMapWorldWidth,
		UE_SMALL_NUMBER,
		1.0f);
	const float HalfVisibleFraction = VisibleFraction * 0.5f;

	const FVector2D ClampedViewCenter(
		FMath::Clamp(
			PlayerMapUV.X,
			HalfVisibleFraction,
			1.0f - HalfVisibleFraction),
		FMath::Clamp(
			PlayerMapUV.Y,
			HalfVisibleFraction,
			1.0f - HalfVisibleFraction));

	const FVector2D RegionMin =
		ClampedViewCenter - FVector2D(HalfVisibleFraction, HalfVisibleFraction);
	const FVector2D RegionMax =
		ClampedViewCenter + FVector2D(HalfVisibleFraction, HalfVisibleFraction);

	OutMarkerNormalizedPosition = FVector2D(
		FMath::Clamp(
			(PlayerMapUV.X - RegionMin.X) / VisibleFraction,
			0.0f,
			1.0f),
		FMath::Clamp(
			(PlayerMapUV.Y - RegionMin.Y) / VisibleFraction,
			0.0f,
			1.0f));

	return FBox2f(FVector2f(RegionMin), FVector2f(RegionMax));
}

bool UFrontierMinimapComponent::IsHeightWithinFloorWithHysteresis(
	const FFrontierMinimapFloorDefinition& Floor,
	const float WorldZ,
	const float Hysteresis)
{
	const float SafeHysteresis = FMath::Max(0.0f, Hysteresis);
	const float LowerHeight =
		FMath::Min(Floor.MinWorldZ, Floor.MaxWorldZ) - SafeHysteresis;
	const float UpperHeight =
		FMath::Max(Floor.MinWorldZ, Floor.MaxWorldZ) + SafeHysteresis;
	return WorldZ >= LowerHeight && WorldZ <= UpperHeight;
}

void UFrontierMinimapComponent::BeginPlay()
{
	Super::BeginPlay();

	bInitializationStarted = false;
	bMissingDefinitionWarningLogged = false;
	OwningController = Cast<AFrontierPlayerController>(GetOwner());
	if (!OwningController || !OwningController->IsLocalController())
	{
		SetComponentTickEnabled(false);
		return;
	}

	SetComponentTickInterval(InitializationPollInterval);
	if (ShouldDisplayMinimap())
	{
		TryInitializeMinimap();
	}
}

void UFrontierMinimapComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPendingLoads();
	ReleaseMinimapWidget();
	LoadedFloorTextures.Reset();
	LoadedMinimapData = nullptr;
	ResolvedMinimapDataAsset.Reset();
	OwningController = nullptr;
	bInitializationStarted = false;
	bMissingDefinitionWarningLogged = false;
	SetComponentTickEnabled(false);
	Super::EndPlay(EndPlayReason);
}

void UFrontierMinimapComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!ShouldDisplayMinimap())
	{
		if (MinimapWidget)
		{
			MinimapWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (!bInitializationStarted)
	{
		TryInitializeMinimap();
		return;
	}

	if (!MinimapWidget && LoadedMinimapData && !LoadedFloorTextures.IsEmpty())
	{
		EnsureMinimapWidget();
	}

	const APawn* Pawn = OwningController ? OwningController->GetPawn() : nullptr;
	if (!Pawn || !LoadedMinimapData || !MinimapWidget)
	{
		if (MinimapWidget)
		{
			MinimapWidget->SetPlayerMarkerVisible(false);
			MinimapWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	UpdateMinimap(Pawn);
}

bool UFrontierMinimapComponent::ShouldDisplayMinimap() const
{
	return OwningController
		&& OwningController->IsLocalController()
		&& !OwningController->IsInLobbyContext();
}

void UFrontierMinimapComponent::TryInitializeMinimap()
{
	if (bInitializationStarted || !ShouldDisplayMinimap())
	{
		return;
	}

	if (!ResolveMinimapDataAsset())
	{
		return;
	}

	bInitializationStarted = true;
	BeginDataAssetLoad();
}

bool UFrontierMinimapComponent::ResolveMinimapDataAsset()
{
	if (!ResolvedMinimapDataAsset.IsNull())
	{
		return true;
	}

	AFrontierMinimapDefinitionActor* DefinitionActor = nullptr;
	int32 DefinitionCount = 0;
	for (TActorIterator<AFrontierMinimapDefinitionActor> It(GetWorld()); It; ++It)
	{
		++DefinitionCount;
		if (!DefinitionActor)
		{
			DefinitionActor = *It;
		}
	}

	if (!DefinitionActor)
	{
		if (!bMissingDefinitionWarningLogged)
		{
			bMissingDefinitionWarningLogged = true;
			FRONTIER_LOG(
				Warning,
				TEXT("No FrontierMinimapDefinitionActor was found in the raid level. "
					"The local minimap will retry while the level finishes loading."));
		}
		return false;
	}

	if (DefinitionCount > 1)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Found %d FrontierMinimapDefinitionActors. Using %s; "
				"keep exactly one in the persistent raid level."),
			DefinitionCount,
			*GetNameSafe(DefinitionActor));
	}

	ResolvedMinimapDataAsset = DefinitionActor->GetMinimapDataAsset();
	if (ResolvedMinimapDataAsset.IsNull())
	{
		bInitializationStarted = true;
		FRONTIER_LOG(
			Error,
			TEXT("FrontierMinimapDefinitionActor %s has no MinimapDataAsset assigned."),
			*GetNameSafe(DefinitionActor));
		return false;
	}

	return true;
}

void UFrontierMinimapComponent::BeginDataAssetLoad()
{
	const FSoftObjectPath DataAssetPath =
		ResolvedMinimapDataAsset.ToSoftObjectPath();
	if (!DataAssetPath.IsValid())
	{
		FRONTIER_LOG(Error, TEXT("Resolved minimap data asset path is invalid."));
		return;
	}

	if (ResolvedMinimapDataAsset.Get())
	{
		HandleDataAssetLoaded();
		return;
	}

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		FRONTIER_LOG(Error, TEXT("AssetManager is unavailable while loading minimap data."));
		return;
	}

	DataAssetLoadHandle =
		AssetManager->GetStreamableManager().RequestAsyncLoad(
			DataAssetPath,
			FStreamableDelegate::CreateWeakLambda(this, [this]()
			{
				HandleDataAssetLoaded();
			}));

	if (!DataAssetLoadHandle)
	{
		FRONTIER_LOG(
			Error,
			TEXT("Failed to request minimap data asset load. Asset=%s"),
			*DataAssetPath.ToString());
	}
}

void UFrontierMinimapComponent::HandleDataAssetLoaded()
{
	if (!HasBegunPlay())
	{
		DataAssetLoadHandle.Reset();
		return;
	}

	LoadedMinimapData = ResolvedMinimapDataAsset.Get();
	DataAssetLoadHandle.Reset();
	if (!LoadedMinimapData)
	{
		FRONTIER_LOG(
			Error,
			TEXT("Failed to load minimap data asset. Asset=%s"),
			*ResolvedMinimapDataAsset.ToSoftObjectPath().ToString());
		return;
	}

	if (LoadedMinimapData->Floors.IsEmpty())
	{
		FRONTIER_LOG(
			Error,
			TEXT("Minimap data asset has no floor definitions. Asset=%s"),
			*GetNameSafe(LoadedMinimapData));
		return;
	}

	BeginFloorTexturePreload();
}

void UFrontierMinimapComponent::BeginFloorTexturePreload()
{
	if (!LoadedMinimapData)
	{
		return;
	}

	TArray<FSoftObjectPath> TexturePaths;
	bool bAllTexturesAlreadyLoaded = true;
	for (const FFrontierMinimapFloorDefinition& Floor : LoadedMinimapData->Floors)
	{
		if (!Floor.IsValid())
		{
			continue;
		}

		TexturePaths.AddUnique(Floor.FloorTexture.ToSoftObjectPath());
		bAllTexturesAlreadyLoaded &= Floor.FloorTexture.Get() != nullptr;
	}

	if (TexturePaths.IsEmpty())
	{
		FRONTIER_LOG(
			Error,
			TEXT("Minimap data asset has no valid floor textures. Asset=%s"),
			*GetNameSafe(LoadedMinimapData));
		return;
	}

	if (bAllTexturesAlreadyLoaded)
	{
		HandleFloorTexturesLoaded();
		return;
	}

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		FRONTIER_LOG(
			Error,
			TEXT("AssetManager is unavailable while preloading minimap floor textures."));
		return;
	}

	FloorTextureLoadHandle =
		AssetManager->GetStreamableManager().RequestAsyncLoad(
			TexturePaths,
			FStreamableDelegate::CreateWeakLambda(this, [this]()
			{
				HandleFloorTexturesLoaded();
			}));

	if (!FloorTextureLoadHandle)
	{
		FRONTIER_LOG(
			Error,
			TEXT("Failed to request minimap floor texture preload. Asset=%s"),
			*GetNameSafe(LoadedMinimapData));
	}
}

void UFrontierMinimapComponent::HandleFloorTexturesLoaded()
{
	if (!HasBegunPlay() || !LoadedMinimapData)
	{
		FloorTextureLoadHandle.Reset();
		return;
	}

	LoadedFloorTextures.SetNum(LoadedMinimapData->Floors.Num());
	int32 LoadedTextureCount = 0;
	for (int32 FloorIndex = 0;
		FloorIndex < LoadedMinimapData->Floors.Num();
		++FloorIndex)
	{
		const FFrontierMinimapFloorDefinition& Floor =
			LoadedMinimapData->Floors[FloorIndex];
		UTexture2D* LoadedTexture = Floor.FloorTexture.Get();
		LoadedFloorTextures[FloorIndex] = LoadedTexture;
		if (LoadedTexture)
		{
			++LoadedTextureCount;
		}
		else if (!Floor.FloorTexture.IsNull())
		{
			FRONTIER_LOG(
				Error,
				TEXT("Failed to preload minimap texture. Floor=%s Texture=%s"),
				Floor.FloorId.IsNone()
					? TEXT("<unnamed>")
					: *Floor.FloorId.ToString(),
				*Floor.FloorTexture.ToSoftObjectPath().ToString());
		}
	}
	FloorTextureLoadHandle.Reset();

	if (LoadedTextureCount == 0)
	{
		LoadedFloorTextures.Reset();
		FRONTIER_LOG(
			Error,
			TEXT("No minimap floor texture could be loaded. Asset=%s"),
			*GetNameSafe(LoadedMinimapData));
		return;
	}

	SetComponentTickInterval(FMath::Max(0.016f, UpdateInterval));
	EnsureMinimapWidget();

	if (MinimapWidget && OwningController)
	{
		UpdateMinimap(OwningController->GetPawn());
	}
}

void UFrontierMinimapComponent::CancelPendingLoads()
{
	if (DataAssetLoadHandle)
	{
		DataAssetLoadHandle->CancelHandle();
		DataAssetLoadHandle.Reset();
	}

	if (FloorTextureLoadHandle)
	{
		FloorTextureLoadHandle->CancelHandle();
		FloorTextureLoadHandle.Reset();
	}
}

void UFrontierMinimapComponent::EnsureMinimapWidget()
{
	if (!ShouldDisplayMinimap()
		|| !OwningController
		|| !MinimapWidgetClass
		|| !LoadedMinimapData
		|| MinimapWidget)
	{
		return;
	}

	if (!LoadedFloorTextures.ContainsByPredicate(
		[](const TObjectPtr<UTexture2D>& Texture)
		{
			return Texture != nullptr;
		}))
	{
		return;
	}

	MinimapWidget = CreateWidget<UFrontierMinimapWidget>(
		OwningController,
		MinimapWidgetClass);
	if (!MinimapWidget)
	{
		FRONTIER_LOG(
			Error,
			TEXT("Failed to create minimap widget. Class=%s"),
			*GetNameSafe(MinimapWidgetClass));
		return;
	}

	MinimapWidget->AddToViewport(MinimapViewportZOrder);
	MinimapWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
	MinimapWidget->SetPositionInViewport(
		FVector2D(MinimapViewportMargin, MinimapViewportMargin),
		false);
	MinimapWidget->SetDesiredSizeInViewport(FVector2D(MinimapSize, MinimapSize));
	MinimapWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void UFrontierMinimapComponent::ReleaseMinimapWidget()
{
	if (MinimapWidget)
	{
		MinimapWidget->RemoveFromParent();
		MinimapWidget = nullptr;
	}

	ActiveFloorIndex = INDEX_NONE;
}

void UFrontierMinimapComponent::UpdateMinimap(const APawn* Pawn)
{
	if (!Pawn || !LoadedMinimapData || !MinimapWidget)
	{
		return;
	}

	const FVector PawnLocation = Pawn->GetActorLocation();
	const int32 FloorIndex = ResolveFloorIndex(PawnLocation.Z);
	const FFrontierMinimapFloorDefinition* Floor =
		LoadedMinimapData->GetFloor(FloorIndex);
	if (!Floor)
	{
		ActiveFloorIndex = INDEX_NONE;
		MinimapWidget->SetPlayerMarkerVisible(false);
		MinimapWidget->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (FloorIndex != ActiveFloorIndex)
	{
		if (!SetActiveFloor(FloorIndex, *Floor))
		{
			MinimapWidget->SetPlayerMarkerVisible(false);
			MinimapWidget->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}
	}

	const FVector2D PlayerMapUV = WorldLocationToMapUV(
		PawnLocation,
		Floor->WorldCenter,
		Floor->WorldWidth);
	FVector2D MarkerNormalizedPosition = FVector2D(0.5f, 0.5f);
	const FBox2f VisibleUVRegion = CalculateVisibleUVRegion(
		PlayerMapUV,
		VisibleWorldWidth,
		Floor->WorldWidth,
		MarkerNormalizedPosition);

	MinimapWidget->SetMapView(
		VisibleUVRegion,
		MarkerNormalizedPosition,
		Pawn->GetActorRotation().Yaw);
	MinimapWidget->SetPlayerMarkerVisible(true);
	MinimapWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

int32 UFrontierMinimapComponent::ResolveFloorIndex(const float WorldZ) const
{
	if (!LoadedMinimapData)
	{
		return INDEX_NONE;
	}

	const FFrontierMinimapFloorDefinition* ActiveFloor =
		LoadedMinimapData->GetFloor(ActiveFloorIndex);
	if (ActiveFloor
		&& IsFloorTextureReady(ActiveFloorIndex)
		&& IsHeightWithinFloorWithHysteresis(
			*ActiveFloor,
			WorldZ,
			FloorHysteresis))
	{
		return ActiveFloorIndex;
	}

	const int32 NewFloorIndex = LoadedMinimapData->FindFloorIndex(WorldZ);
	return IsFloorTextureReady(NewFloorIndex) ? NewFloorIndex : INDEX_NONE;
}

bool UFrontierMinimapComponent::SetActiveFloor(
	const int32 FloorIndex,
	const FFrontierMinimapFloorDefinition& Floor)
{
	if (!MinimapWidget || !IsFloorTextureReady(FloorIndex))
	{
		ActiveFloorIndex = INDEX_NONE;
		return false;
	}

	ActiveFloorIndex = FloorIndex;
	MinimapWidget->SetMapTexture(LoadedFloorTextures[FloorIndex]);

	FRONTIER_LOG(
		Log,
		TEXT("Minimap floor changed. Floor=%s Index=%d Texture=%s"),
		Floor.FloorId.IsNone() ? TEXT("<unnamed>") : *Floor.FloorId.ToString(),
		FloorIndex,
		*GetNameSafe(LoadedFloorTextures[FloorIndex]));
	return true;
}

bool UFrontierMinimapComponent::IsFloorTextureReady(
	const int32 FloorIndex) const
{
	return LoadedFloorTextures.IsValidIndex(FloorIndex)
		&& LoadedFloorTextures[FloorIndex] != nullptr;
}
