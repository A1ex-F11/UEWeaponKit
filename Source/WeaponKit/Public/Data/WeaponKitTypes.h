/**
 * 武器运行时共用的枚举和结构：开火模式、散布、掉落 / 存档状态、伤害区域。
 * 这些类型不访问 World。运行中的弹药和散布累计放在 AWeaponKitRuntime 上，不要写回这里的配置。
 */
#pragma once

#include "CoreMinimal.h"
#include "Data/WeaponKitTypes.generated.h"

/** 一把枪允许切换的开火模式。默认模式和允许列表都配在 UWeaponKitDefinitionData 上。 */
UENUM(BlueprintType)
enum class EWeaponKitFireMode : uint8
{
	/** 每次扣动扳机只打一发，间隔由 FireInterval 决定。 */
	SemiAutomatic UMETA(DisplayName = "半自动"),
	/** 按住连续射击，相邻两发间隔仍是 FireInterval。 */
	FullAutomatic UMETA(DisplayName = "全自动"),
	/** 三连发。连发内间隔用 BurstShotInterval，一轮结束后等待 BurstRecoveryInterval。 */
	Burst UMETA(DisplayName = "三连发"),
};

/**
 * 腰射和瞄准共用的散布形状。连续射击散布也写在同一份配置里。
 * 瞄准是否改用另一份，由 UWeaponKitDefinitionData::bUseSeparateAimingSpread 决定。
 */
USTRUCT(BlueprintType)
struct WEAPONKIT_API FWeaponKitSpreadProfile
{
	GENERATED_BODY()

	/** 站立且水平速度不超过移动阈值时的锥角（度）。下蹲也使用这一项。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "静止散布角度", ClampMin = "0.0"))
	float StationarySpreadAngle = 0.0f;

	/** 水平速度超过移动阈值、且不是慢走或跳跃时的锥角（度）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "移动散布角度", ClampMin = "0.0"))
	float MovingSpreadAngle = 3.0f;

	/** 角色处于下落 / 跳跃时的锥角（度）。优先级高于移动、下蹲和慢走。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "跳跃散布角度", ClampMin = "0.0"))
	float JumpSpreadAngle = 12.0f;

	/** 慢走且速度已超过移动阈值时的锥角（度）。慢走标记由调用 GenerateShotDirection 的一方传入。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "慢走散布角度", ClampMin = "0.0"))
	float SlowWalkSpreadAngle = 1.0f;

	/** 水平速度（厘米/秒）超过该值才算移动散布。不超过则用静止锥角。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "移动散布速度阈值", ClampMin = "0.0"))
	float MovementSpreadSpeedThreshold = 10.0f;

	/** 为 true 时，连续射击会在基础锥角上再累加散布，并在停火后恢复。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "启用连续射击散布"))
	bool bUseFiringSpread = false;

	/** 每成功提交一发，连续射击散布增加的角度。只在 bUseFiringSpread 为 true 时生效。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "每发增加散布", ClampMin = "0.0"))
	float SpreadPerShot = 0.15f;

	/** 连续射击散布的上限（度）。基础锥角本身不受这一项限制。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "连续射击散布上限", ClampMin = "0.0"))
	float MaxFiringSpread = 0.8f;

	/** 停火后等待这么多秒才开始恢复连续射击散布。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "连续射击散布恢复延迟", ClampMin = "0.0"))
	float SpreadRecoveryDelay = 0.2f;

	/** 延迟结束后，连续射击散布每秒恢复的角度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "连续射击散布恢复速度", ClampMin = "0.0"))
	float SpreadRecoverySpeed = 2.0f;
};

/**
 * 掉落或存档时带走的运行时弹药和开火模式。
 * bHasSavedState 为 false 时，AWeaponKitRuntime::RestorePickupRuntimeState 会忽略整份状态，改用配置资产的初始值。
 */
USTRUCT(BlueprintType)
struct WEAPONKIT_API FWeaponKitPickupState
{
	GENERATED_BODY()

	/** 为 true 表示下面的弹药、开火模式和首抽标记有效。新生成的拾取物保持 false，直到被丢枪或读档写入。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponKit|Pickup")
	bool bHasSavedState = false;

	/** 弹匣内剩余子弹。恢复时会被钳到 0 和弹匣容量之间。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponKit|Pickup")
	int32 CurrentAmmo = 0;

	/** 备用弹药。恢复时小于 0 会收成 0。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponKit|Pickup")
	int32 ReserveAmmo = 0;

	/** 丢下或存档时的开火模式。恢复后不会被 PostInitialize 里的默认模式覆盖。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponKit|Pickup")
	EWeaponKitFireMode FireMode = EWeaponKitFireMode::SemiAutomatic;

	/** 这把枪的首抽动画是否已经播过。背包读档时会转交给 IWeaponKitCarrier。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponKit|Pickup")
	bool bInitialDrawConsumed = false;
};

/**
 * 一条伤害规则。先命中的区域生效。
 * 没有任何区域列出的骨骼使用倍率 1，并且吃身体护甲；因此骨骼名 "head" 本身没有特殊含义。
 */
USTRUCT(BlueprintType)
struct WEAPONKIT_API FWeaponKitDamageRegion
{
	GENERATED_BODY()

	/** 区域标识，仅供配置和调试辨认，结算时不读取它。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponKit|Damage")
	FName RegionId;

	/** 属于这个区域的骨骼名。命中骨骼在数组里即算命中该区域。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponKit|Damage")
	TArray<FName> BoneNames;

	/** 命中后乘到基础伤害上的倍率。非有限数按 0 处理。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponKit|Damage", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.0f;

	/** 为 true 且目标戴头盔时，最终伤害再乘穿甲参数。与身体护甲不会同时生效，头盔优先。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponKit|Damage")
	bool bMitigatedByHelmet = false;

	/** 为 true 且目标有身体护甲、并且头盔没有先生效时，最终伤害再乘穿甲参数。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponKit|Damage")
	bool bMitigatedByBodyArmor = false;
};
