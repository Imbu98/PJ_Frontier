// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Frontier : ModuleRules
{
	public Frontier(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"GeometryCollectionEngine",
			"FieldSystemEngine",
			"Niagara",
			"NiagaraAnimNotifies",
			"UMG",
			"Slate",
			"SlateCore",
			"MoviePlayer",
			"NavigationSystem",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"NetCore",
			"AssetRegistry",
			"Json",
			"HTTP",
			"HTTPServer",
			"WebSockets",
			"DeveloperSettings",
			"FrontierOnline"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		if (Target.bBuildEditor)
		{
			// Editor-only multiplayer PIE automation tests use the official UnrealEd play-session API.
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		PublicIncludePaths.AddRange(new string[] {
			"Frontier",
			"Frontier/AbilitySystem",
			"Frontier/AbilitySystem/Abilities",
			"Frontier/Animation/Notifies",
			"Frontier/AI",
			"Frontier/AI/StateTree",
			"Frontier/Character",
			"Frontier/Combat",
			"Frontier/Components",
			"Frontier/Game",
			"Frontier/Inventory",
			"Frontier/Inventory/Items",
			"Frontier/Interaction",
			"Frontier/Progression",
			"Frontier/Skill",
			"Frontier/SkillTree",
			"Frontier/Warning",
			"Frontier/Tags",
			"Frontier/UI",
			"Frontier/Weapons",
		});

		// Steam party lobbies and profile avatars use the Steamworks lobby/friends APIs.
		AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");

		// UE's generic HTTP interface cannot attach a client certificate. Keep the
		// secret-bearing transport out of game clients and compile direct libcurl
		// support only into dedicated servers.
		if (Target.Type == TargetType.Server
			&& (Target.Platform == UnrealTargetPlatform.Linux
				|| Target.Platform == UnrealTargetPlatform.Win64))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl", "OpenSSL");
			PublicDefinitions.Add("FRONTIER_WITH_MTLS_CURL=1");
		}
		else
		{
			PublicDefinitions.Add("FRONTIER_WITH_MTLS_CURL=0");
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

