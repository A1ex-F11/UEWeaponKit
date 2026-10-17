/**
 * 弹道后坐：把累计的俯仰 / 偏航加到射击方向上，并按停火时间收回。
 * 只改子弹方向，不改镜头。镜头冲量由 UWeaponKitCameraRecoilComponent 单独处理。
 */
#pragma once

#include "CoreMinimal.h"

/**
 * 射击方向上的后坐偏移。
 * Offset.X 是俯仰（度），Offset.Y 是偏航（度）。瞄准开火时武器不会再把这份偏移叠到视线方向上。
 */
struct FWeaponKitShotRecoilMath
{
	/**
	 * 停火后把偏移收回 0。
	 * QuietTime 不超过 Delay 时原样返回；超过后按 Speed（度/秒）分别插值俯仰和偏航。
	 */
	static FVector2D Recover(FVector2D Offset, float QuietTime, float Delay, float Speed)
	{
		const float Time = FMath::Max(0.0f, QuietTime - Delay);
		return FVector2D(
			FMath::FInterpConstantTo(Offset.X, 0.0, Time, Speed),
			FMath::FInterpConstantTo(Offset.Y, 0.0, Time, Speed));
	}

	/**
	 * 把后坐偏移加到基准射击方向上，返回单位向量。
	 * 基准或偏移含 NaN、或基准接近零时返回零向量，调用方应放弃这一发。俯仰钳在 ±89 度。
	 */
	static FVector Apply(const FVector& Base, const FVector2D& Offset)
	{
		if (Base.ContainsNaN() || Offset.ContainsNaN() || Base.IsNearlyZero())
		{
			return FVector::ZeroVector;
		}
		FRotator Aim = Base.Rotation();
		Aim.Pitch = FMath::Clamp(Aim.Pitch + Offset.X, -89.0, 89.0);
		Aim.Yaw += Offset.Y;
		return Aim.Vector().GetSafeNormal();
	}
};
