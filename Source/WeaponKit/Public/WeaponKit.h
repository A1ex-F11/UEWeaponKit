/**
 * WeaponKit 运行时模块入口。
 * 模块本身不创建子系统。游戏在角色上挂背包、槽位、后坐和曳光组件，再调用公开接口。
 */
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** 插件模块。类型注册由 UHT 生成代码完成，这里没有额外的模块级服务。 */
class FWeaponKitModule : public IModuleInterface
{
public:
	/** 模块加载时调用。当前不注册控制台命令或子系统。 */
	virtual void StartupModule() override;

	/** 模块卸载时调用。当前没有模块级资源需要释放。 */
	virtual void ShutdownModule() override;
};
