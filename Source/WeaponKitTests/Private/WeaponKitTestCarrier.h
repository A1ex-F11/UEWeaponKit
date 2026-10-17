/**
 * 测试用携带者。实现 IWeaponKitCarrier，供背包和扫描器自动化测试使用。
 * 不属于运行时插件，不会打进游戏包。
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/WeaponKitInterfaces.h"
#include "Runtime/WeaponKitRuntime.h"
#include "WeaponKitTestCarrier.generated.h"

/**
 * 可开关存活和首抽标记的测试 Pawn。
 * bAlive 为 false 时拾取和背包操作应失败。首抽只对 LastDrawWeapon 记为已消耗。
 */
UCLASS()
class WEAPONKITTESTS_API AWeaponKitTestCarrier : public APawn, public IWeaponKitCarrier
{
	GENERATED_BODY()

public:
	/** 测试里直接改这个值，模拟角色死亡或复活。 */
	bool bAlive = true;

	/** 为 true 时，仅 LastDrawWeapon 那把枪的首抽视为已播过。 */
	bool bDrawConsumed = false;

	/** 返回 bAlive。 */
	virtual bool IsWeaponKitCarrierAlive_Implementation() const override
	{
		return bAlive;
	}

	/** 只有标记已消耗、且传入的就是记下的那把枪时返回 true。 */
	virtual bool HasConsumedWeaponInitialDraw_Implementation(AWeaponKitRuntime* Weapon) const override
	{
		return bDrawConsumed && Weapon == LastDrawWeapon.Get();
	}

	/** 记下这把枪已经播过首抽。 */
	virtual void ApplyWeaponInitialDrawConsumed_Implementation(AWeaponKitRuntime* Weapon) override
	{
		bDrawConsumed = true;
		LastDrawWeapon = Weapon;
	}

	/** 最近一次被标记为首抽已消耗的武器。 */
	TWeakObjectPtr<AWeaponKitRuntime> LastDrawWeapon;
};
