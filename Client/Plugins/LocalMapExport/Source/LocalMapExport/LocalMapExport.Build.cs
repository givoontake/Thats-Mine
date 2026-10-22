using UnrealBuildTool;
public class LocalMapExport : ModuleRules
{
    public LocalMapExport(ReadOnlyTargetRules Target) : base(Target)
    {
        // 게임 모듈의 캐릭터 헤더가 서버 패킷 정의를 포함한다.
        PrivateIncludePaths.Add(System.IO.Path.GetFullPath(System.IO.Path.Combine(ModuleDirectory, "../../../../../Server/Packets")));
        PrivateIncludePaths.Add(System.IO.Path.GetFullPath(System.IO.Path.Combine(ModuleDirectory, "../../../../../Server/Common")));
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "RenderCore", "Landscape", "Foliage", "PhysicsCore", "ThatsMine" });
    }
}
