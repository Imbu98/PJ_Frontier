#include "Minimap/FrontierEditorModuleInterface.h"

#include "Minimap/FrontierMinimapBaker.h"
#include "Modules/ModuleManager.h"

class FFrontierEditorModule final : public IFrontierEditorModule
{
public:
	virtual bool BakeAllMinimapFloors(
		AFrontierMinimapDefinitionActor& DefinitionActor) override
	{
		return FFrontierMinimapBaker::BakeAllFloors(DefinitionActor);
	}
};

IMPLEMENT_MODULE(FFrontierEditorModule, FrontierEditor)
