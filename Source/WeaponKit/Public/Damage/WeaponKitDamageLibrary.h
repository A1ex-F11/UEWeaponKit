/**
 * 按「骨骼 → 区域 → 倍率」结算最终伤害。
 * 蓝图里作为纯函数调用。区域表由游戏准备，可用 MakeHeadBodyRegions 生成和旧算法相同的头 / 身体表。
 */
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/WeaponKitTypes.h"
#include "WeaponKitDamageLibrary.generated.h"

/**
 * 伤害结算函数库。没有状态。
 * 先命中的区域生效；表里没有的骨骼倍率是 1，并且吃身体护甲。
 */
UCLASS()
class WEAPONKIT_API UWeaponKitDamageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 计算一次命中的最终伤害。
	 * 区域列出的骨骼使用该区域的倍率和护甲开关；其余骨骼（包括未被列入的 "head"）倍率是 1，并吃身体护甲。
	 * 头盔和身体护甲不会叠加，头盔优先。护甲生效时再乘 ArmorPenetration。Damage 非有限数时返回 0。
	 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Damage")
	static float CalculateFinalDamage(
		float Damage,
		FName HitBoneName,
		float ArmorPenetration,
		const TArray<FWeaponKitDamageRegion>& Regions,
		bool bHasHelmet,
		bool bHasBodyArmor);

	/**
	 * 生成一张头部区域表，身体走隐式回退（倍率 1，吃身体护甲）。
	 * HeadBoneName 传骨架上的头部骨骼，不要依赖写死的名字。头部吃头盔、不吃身体护甲。
	 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Damage")
	static TArray<FWeaponKitDamageRegion> MakeHeadBodyRegions(FName HeadBoneName, float HeadDamageMultiplier);
};
