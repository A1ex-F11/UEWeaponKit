// WeaponKit 运行时模块的构建规则。游戏模块依赖 "WeaponKit" 即可包含公开头。
using UnrealBuildTool;

/// <summary>
/// 运行时模块规则。公开依赖只有引擎核心，Niagara 保持私有，游戏模块不必直接链接特效模块。
/// </summary>
public class WeaponKit : ModuleRules
{
	/// <summary>启用显式或共享预编译头，并登记公开与私有模块依赖。</summary>
	public WeaponKit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 游戏代码可以直接包含这些模块的类型：Actor、组件、数学和反射。
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
            "UMG",
            "SlateCore"
		});

		// Niagara 只在曳光实现里使用。公开头只前向声明，避免把特效依赖泄漏给游戏模块。
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Niagara",
            "Slate"
		});
	}
}
