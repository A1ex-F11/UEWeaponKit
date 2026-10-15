/**
 * 连续射击散布的无状态计算。
 * 每把武器自己保存累计值，配置资产保持只读。开火提交时调用 AddShot，采样方向前调用 Recover。
 */
#pragma once

#include "CoreMinimal.h"

/**
 * 连续射击散布数学。
 * 非法或 NaN 输入收成 0，结果不为负，也不超过调用方给出的上限。
 */
struct FWeaponKitFiringSpreadMath
{
	/** 非有限数收成 0，负数钳到 0。散布角度不允许为负。 */
	static float Safe(float Value)
	{
		return FMath::IsFinite(Value) ? FMath::Max(0.0f, Value) : 0.0f;
	}

	/**
	 * 停火后的散布恢复。
	 * Elapsed 不超过 Delay 时保持原值；超过后按 Speed（度/秒）线性下降，并夹在 0 与 Maximum 之间。
	 */
	static float Recover(float Current, float Elapsed, float Delay, float Speed, float Maximum)
	{
		// 可恢复时间 = max(0, 已停火时间 - 延迟)，再乘恢复速度。
		return FMath::Max(0.0f, FMath::Min(Safe(Current), Safe(Maximum)) -
			FMath::Max(0.0f, Safe(Elapsed) - Safe(Delay)) * Safe(Speed));
	}

	/**
	 * 成功开火后增加散布。
	 * 任一参数不是有限数时返回 0，避免 NaN 继续扩散；否则累加后不超过 Maximum。
	 */
	static float AddShot(float Current, float PerShot, float Maximum)
	{
		if (!FMath::IsFinite(Current) || !FMath::IsFinite(PerShot) || !FMath::IsFinite(Maximum))
		{
			return 0.0f;
		}
		return FMath::Min(Safe(Maximum), Safe(Current) + Safe(PerShot));
	}
};
