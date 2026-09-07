using UnrealBuildTool;

public class FrontierOnline : ModuleRules
{
	public FrontierOnline(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"HTTP"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json"
		});
	}
}
