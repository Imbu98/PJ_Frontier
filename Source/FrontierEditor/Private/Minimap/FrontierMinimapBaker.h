#pragma once

class AFrontierMinimapDefinitionActor;

/** Creates deterministic minimap assets from loaded editor-world static meshes. */
class FFrontierMinimapBaker
{
public:
	static bool BakeAllFloors(
		AFrontierMinimapDefinitionActor& DefinitionActor);
};
