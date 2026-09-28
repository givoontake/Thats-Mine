// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ThatsMine : ModuleRules
{
	public ThatsMine(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate",
			"SlateCore"
		});

		// 서버와 같은 패킷 정의를 사용해 필드 크기와 번호가 달라지는 것을 막는다.
		PrivateIncludePaths.Add(System.IO.Path.GetFullPath(System.IO.Path.Combine(ModuleDirectory, "../../../Server/Packets")));
		PrivateIncludePaths.Add(System.IO.Path.GetFullPath(System.IO.Path.Combine(ModuleDirectory, "../../../Server/Common")));

		PrivateDependencyModuleNames.AddRange(new string[] { "Sockets", "Networking", "Niagara" });

		PublicIncludePaths.AddRange(new string[] {
			"ThatsMine",
			"ThatsMine/Variant_Horror",
			"ThatsMine/Variant_Horror/UI",
			"ThatsMine/Variant_Shooter",
			"ThatsMine/Variant_Shooter/AI",
			"ThatsMine/Variant_Shooter/UI",
			"ThatsMine/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
