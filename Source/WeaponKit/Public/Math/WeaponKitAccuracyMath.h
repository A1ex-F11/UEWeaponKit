/** 射击方向和准星投影共用的姿态散布解析。 */
#pragma once

#include "CoreMinimal.h"
#include "Data/WeaponKitTypes.h"

struct WEAPONKIT_API FWeaponKitAccuracyMath
{
	/** 根据同一份射击配置和姿态，返回基础 / 姿态 / 射击累计的总锥角。 */
	static float ResolveTotalSpreadAngle(
		const FWeaponKitSpreadProfile& Profile,
		float CurrentHorizontalSpeed,
		bool bIsFalling,
		bool bIsCrouched,
		bool bIsSlowWalking,
		float FiringSpread);
};

inline float FWeaponKitAccuracyMath::ResolveTotalSpreadAngle(
	const FWeaponKitSpreadProfile& Profile,
	float CurrentHorizontalSpeed,
	bool bIsFalling,
	bool bIsCrouched,
	bool bIsSlowWalking,
	float FiringSpread)
{
	const auto Safe = [](float Value)
	{
		return FMath::IsFinite(Value) ? FMath::Max(0.0f, Value) : 0.0f;
	};
	const float Speed = Safe(CurrentHorizontalSpeed);
	const float Threshold = Safe(Profile.MovementSpreadSpeedThreshold);
	const float ShotSpread = Safe(FiringSpread);
	if (Profile.bUseAdditiveStateSpread)
	{
		const float MovementExtra = Speed > Threshold
			? Safe(bIsSlowWalking && !bIsFalling ? Profile.SlowWalkSpreadAngle : Profile.MovingSpreadAngle)
			: 0.0f;
		return FMath::Clamp(
			Safe(Profile.StationarySpreadAngle) + MovementExtra
				+ (bIsFalling ? Safe(Profile.JumpSpreadAngle) : 0.0f) + ShotSpread,
			0.0f,
			89.0f);
	}

	float SelectedSpreadAngle = Profile.MovingSpreadAngle;
	if (bIsFalling)
	{
		SelectedSpreadAngle = Profile.JumpSpreadAngle;
	}
	else if (bIsCrouched || Speed <= Threshold)
	{
		SelectedSpreadAngle = Profile.StationarySpreadAngle;
	}
	else if (bIsSlowWalking)
	{
		SelectedSpreadAngle = Profile.SlowWalkSpreadAngle;
	}
	return FMath::Clamp(Safe(SelectedSpreadAngle) + ShotSpread, 0.0f, 89.0f);
}
