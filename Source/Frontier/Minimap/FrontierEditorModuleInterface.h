#pragma once

#include "Modules/ModuleInterface.h"

class AFrontierMinimapDefinitionActor;

/** Editor-only services exposed to the runtime module without an UnrealEd dependency. */
class IFrontierEditorModule : public IModuleInterface
{
public:
	virtual bool BakeAllMinimapFloors(
		AFrontierMinimapDefinitionActor& DefinitionActor) = 0;
};
