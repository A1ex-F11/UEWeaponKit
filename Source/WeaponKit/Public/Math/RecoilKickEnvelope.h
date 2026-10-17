/**
 * 一发镜头冲量的时间包络。
 * 积分只跟已经过的时间有关，把一帧拆成多步不会改变总量。镜头后坐组件用它平滑施加剩余冲量。
 */
#pragma once

#include "CoreMinimal.h"

/**
 * 一发已被接受的镜头冲量。
 * Total 是整发冲量（俯仰 X、偏航 Y），Duration 是平滑时长，Elapsed / AppliedFraction 记录已经施加的部分。
 */
struct FWeaponKitRecoilKickEnvelope
{
	/** 这一发要施加的总冲量。X 为俯仰，Y 为偏航。 */
	FVector2D Total = FVector2D::ZeroVector;

	/** 平滑施加的总时长（秒）。0 表示调用方应立即施加，不走 Advance。 */
	float Duration = 0.0f;

	/** 已经推进的时间（秒），不会超过 Duration。 */
	float Elapsed = 0.0f;

	/** 已经施加的比例，范围 0 到 1。Advance 用它算出本步增量，避免重复施加。 */
	float AppliedFraction = 0.0f;

	/**
	 * 为 true 时，施加到控制器的增量会计入可恢复的累计角。
	 * 玩家手动转视角后会被改成 false，尾巴继续播完，但不再被自动恢复拽回去。
	 */
	bool bRecoverable = true;

	/**
	 * 推进 DeltaSeconds，返回这一步新施加的冲量。
	 * 曲线是 1 - (1 - T)^3，前段更快、末段变缓；同一时刻的累计值与步长划分无关。
	 */
	FVector2D Advance(float DeltaSeconds)
	{
		Elapsed = FMath::Min(Duration, Elapsed + FMath::Max(0.0f, DeltaSeconds));
		const float T = Duration > KINDA_SMALL_NUMBER ? Elapsed / Duration : 1.0f;
		const float Remaining = 1.0f - T;
		// 缓出立方：已施加比例只由归一化时间决定。
		const float Fraction = 1.0f - Remaining * Remaining * Remaining;
		const FVector2D Delta = Total * (Fraction - AppliedFraction);
		AppliedFraction = Fraction;
		return Delta;
	}

	/** 平滑时长已经走完。时长为 0 时 Elapsed >= Duration 立即成立。 */
	bool IsComplete() const
	{
		return Elapsed >= Duration;
	}

	/** 尚未施加的冲量。手动观察时用它确认尾巴还在，但不再参与恢复。 */
	FVector2D GetRemaining() const
	{
		return Total * (1.0f - AppliedFraction);
	}
};
