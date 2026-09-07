#include "Minimap/FrontierMinimapDefinitionActor.h"

#include "Frontier.h"
#include "Minimap/FrontierMinimapDataAsset.h"

#if WITH_EDITOR
#include "Minimap/FrontierEditorModuleInterface.h"
#include "Modules/ModuleManager.h"
#endif

AFrontierMinimapDefinitionActor::AFrontierMinimapDefinitionActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

#if WITH_EDITORONLY_DATA
	BakeOutputDirectory.Path = TEXT("/Game/SY/Minimap/Generated");
	BakeFloors.Emplace();
#endif
}

void AFrontierMinimapDefinitionActor::BakeAllFloors()
{
#if WITH_EDITOR
	IFrontierEditorModule* EditorModule =
		FModuleManager::LoadModulePtr<IFrontierEditorModule>(
			TEXT("FrontierEditor"));
	if (!EditorModule)
	{
		FRONTIER_LOG(
			Error,
			TEXT("FrontierEditor module could not be loaded for the minimap bake."));
		return;
	}

	EditorModule->BakeAllMinimapFloors(*this);
#endif
}

#if WITH_EDITOR
void AFrontierMinimapDefinitionActor::SetMinimapDataAsset(
	UFrontierMinimapDataAsset* InMinimapDataAsset)
{
	Modify();
	MinimapDataAsset = InMinimapDataAsset;
	MarkPackageDirty();
}
#endif
