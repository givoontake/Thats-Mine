// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class unreal_practice : ModuleRules
{
	public unreal_practice(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"unreal_practice",
			"unreal_practice/Variant_Horror",
			"unreal_practice/Variant_Horror/UI",
			"unreal_practice/Variant_Shooter",
			"unreal_practice/Variant_Shooter/AI",
			"unreal_practice/Variant_Shooter/UI",
			"unreal_practice/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
