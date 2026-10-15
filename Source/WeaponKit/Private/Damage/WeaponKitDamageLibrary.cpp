// 按伤害区域结算最终伤害。未列入的骨骼走身体规则：倍率 1，并吃身体护甲。
#include "Damage/WeaponKitDamageLibrary.h"

namespace
{
// 按数组顺序返回第一个列出该骨骼的区域。都没有则返回空，调用方使用身体回退。
const FWeaponKitDamageRegion* FindDamageRegion(const TArray<FWeaponKitDamageRegion>& Regions, FName HitBoneName)
{
	for (const FWeaponKitDamageRegion& Region : Regions)
	{
		if (Region.BoneNames.Contains(HitBoneName))
		{
			return &Region;
		}
	}
	return nullptr;
}
}

// 基础伤害乘区域倍率；头盔或身体护甲生效时再乘穿甲。头盔优先，两者不叠加。
float UWeaponKitDamageLibrary::CalculateFinalDamage(
	float Damage,
	FName HitBoneName,
	float ArmorPenetration,
	const TArray<FWeaponKitDamageRegion>& Regions,
	bool bHasHelmet,
	bool bHasBodyArmor)
{
	if (!FMath::IsFinite(Damage))
	{
		return 0.0f;
	}

	// 没有区域命中时保持这些回退：倍率 1，不吃头盔，吃身体护甲。
	float Multiplier = 1.0f;
	bool bMitigatedByHelmet = false;
	bool bMitigatedByBodyArmor = true;
	if (const FWeaponKitDamageRegion* Region = FindDamageRegion(Regions, HitBoneName))
	{
		Multiplier = FMath::IsFinite(Region->DamageMultiplier) ? FMath::Max(0.0f, Region->DamageMultiplier) : 0.0f;
		bMitigatedByHelmet = Region->bMitigatedByHelmet;
		bMitigatedByBodyArmor = Region->bMitigatedByBodyArmor;
	}

	float FinalDamage = FMath::Max(Damage, 0.0f) * Multiplier;
	// 头盔生效后不再乘身体护甲，避免两层护甲叠乘。
	const bool bHelmetApplies = bMitigatedByHelmet && bHasHelmet;
	const bool bBodyArmorApplies = bMitigatedByBodyArmor && bHasBodyArmor && !bHelmetApplies;
	if (bHelmetApplies || bBodyArmorApplies)
	{
		const float Penetration = FMath::IsFinite(ArmorPenetration) ? ArmorPenetration : 0.0f;
		FinalDamage *= Penetration;
	}
	return FMath::Max(FinalDamage, 0.0f);
}

// 只生成头部区域。其余骨骼不在表里，结算时自然走身体规则。
TArray<FWeaponKitDamageRegion> UWeaponKitDamageLibrary::MakeHeadBodyRegions(
	FName HeadBoneName,
	float HeadDamageMultiplier)
{
	FWeaponKitDamageRegion Head;
	Head.RegionId = TEXT("Head");
	if (!HeadBoneName.IsNone())
	{
		Head.BoneNames.Add(HeadBoneName);
	}
	Head.DamageMultiplier = FMath::IsFinite(HeadDamageMultiplier) ? FMath::Max(0.0f, HeadDamageMultiplier) : 0.0f;
	Head.bMitigatedByHelmet = true;
	Head.bMitigatedByBodyArmor = false;

	TArray<FWeaponKitDamageRegion> Regions;
	Regions.Add(Head);
	return Regions;
}
