// WeaponKitTests 自动化测试模块的构建规则。模块类型是 UncookedOnly，不会打进正式包。
using UnrealBuildTool;

/// <summary>编辑器自动化测试模块。只在开发测试里链接 WeaponKit，不向游戏公开自己的头。</summary>
public class WeaponKitTests : ModuleRules
{
	/// <summary>测试代码私有依赖引擎和 WeaponKit。没有需要给其他模块包含的公开头。</summary>
	public WeaponKitTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"WeaponKit",
            "UMG",
            "SlateCore"
		});
	}
}
