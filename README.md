# WeaponKit

从 Dust FPS 演示里抽出来的 **UE 5.8 运行时插件**。它只依赖引擎模块（Core、CoreUObject、Engine、准星用的 UMG/Slate，以及曳光用的 Niagara），不包含 Dust2 角色、表现层、关卡或任务代码。拷贝到别的工程后，用自己的角色调用公开接口即可。

反射类型统一加了 `WeaponKit` 前缀，导出宏是 `WEAPONKIT_API`。这样插件可以和 Dust2 放在同一个工程里，而不会和原来的 `AWeaponRuntime`、`UWeaponDefinitionData` 等同名类冲突。

## 2026-10-17：统一配置与可复用准星

本仓库收录的是**配置类、结构和接口代码**，不包含 Dust2 五把枪的实际数值、JSON、`.uasset`、模型、声音或关卡。代码里的构造默认值和自动化测试夹具仅用于类型默认行为与验证，不代表某把枪的平衡配置。

- `UWeaponKitDefinitionData`：开火模式列表首项作为初始模式；装填方式用 `EWeaponKitReloadStyle`。弹匣容量与初始装弹数仍独立。
- `FWeaponKitIntuitiveAccuracy`：常调基础散布、射击累计上限、参考发数和恢复秒数；引用 `UWeaponKitAccuracyPreset` 提供姿态惩罚，可以单枪覆盖。
- `UWeaponKitCameraFeedbackPreset`：共享镜头回落/平滑/立即施加比例；每枪保留反馈强度和独立覆盖。
- 新建持久资产直接使用统一配置；旧包在 `PostLoad` 迁移，旧 transient C++ 配置仍可按旧字段运行。旧射击节奏精确保留至首次修改四项精度字段；只改姿态不清旧射击节奏。
- `AWeaponKitRuntime::GetCurrentTotalSpreadAngle` 与实际射击共用同一份姿态/累计解析，可以给调用方的准星投影使用。
- `UWeaponKitCrosshairStyleData`：长度、粗细、中心间距、颜色/透明度、动态扩散开关、显示强度。
- `UWeaponKitCrosshairWidget`：四线绘制，实例样式覆盖优先于资产；不认识 Dust2 角色、武器槽位和关卡。
- `FWeaponKitCrosshairDisplayTransitionState`：纯数学的跟随/限时平滑；调用方把实际拔枪剩余时间传给 `ConstrainRemainingTime`，不依赖本仓库的动画系统。

### 在其他项目接准星

1. 建一个继承 `WeaponKitCrosshairWidget` 的 Widget Blueprint，按游戏需要放入HUD。
2. 创建 `WeaponKitCrosshairStyleData` 数据资产，设置默认外观，在控件上选择它。
3. 游戏侧根据武器总散布和相机视野得到投影间距。先调用 `ResolveStyleTargetGap`，再把结果送进平滑过渡，最后调用 `SetCrosshairGap`。显示强度只在目标解析处应用一次。
4. 切枪/拔枪开始时调用纯数学状态的过渡接口。瞄准隐藏、暂停、死亡与HUD显隐由宿主负责。
5. 玩家设置菜单调用 `ApplyStyleOverride` / `ClearStyleOverride`；用自己的 SaveGame 或用户设置保存玩家选项，不能写共享DataAsset。

关闭动态扩散只固定准星间距，不改变弹道。样式显示强度与枪械真实精度无关。`SetCrosshairGap` 接收已经解析/平滑后的显示间距，不会再次放大。

头文件入口：`Data/WeaponKitCrosshairStyleData.h`、`UI/WeaponKitCrosshairWidget.h`、`Math/WeaponKitCrosshairDisplayMath.h`。新的UI公开头依赖UMG，模块构建规则已声明相关引擎依赖。

### 本次兼容修正

早期仓库尚未经过UHT。本次首次独立编译修正了所有 `.generated.h` 的目录前缀，并把拾取接口改名为 `UWeaponKitPickupInterface` / `IWeaponKitPickupInterface`，避免与 `AWeaponKitPickup` 共享反射名。默认拾取物另补根组件，确保原生 Actor 能移动且距离判定读取实际位置。手写实现该旧接口的代码须采用新接口名。Actor名称不变。

命中/击杀声音、临时准星变化以及玩家设置菜单尚未包含在插件里；它们是 Dust2 W12 的后续计划，不冒充本次已实现功能。

## 仓库布局

本仓库的根目录就是插件本体，没有再套一层 `Plugins/WeaponKit`。根上是 `WeaponKit.uplugin`、`Source/` 和这份中文说明。把它放进任意 UE 5.8 工程时，目录名用 `WeaponKit`，让描述文件落在 `YourProject/Plugins/WeaponKit/WeaponKit.uplugin`：

```bash
cd YourProject
mkdir -p Plugins
git clone https://github.com/A1ex-F11/UEWeaponKit.git Plugins/WeaponKit
```

也可以把本仓库根目录里的文件复制到已经建好的 `YourProject/Plugins/WeaponKit/`。不要把克隆出来的 `UEWeaponKit` 文件夹原样嵌进 `Plugins/`，否则描述文件会停在 `Plugins/UEWeaponKit/WeaponKit.uplugin`，文件夹名和插件名不一致。

源码从 [Dust](https://github.com/A1ex-F11/Dust) 分支 `cursor/weapon-kit-plugin-2152` 的 `Plugins/WeaponKit` 原样搬出（[PR #1](https://github.com/A1ex-F11/Dust/pull/1)）。`.gitignore` 忽略插件编译时生成的 `Binaries/` 和 `Intermediate/`。

> **2026-10-17 已独立验证。** 在全新 UE 5.8 C++ 宿主中编译通过，不依赖 Dust2 模块或资产；`WeaponKit.*` 自动化测试 28/28 成功。此前“尚未编译”是历史状态。本次包含真实编译发现的 generated 头引用及拾取接口反射重名修复。尚未替用户的目标项目验证完整玩法或打包。


## 目录

| 路径 | 作用 |
| --- | --- |
| `Source/WeaponKit` | 运行时模块。游戏打包会带上它。 |
| `Source/WeaponKit/Public/Math` | 无状态数学：连续射击散布、弹道后坐、镜头冲量包络、曳光线段。 |
| `Source/WeaponKit/Public/Data` | 开火模式、散布配置、枪械 / 后坐 / 弹道 DataAsset、存档和伤害区域结构。 |
| `Source/WeaponKit/Public/Damage` | 按「骨骼 → 区域 → 倍率」结算伤害。 |
| `Source/WeaponKit/Public/Tracer` | 射击后播放 Niagara 曳光。 |
| `Source/WeaponKit/Public/Recoil` | 镜头 / 控制器后坐。 |
| `Source/WeaponKit/Public/Runtime` | 武器 Actor、拾取物、交互扫描、Child Actor 槽位实现。 |
| `Source/WeaponKit/Public/Inventory` | 可变槽位背包。 |
| `Source/WeaponKit/Public/Interfaces` | 携带者、槽位宿主、拾取物三个接口。 |
| `Source/WeaponKitTests` | 自动化测试模块，类型是 `UncookedOnly`，不会打进正式包。 |

`WeaponKit.Build.cs` 里 Niagara 是私有依赖：公开头文件只前向声明 Niagara 类型，真正刷参数的代码在 cpp 里。

## 模块文件一览

头文件里的类型、函数和 `UPROPERTY` 用 `/** */` 说明用途和调用方式。`.cpp` 里只在关键分支加简短行注释。`WeaponKit.uplugin` 是 JSON，不能写注释。

### WeaponKit

| 文件 | 作用 |
| --- | --- |
| `WeaponKit.uplugin` | 插件描述：运行时模块加 UncookedOnly 测试模块，默认关闭，依赖 Niagara。 |
| `.gitignore` | 忽略插件编译生成的 `Binaries/`、`Intermediate/` 以及本地工程文件。 |
| `Source/WeaponKit/WeaponKit.Build.cs` | 运行时模块构建规则。公开依赖引擎核心，Niagara 只给实现文件用。 |
| `Source/WeaponKit/Public/WeaponKit.h` | 模块接口。启动和关闭目前不做额外注册。 |
| `Source/WeaponKit/Private/WeaponKit.cpp` | 把该模块登记为 WeaponKit。 |
| `Source/WeaponKit/Public/Math/FiringSpreadMath.h` | 连续射击散布的增加、上限和延迟恢复。 |
| `Source/WeaponKit/Public/Math/ShotRecoilMath.h` | 弹道后坐的停火恢复，以及把俯仰 / 偏航加到射击方向上。 |
| `Source/WeaponKit/Public/Math/RecoilKickEnvelope.h` | 一发镜头冲量的平滑包络，拆帧不改变总量。 |
| `Source/WeaponKit/Public/Math/ShotTracerGeometry.h` | 曳光飞行时间和线段，不做伤害射线。 |
| `Source/WeaponKit/Public/Data/WeaponKitTypes.h` | 开火模式、散布配置、拾取存档和伤害区域。 |
| `Source/WeaponKit/Public/Data/WeaponKitBallisticsData.h` | 单发伤害、穿甲、爆头倍率和射线长度。 |
| `Source/WeaponKit/Public/Data/WeaponKitRecoilData.h` | 固定后坐序列、随机冲量、镜头反馈和恢复。 |
| `Source/WeaponKit/Private/Data/WeaponKitRecoilData.cpp` | 后坐资产的构造默认值。 |
| `Source/WeaponKit/Public/Data/WeaponKitDefinitionData.h` | 一把枪的静态玩法配置。 |
| `Source/WeaponKit/Private/Data/WeaponKitDefinitionData.cpp` | 创建内联后坐和弹道，并选择腰射或瞄准散布。 |
| `Source/WeaponKit/Public/Damage/WeaponKitDamageLibrary.h` | 按骨骼区域结算伤害的蓝图函数。 |
| `Source/WeaponKit/Private/Damage/WeaponKitDamageLibrary.cpp` | 区域查找、头盔优先于身体护甲，以及头 / 身体区域表。 |
| `Source/WeaponKit/Public/Interfaces/WeaponKitInterfaces.h` | 携带者、槽位宿主、拾取物三个接口。 |
| `Source/WeaponKit/Private/Interfaces/WeaponKitInterfaces.cpp` | 三个接口未覆盖时的默认回退。 |
| `Source/WeaponKit/Public/Runtime/WeaponKitRuntime.h` | 武器 Actor，以及槽位存档和背包快照。 |
| `Source/WeaponKit/Private/Runtime/WeaponKitRuntime.cpp` | 开火间隔、换弹、散布采样和后坐序列。 |
| `Source/WeaponKit/Public/Runtime/WeaponKitChildActorSlot.h` | 用 Child Actor 实现的默认武器槽。 |
| `Source/WeaponKit/Private/Runtime/WeaponKitChildActorSlot.cpp` | 生成和清空槽里的武器。 |
| `Source/WeaponKit/Public/Runtime/WeaponKitPickup.h` | 默认可拾取武器，自动拾取体积由调用方指定。 |
| `Source/WeaponKit/Private/Runtime/WeaponKitPickup.cpp` | 占用、丢枪冷却，以及自动拾取的重叠判断。 |
| `Source/WeaponKit/Public/Runtime/WeaponKitInteractionScanner.h` | 登记附近拾取物，并按距离和视线选择目标。 |
| `Source/WeaponKit/Private/Runtime/WeaponKitInteractionScanner.cpp` | 视线优先、否则取最近可见目标，并用定时器刷新瞄准。 |
| `Source/WeaponKit/Public/Recoil/WeaponKitCameraRecoilComponent.h` | 把冲量写入控制旋转，或只偏移摄像机。 |
| `Source/WeaponKit/Private/Recoil/WeaponKitCameraRecoilComponent.cpp` | 瞄准写视角，分离模式只做镜头反馈，并处理恢复。 |
| `Source/WeaponKit/Public/Tracer/WeaponKitShotTracerComponent.h` | Niagara 曳光的速度、尺寸和可改参数名。 |
| `Source/WeaponKit/Private/Tracer/WeaponKitShotTracerComponent.cpp` | 生成曳光、写入用户参数，并限制同时存在的数量。 |
| `Source/WeaponKit/Public/Inventory/WeaponKitInventoryComponent.h` | 可变槽位背包的拾取、切枪、丢枪和读档接口。 |
| `Source/WeaponKit/Private/Inventory/WeaponKitInventoryComponent.cpp` | 槽位校验、替换当前武器，以及丢枪后的自动补拾取。 |

### WeaponKitTests

| 文件 | 作用 |
| --- | --- |
| `Source/WeaponKitTests/WeaponKitTests.Build.cs` | 测试模块构建规则，私有依赖 WeaponKit，不打进正式包。 |
| `Source/WeaponKitTests/Private/WeaponKitTests.cpp` | 登记测试模块，用例由自动化框架按 WeaponKit 前缀发现。 |
| `Source/WeaponKitTests/Private/WeaponKitTestWorld.h` | 为运行时和背包测试创建临时 Game 世界。 |
| `Source/WeaponKitTests/Private/WeaponKitTestCarrier.h` | 实现携带者接口的测试 Pawn，可开关死亡和首抽标记。 |
| `Source/WeaponKitTests/Private/WeaponKitMathTests.cpp` | 散布、弹道后坐、冲量包络、曳光几何和伤害区域测试。 |
| `Source/WeaponKitTests/Private/WeaponKitRuntimeTests.cpp` | 初始配置、连续射击散布，以及瞄准时忽略弹道后坐。 |
| `Source/WeaponKitTests/Private/WeaponKitInventoryTests.cpp` | 镜头后坐写入、背包读档切换丢弃，以及扫描器筛选。 |


## 装进任意 UE 5.8 工程

1. 按上面的「仓库布局」，把本仓库放到目标工程的 `Plugins/WeaponKit`（克隆时把目录名写成 `WeaponKit`，或复制根目录里的文件）。如果该工程的 `.gitignore` 忽略了 `Plugins/`，要单独放行这个目录，否则源码不会进版本库。
2. 编辑器里打开 **Edit → Plugins**，启用 **Weapon Kit**。它声明了对 Niagara 的依赖，启用时 Niagara 也会打开。也可以在 `.uproject` 里写：

```json
{
  "Name": "WeaponKit",
  "Enabled": true
}
```

3. 在使用这些类的游戏模块 `Build.cs` 里增加依赖：

```csharp
PrivateDependencyModuleNames.Add("WeaponKit");
```

4. 重新生成工程文件并编译。不要再依赖 Dust2。角色、动画、准星的武器投影/显隐接入和换弹表现留在游戏模块里，由游戏在合适的时机调用下面的接口。

插件描述里 `EnabledByDefault` 是 `false`。放进 Dust 工程时，不打开它就不会参与编译，原来的 Dust2 模块保持原样。

### 游戏侧最少要接的几件事

- 给角色加 `UWeaponKitInventoryComponent`、若干 `UWeaponKitChildActorSlot`（或自己的 `IWeaponKitSlotHost`）、一个 `UWeaponKitCameraRecoilComponent`。
- 右键瞄准时，由角色自己调用 `AWeaponKitRuntime::SetAiming` 和 `UWeaponKitCameraRecoilComponent::SetAiming`。插件不再认识具体的玩家类。
- 需要死亡、首抽是否播过，就让 Pawn 实现 `IWeaponKitCarrier`。不实现时，插件把携带者当成存活，并且没有「首抽已消耗」的记录。
- 拾取扫描用 `UWeaponKitInteractionScanner`。玩家按下拾取键时，把扫描结果传给 `TryPickup`。背包不会自己去角色身上找「当前瞄准的那把枪」。
- 自动拾取的重叠体用 `AWeaponKitPickup::SetAutoPickupVolume` 指过去。没有按组件名字搜索。

## 各部分做什么

### 数学

数学头文件是纯函数或很小的状态结构，不碰 World，也不读资产。

- `FWeaponKitFiringSpreadMath`：连续射击散布的增加和延迟恢复。非法数值会收成 0。
- `FWeaponKitShotRecoilMath`：把累计的俯仰 / 偏航加到射击方向上，并按停火时间收回。瞄准时武器不会把这份偏移再加一次，下一发用的就是调用方给的视线。
- `FWeaponKitRecoilKickEnvelope`：一发镜头冲量。积分只跟已经过的时间有关，把一帧拆成多步不会改变总量。
- `FWeaponKitShotTracerGeometry`：只算曳光的飞行时间和线段，不做伤害射线。

### 数据资产

`UWeaponKitDefinitionData` 是一把枪的静态配置。运行时弹药、当前开火模式、散布累计都在 `AWeaponKitRuntime` 上，打出去的子弹不会改共享资产。

当前腰射和瞄准编辑 `HipAccuracy` / `AimAccuracy`，由四项精度与姿态预设派生 `FWeaponKitSpreadProfile`；`bUseAimingSpread` 为 false 时沿用腰射。`bUseSeparateAimingSpread` 与旧原始锥角仅保留兼容迁移。统一模式按基础＋水平移动或静步＋空中＋射击累计计算；旧模式仍保留旧蹲姿选择规则，新模式不提供蹲姿精度奖励。

`UWeaponKitRecoilData`、`UWeaponKitBallisticsData` 在配置资产的构造函数里作为内联子对象创建，并标成 `RF_Public`，方便在编辑器里展开。弹道资产上的爆头倍率只是一个数；哪根骨骼算头，由伤害区域表决定。

`FWeaponKitLoadoutSave` 用一个数组描述每个槽位的武器类和 `FWeaponKitPickupState`，再加上当前槽位下标。

### 武器运行时

`AWeaponKitRuntime` 负责开火间隔、弹药、换弹（整匣或逐发）、开火模式循环、后坐序列和散布采样。静态数从 DataAsset 读。没有绑配置时，只用构造函数里的代码默认值，不再保留一套「给旧蓝图填的」可编辑字段，也不再通过属性名把弹道对象写回蓝图子类。

瞄准是公开的 `SetAiming(bool)`。瞄准期间提交的那一发会记下「不要再叠加弹道后坐」，松开之后这一发的采样结果不变。连续射击散布在腰射和瞄准两套配置上分别累计，来回切换不会把另一套清零。

### 镜头后坐

`UWeaponKitCameraRecoilComponent::ApplyRecoil` 是唯一的施加入口。

- 瞄准：按 `AimingRecoilScale` 写入控制旋转，下一发采样到的就是同一条视线。
- 腰射且开启「射击后坐与镜头分离」：只给摄像机加偏移，不改控制旋转。
- 其他腰射：走控制器输入。随机后坐仍受组件上的俯仰 / 偏航累计上限约束。

`SetAiming` 在瞄准状态真正发生变化时会清掉还没施加完的尾巴，已经写进视角的角度保持不动。`NotifyManualLook` 只取消自动恢复，不把视角拽回去。

### 曳光

`UWeaponKitShotTracerComponent::PlayShotTracer` 在伤害射线成功之后调用。距离、寿命、长度、宽度、颜色对应的 Niagara User 参数都是可编辑的 `FName`，默认仍是 `User.TracerDistance`、`User.TracerLifetime`、`User.TracerLength`、`User.TracerWidth`、`User.TracerColor`。旧特效不用改名；换了参数名的特效改组件上的字段即可。同时存在的曳光数量也有上限，默认 64。

插件里没有 Dust 工程的特效资产路径。几何测试不加载 `/Game/...` 资源。

### 背包、槽位、拾取、扫描

- `IWeaponKitSlotHost`：一个槽位如何生成和清空武器。`UWeaponKitChildActorSlot` 是用 Child Actor 做的默认实现，背包不依赖这个具体类。
- `UWeaponKitInventoryComponent`：槽位个数等于初始化时传入的宿主数量。切换会找下一个有枪的槽。身上至少还留一把枪时才允许丢弃当前武器。
- `IWeaponKitPickupInterface` / `AWeaponKitPickup`：背包只认接口。丢枪时用 `PickupClassByWeaponClass` 里登记的 Actor 类生成，这个类必须实现接口。
- `UWeaponKitInteractionScanner`：登记附近的拾取物，按距离、视线和存活状态选出手动或自动目标，并用定时器刷新「当前瞄准的拾取物」。自动拾取要求接口返回的重叠体正在和 Pawn 重叠。
- `IWeaponKitCarrier`：可选。用来回答「还活着吗」和「这把枪的首抽动画是否已经播过」。

丢枪成功之后，如果背包上指定了扫描器，会再问一次自动拾取候选，用来补上原来「丢掉一把后立刻捡起脚边另一把」的行为。扫描逻辑不写在背包里面。

### 伤害

`UWeaponKitDamageLibrary::CalculateFinalDamage` 的入参是一组 `FWeaponKitDamageRegion`。区域列出骨骼名、倍率，以及是否吃头盔、是否吃护甲。先命中的区域生效。没有任何区域列出的骨骼，倍率是 1，并且吃身体护甲。因此骨骼名 `"head"` 本身没有特殊含义。

`MakeHeadBodyRegions` 用来按调用方给出的头部骨骼名拼出和旧算法相同的一张表：头部乘爆头倍率，有头盔再乘穿甲；其余骨骼有护甲时乘穿甲。

## 解耦：之前和之后

Dust 里这些类型当时直接服务于 `ADust2PlayerCharacter` 和两格背包。插件里的对应物去掉了这种绑定。Dust2 源码没有改，下面的「之前」描述的是抽出时的写法。

| 点 | 之前 | 之后 |
| --- | --- | --- |
| 瞄准 | `AWeaponRuntime` 和镜头后坐组件把 `ADust2PlayerCharacter` 设为 `friend`，由角色调用私有的 `SetOwnerAimState` / `SetAimingRecoilMode`。 | 两个 `SetAiming(bool)` 都是公开 API。角色、AI 或测试代码都可以调用。 |
| 腰射 / 瞄准散布 | 腰射是配置资产上的一排散落字段，瞄准才是 `FWeaponSpreadProfile`，另外还有只序列化、不再参与计算的 `bIgnoreShotRecoilWhenAiming`。 | 统一四项精度与姿态预设；运行时派生 `FWeaponKitSpreadProfile`。瞄准是否使用独立配置由 `bUseAimingSpread` 决定。瞄准时忽略弹道后坐是运行时规则，不再靠遗留布尔值。 |
| 蓝图兜底 | 武器 Actor 上有一套 `EditCondition = WeaponDefinition == nullptr` 的字段；弹道和连发恢复间隔用属性名反射写回蓝图。`CaptureDefinition` 和 `SyncLegacyWeaponReferences` 是迁移 / 旧变量镜像。 | 这些入口都没有搬进来。静态数据只住在 DataAsset 里。展示名、弹道、连发恢复间隔通过读取函数拿。 |
| 镜头后坐入口 | `ApplyRecoilKick` 是不带数据资产的兼容入口，`ApplyRecoilKickWithData` 再分流到旧路径。 | 只保留 `ApplyRecoil`。随机后坐的累计上限仍是组件上的可配置值，而不是一条单独的兼容函数。 |
| 背包槽位 | 写死 `Slot1` / `Slot2` 两个 `UChildActorComponent`。 | `TArray` 槽位，元素是 `IWeaponKitSlotHost`。 |
| 存档 | `RestoreCheckpointWeapons` 有 13 个参数（两个槽的类、弹药、开火模式、首抽标记，再加谁是当前武器）。 | `RestoreLoadout(槽位, FWeaponKitLoadoutSave)`。 |
| 拾取 | 背包直接依赖 `AWeaponPickupActor`，并 `Cast` 到 `ADust2PlayerCharacter` 查死亡、附近武器和首抽。 | 背包只调用 `IWeaponKitPickupInterface` 和可选的 `IWeaponKitCarrier`。 |
| 交互 | 自动拾取体积靠组件名 `AutoPickupCollision` 查找；附近武器和瞄准武器的射线写在角色里。 | 体积由 `SetAutoPickupVolume` 指定。`UWeaponKitInteractionScanner` 单独负责登记、距离、视线和瞄准回调。 |
| 伤害 | `UDamageLibrary` 写死骨骼名 `"head"`。 | 骨骼到区域的映射是数据。不在表里的骨骼走身体规则。 |
| 曳光 | Niagara 参数名写在代码里；编辑器测试还加载 Dust 的特效资产。 | 参数名是组件属性。插件测试只检查几何和一个未配置组件的失败路径。 |

没有搬进插件的部分：`WeaponPresentationComponent`、武器数据迁移和特效搭建脚本、GameMode、任务流程，以及玩家角色本身。角色上的瞄准输入、换弹表现和血量仍然属于游戏。

## 自动化测试

当前另含配置迁移、新建资产首次编辑、共享预设、散布同源及准星样式/过渡用例，以独立宿主实际测试结果为准。

启用插件后，在编辑器 **Session Frontend → Automation** 里筛选 `WeaponKit`。测试名前缀从原来的 `Dust2.*` 改成了 `WeaponKit.*`。

| 测试 | 覆盖 |
| --- | --- |
| `WeaponKit.Spread.MathBoundaries` | 散布加减和恢复的边界、NaN。 |
| `WeaponKit.Recoil.ShotMath` | 弹道后坐的延迟恢复和方向叠加。 |
| `WeaponKit.Recoil.KickEnvelopeIntegral` | 冲量包络拆帧后总量不变。 |
| `WeaponKit.Tracer.FlightAndSegmentBounds` | 曳光飞行时间、线段不越出枪口和落点；参数名可改。 |
| `WeaponKit.Damage.RegionMultipliers` | 旧的头 / 身体数值，以及 `"head"` 未列入区域时不再爆头。 |
| `WeaponKit.Runtime.InitialConfigAndRestore` | 初始弹药来自资产，读档不被 `PostInitialize` 覆盖，共享资产不被实例改写。 |
| `WeaponKit.Runtime.ContinuousFireSpread` | 连续射击散布的增长、上限和恢复。 |
| `WeaponKit.Runtime.AimSpreadSnapshotAndBloom` | 已提交的那一发保持提交时的锥角；两套散布分开累计。 |
| `WeaponKit.Runtime.AimIgnoresAccumulatedShotRecoil` | 瞄准提交的射击跟着输入方向走，镜头冲量仍然会生成。 |
| `WeaponKit.Recoil.AimViewWriter` | 瞄准写入控制旋转并乘瞄准倍率；腰射分离模式不写控制旋转。 |
| `WeaponKit.Recoil.TailCleanup` | 退出瞄准取消未完成的尾巴；手动观察不会被恢复逻辑拽回去。 |
| `WeaponKit.Inventory.LoadoutSwitchAndDrop` | 三个槽、一份存档结构、切换、丢弃、再通过接口捡回。 |
| `WeaponKit.Scanner.ManualPickupWithoutNameLookup` | 不实现接口的 Actor 被拒绝；距离和死亡生效；没有指定体积时不会自动拾取。 |

原来依赖 `ADust2PlayerCharacter` 和 `UWeaponPresentationComponent` 的瞄准输入、换弹取消瞄准、死亡清瞄准，没有搬进来。那些规则留在角色上。Dust 曳光资产的模拟测试也没有搬，因为它加载的是工程内容而不是插件代码。

## 验证边界

2026-10-17：独立空白宿主真实 Editor 构建成功，`WeaponKit.*` 共 28 项成功、0失败。验证覆盖该宿主中的原生接口、配置迁移、散布/后坐、预设共享和准星样式。未验证所有 UE 版本、目标项目资源、PIE观感或打包；不能把原生测试通过称为任意项目开箱即用。
