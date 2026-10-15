/**
 * 一把枪的静态玩法配置：开火、弹药、散布、后坐和换弹。
 * 运行时弹药、当前模式和散布累计在 AWeaponKitRuntime 上，射击不会改这份共享资产。
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/WeaponKitTypes.h"
#include "Data/WeaponKitRecoilData.h"
#include "Data/WeaponKitDefinitionData.generated.h"

class UWeaponKitBallisticsData;

/**
 * 枪械玩法配置。
 * 在编辑器里创建 DataAsset，赋给 AWeaponKitRuntime::WeaponDefinition。没有绑配置时，武器只用代码里的默认值。
 */
UCLASS(BlueprintType, meta = (DisplayName = "枪械玩法配置"))
class WEAPONKIT_API UWeaponKitDefinitionData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 填入腰射 / 瞄准散布默认值，并把后坐、弹道创建为可在编辑器展开的内联子对象。 */
	UWeaponKitDefinitionData();

	/** 界面和武器展示用的名称。运行时用 GetWeaponDisplayName 读取，不在武器 Actor 上再存一份可编辑副本。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|基础", meta = (DisplayName = "武器展示名", ToolTip = "用于界面和武器展示的名称。"))
	FText WeaponDisplayName;

	/** 武器首次应用配置时使用的开火模式。应当出现在允许列表里。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|开火", meta = (DisplayName = "默认开火模式", ToolTip = "武器生成时使用的开火模式；必须包含在允许的开火模式中。"))
	EWeaponKitFireMode DefaultFireMode = EWeaponKitFireMode::SemiAutomatic;

	/** 玩家可以循环切换的开火模式。少于两项时 CycleFireMode 不会改变当前模式。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|开火", meta = (DisplayName = "允许的开火模式", ToolTip = "玩家可以在这把枪上切换的开火模式。"))
	TArray<EWeaponKitFireMode> AllowedFireModes = { EWeaponKitFireMode::SemiAutomatic };

	/** 两次普通射击之间的最短间隔（秒）。运行时至少按 0.001 秒使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|开火", meta = (DisplayName = "射击间隔", ClampMin = "0.0", ToolTip = "两次普通射击之间的最短间隔，单位为秒。"))
	float FireInterval = 0.15f;

	/** 三连发中相邻子弹的间隔（秒）。非三连发或设为 0 时，间隔退回 FireInterval。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|开火", meta = (DisplayName = "连发间隔", ClampMin = "0.0", ToolTip = "三连发中相邻子弹之间的间隔，单位为秒。非三连发武器通常设为0。"))
	float BurstShotInterval = 0.0f;

	/** 一轮三连发结束后，到下一轮连发之前的等待时间（秒）。由游戏在连发流程里读取。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|开火", meta = (DisplayName = "连发恢复间隔", ClampMin = "0.0", ToolTip = "三连发结束后允许下一轮连发前等待的时间，单位为秒。"))
	float BurstRecoveryInterval = 0.3f;

	/** 弹匣可容纳的最大子弹数。运行时至少为 1。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|弹药", meta = (DisplayName = "弹匣容量", ClampMin = "1", ToolTip = "弹匣可容纳的最大子弹数。"))
	int32 MagazineCapacity = 30;

	/** 新武器第一次应用配置时装入弹匣的子弹数。读档或丢枪恢复后的当前弹药不使用这项。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|弹药", meta = (DisplayName = "初始装弹数", ClampMin = "0", ToolTip = "新武器首次生成时装入弹匣的子弹数；不是运行中的当前弹药。"))
	int32 InitialMagazineAmmo = 30;

	/** 新武器第一次应用配置时的备用子弹。不是运行中的当前备弹。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|弹药", meta = (DisplayName = "初始备弹数", ClampMin = "0", ToolTip = "新武器首次生成时携带的备用子弹数；不是运行中的当前备弹。"))
	int32 InitialReserveAmmo = 90;

	/** 单发伤害、穿甲、爆头倍率和射线距离。构造时创建为内联子对象。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "枪械配置|伤害与弹道", meta = (DisplayName = "伤害与弹道参数", ToolTip = "单发伤害、护甲穿透、爆头倍率和射线距离。"))
	TObjectPtr<UWeaponKitBallisticsData> BallisticsData;

	/** 腰射散布。未开启独立瞄准散布时，瞄准也使用这一份。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|腰射散布", meta = (DisplayName = "腰射散布"))
	FWeaponKitSpreadProfile HipFire;

	/** 为 true 时右键瞄准使用 Aiming；为 false 时瞄准沿用腰射散布。两套连续射击散布仍然分开累计。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|瞄准散布", meta = (DisplayName = "使用独立瞄准散布", ToolTip = "开启后右键使用瞄准散布；关闭时瞄准沿用腰射散布。"))
	bool bUseSeparateAimingSpread = false;

	/** 右键瞄准散布。仅当 bUseSeparateAimingSpread 为 true 时参与计算。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|瞄准散布", meta = (DisplayName = "右键瞄准散布", EditCondition = "bUseSeparateAimingSpread"))
	FWeaponKitSpreadProfile Aiming;

	/** 后坐序列、单发冲量和镜头反馈。构造时创建为内联子对象。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "枪械配置|后坐", meta = (DisplayName = "后坐参数", ToolTip = "后坐序列、单发冲量、镜头反馈和平滑恢复。"))
	TObjectPtr<UWeaponKitRecoilData> RecoilData;

	/** 为 true 时，换弹开始就固定本次预计装入的数量，途中备弹变化不会改这次计划。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|换弹", meta = (DisplayName = "开始换弹时锁定装弹数量", ToolTip = "换弹开始时固定本次预计装入的子弹数，避免换弹途中备用弹变化导致数量改变。"))
	bool bLockReloadAmountAtStart = false;

	/** 为 true 时由游戏按通知逐发调用 TryCommitReloadRound。整匣换弹保持 false。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|换弹", meta = (DisplayName = "逐颗装入弹药", ToolTip = "开启后按每发换弹通知逐颗装入弹药。"))
	bool bReloadOneRoundAtATime = false;

	/** 逐发换弹时，若弹匣已空且备弹足以装满，则改成一次装入全部计划弹药。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|换弹", meta = (DisplayName = "满弹匣空仓换弹一次装入", ToolTip = "开启逐颗装弹时，若空仓且备用弹足以装满弹匣，则允许一次装入全部计划弹药。"))
	bool bUseBatchReloadForFullEmptyReload = false;

	/**
	 * 按当前是否瞄准返回要用的散布配置。
	 * bAiming 为 true 且 bUseSeparateAimingSpread 为 true 时返回 Aiming，否则返回 HipFire。
	 */
	const FWeaponKitSpreadProfile& GetSpreadProfile(bool bAiming) const;
};
