// 枪械配置的构造和散布选择。运行时弹药不在这里。
#include "Data/WeaponKitDefinitionData.h"
#include "Data/WeaponKitBallisticsData.h"

// 腰射和瞄准使用同一结构的默认锥角。瞄准默认更紧，是否启用由 bUseSeparateAimingSpread 决定。
UWeaponKitDefinitionData::UWeaponKitDefinitionData()
{
	HipFire.StationarySpreadAngle = 0.0f;
	HipFire.MovingSpreadAngle = 3.0f;
	HipFire.JumpSpreadAngle = 12.0f;
	HipFire.SlowWalkSpreadAngle = 1.0f;
	HipFire.MovementSpreadSpeedThreshold = 10.0f;
	HipFire.bUseFiringSpread = false;
	HipFire.SpreadPerShot = 0.15f;
	HipFire.MaxFiringSpread = 0.8f;
	HipFire.SpreadRecoveryDelay = 0.2f;
	HipFire.SpreadRecoverySpeed = 2.0f;

	Aiming.StationarySpreadAngle = 0.0f;
	Aiming.MovingSpreadAngle = 1.5f;
	Aiming.JumpSpreadAngle = 6.0f;
	Aiming.SlowWalkSpreadAngle = 0.5f;
	Aiming.MovementSpreadSpeedThreshold = 10.0f;
	Aiming.bUseFiringSpread = false;
	Aiming.SpreadPerShot = 0.02f;
	Aiming.MaxFiringSpread = 0.3f;
	Aiming.SpreadRecoveryDelay = 0.15f;
	Aiming.SpreadRecoverySpeed = 1.5f;

	// 内联子对象设为 RF_Public，编辑器里才能把后坐和弹道展开编辑。
	RecoilData = CreateDefaultSubobject<UWeaponKitRecoilData>(TEXT("Recoil"));
	RecoilData->SetFlags(RF_Public);
	BallisticsData = CreateDefaultSubobject<UWeaponKitBallisticsData>(TEXT("Ballistics"));
	BallisticsData->SetFlags(RF_Public);
}

// 未开启独立瞄准散布时，瞄准也返回腰射那一份。
const FWeaponKitSpreadProfile& UWeaponKitDefinitionData::GetSpreadProfile(bool bAiming) const
{
	return (bAiming && bUseSeparateAimingSpread) ? Aiming : HipFire;
}
