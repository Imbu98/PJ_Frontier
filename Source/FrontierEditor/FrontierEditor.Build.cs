// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FrontierEditor : ModuleRules
{
	public FrontierEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Frontier",
			"AssetRegistry",
			"MeshDescription",
			"StaticMeshDescription",
			"UnrealEd"
		});
	}
}
