/**
 * 单发伤害、穿甲、爆头倍率和射线长度。
 * 作为 UWeaponKitDefinitionData 的内联子对象创建。骨骼算不算头由伤害区域表决定，这里只提供数字。
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/WeaponKitBallisticsData.generated.h"

/**
 * 一把枪的伤害与射程。
 * 游戏在射线命中后读取这些值，再交给 UWeaponKitDamageLibrary::CalculateFinalDamage。
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, meta = (DisplayName = "伤害与射程设置"))
class WEAPONKIT_API UWeaponKitBallisticsData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 单发基础伤害。结算前还会乘区域倍率，并可能再乘穿甲。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "伤害与射程", meta = (DisplayName = "每发伤害", ToolTip = "单发基础伤害。", ClampMin = "0.0"))
	float DamagePerShot = 0.0f;

	/** 传给伤害结算的穿甲系数。头盔或身体护甲生效时，最终伤害乘这个值；不在这里判断部位。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "伤害与射程", meta = (DisplayName = "穿甲参数", ToolTip = "传给伤害结算的穿甲值。命中哪个部位、乘什么倍率，由伤害区域表决定。", ClampMin = "0.0"))
	float ArmorPenetration = 0.0f;

	/** 提供给头部区域的倍率。哪根骨骼算头由调用方的伤害区域表决定，资产本身不写死骨骼名。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "伤害与射程", meta = (DisplayName = "爆头伤害倍率", ToolTip = "提供给头部区域的倍率。骨骼是否算头部由伤害区域表决定，这里不写死骨骼名。", ClampMin = "0.0"))
	float HeadDamageMultiplier = 0.0f;

	/** 命中检测射线的最大长度（厘米）。插件不自己开枪，由游戏用这个长度做射线。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "伤害与射程", meta = (DisplayName = "最大射程（厘米）", ToolTip = "命中检测射线的最大长度。", ClampMin = "0.0"))
	float TraceDistance = 0.0f;
};
