/**
 * 曳光的飞行时间和线段几何。只服务视觉，不做伤害射线。
 * UWeaponKitShotTracerComponent 在伤害检测成功后调用这里，再把结果写入 Niagara。
 */
#pragma once

#include "CoreMinimal.h"

/**
 * 曳光线段计算。
 * BuildFlight 决定特效参数；Segment 把飞行进度变成线段中心和长度，供测试或自定义绘制使用。
 */
struct FWeaponKitShotTracerGeometry
{
	/**
	 * 由枪口、落点和视觉速度算出曳光参数。
	 * 成功时 OutDistance 为厘米距离，OutDuration 夹在 1/60 秒和 MaxDuration（再夹到 1 秒）之间，OutLength 不超过距离。
	 * 端点非法、速度或长度非正、距离过近或超过 1e7 厘米时失败，并把三个输出清零。
	 */
	static bool BuildFlight(
		const FVector& Start,
		const FVector& End,
		float Speed,
		float MaxDuration,
		float Length,
		float& OutDistance,
		float& OutDuration,
		float& OutLength)
	{
		OutDistance = OutDuration = OutLength = 0.0f;
		if (Start.ContainsNaN() || End.ContainsNaN() || !FMath::IsFinite(Speed) ||
			!FMath::IsFinite(MaxDuration) || !FMath::IsFinite(Length) ||
			Speed <= 0.0f || MaxDuration <= 0.0f || Length <= 0.0f)
		{
			return false;
		}
		const double Distance = FVector::Distance(Start, End);
		if (!FMath::IsFinite(Distance) || Distance <= KINDA_SMALL_NUMBER || Distance > 1.0e7)
		{
			return false;
		}
		OutDistance = static_cast<float>(Distance);
		// 视觉寿命有下限，避免一帧内消失；也有上限，避免超远距离拖太久。
		OutDuration = FMath::Clamp(
			OutDistance / Speed,
			1.0f / 60.0f,
			FMath::Clamp(MaxDuration, 1.0f / 60.0f, 1.0f));
		OutLength = FMath::Min(Length, OutDistance);
		return true;
	}

	/**
	 * 给定飞行年龄，返回亮线的中心距离和可见长度（X = 中点沿弹道的距离，Y = 线段长度）。
	 * 头不会越过落点，尾不会退到枪口之前。参数非法时返回零。
	 */
	static FVector2D Segment(float Distance, float Duration, float Length, float Age)
	{
		if (!FMath::IsFinite(Distance) || !FMath::IsFinite(Duration) ||
			!FMath::IsFinite(Length) || !FMath::IsFinite(Age) ||
			Distance <= 0.0f || Duration <= 0.0f || Length <= 0.0f)
		{
			return FVector2D::ZeroVector;
		}
		const float Head = Distance * FMath::Clamp(Age / Duration, 0.0f, 1.0f);
		const float Tail = FMath::Max(0.0f, Head - Length);
		return FVector2D((Head + Tail) * 0.5f, Head - Tail);
	}
};
