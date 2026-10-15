// 后坐资产的构造默认值。头文件里的成员初值再在初始化列表里写一遍，保证生成的类默认对象一致。
#include "Data/WeaponKitRecoilData.h"

// 默认关闭固定序列，使用随机上抬。平滑时长 0.05 秒，立刻施加 25%。
UWeaponKitRecoilData::UWeaponKitRecoilData()
	: bUseFixedPattern(false)
	, PatternResetDelay(0.25f)
	, FixedPatternPitchScale(1.0f)
	, FixedPatternYawScale(1.0f)
	, LoopStartIndex(-1)
	, SmoothKickDuration(0.05f)
	, ImmediateKickFraction(0.25f)
	, RecoveryDelay(0.2f)
	, RecoverySpeed(1.0f)
	, RecoveryPitchRatio(0.35f)
	, VerticalKickMin(0.35f)
	, VerticalKickMax(0.50f)
	, HorizontalKickMax(0.08f)
{
}
