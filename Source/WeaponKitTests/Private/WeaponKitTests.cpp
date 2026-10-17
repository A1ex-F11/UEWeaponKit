// WeaponKitTests 模块实现。使用引擎默认模块实现，测试由自动化框架按名字发现。
#include "Modules/ModuleManager.h"

// 登记 UncookedOnly 测试模块。具体用例在同目录的其他 cpp 里，筛选前缀是 WeaponKit。
IMPLEMENT_MODULE(FDefaultModuleImpl, WeaponKitTests)
