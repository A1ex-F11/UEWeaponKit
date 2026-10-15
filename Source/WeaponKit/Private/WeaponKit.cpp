// WeaponKit 模块实现。启动和关闭都是空操作，武器逻辑在各自的类里。
#include "WeaponKit.h"

// 模块装载：不注册额外回调。
void FWeaponKitModule::StartupModule()
{
}

// 模块卸载：没有模块级资源需要释放。
void FWeaponKitModule::ShutdownModule()
{
}

// 把 FWeaponKitModule 登记为名为 WeaponKit 的引擎模块，名字必须和 .uplugin 里的模块名一致。
IMPLEMENT_MODULE(FWeaponKitModule, WeaponKit)
