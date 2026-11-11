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
			"UMG",
			"Slate",
			"SlateCore"
		});

		// 서버와 같은 패킷 정의를 사용해 필드 크기와 번호가 달라지는 것을 막는다.
		PrivateIncludePaths.Add(System.IO.Path.GetFullPath(System.IO.Path.Combine(ModuleDirectory, "../../../Server/Packets")));
		PrivateIncludePaths.Add(System.IO.Path.GetFullPath(System.IO.Path.Combine(ModuleDirectory, "../../../Server/Common")));
		// 서버와 같은 설정값을 쓴다.
		PrivateIncludePaths.Add(System.IO.Path.GetFullPath(System.IO.Path.Combine(ModuleDirectory, "../../../Server/Settings")));
		// 서버와 같은 공용 이동 계산을 쓴다. 소스는 Player/shared_movement_build.cpp가 함께 빌드한다.
		PrivateIncludePaths.Add(System.IO.Path.GetFullPath(System.IO.Path.Combine(ModuleDirectory, "../../../Server/SharedMovement")));

		PrivateDependencyModuleNames.AddRange(new string[] { "Sockets", "Networking", "Niagara", "MoviePlayer", "RenderCore", "RHI", "Landscape", "PhysicsCore", "Foliage" });

		PublicIncludePaths.AddRange(new string[] {
			"ThatsMine"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
