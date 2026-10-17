/** 共用的姿态精度设置。多把枪可引用同一份资产，并可按枪覆盖。 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponKitAccuracyPreset.generated.h"

USTRUCT(BlueprintType)
struct WEAPONKIT_API FWeaponKitPostureSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "姿态精度", meta = (DisplayName = "走动额外散布", ClampMin = "0.0"))
	float MovingExtraAngle = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "姿态精度", meta = (DisplayName = "静步额外散布", ClampMin = "0.0"))
	float SlowWalkExtraAngle = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "姿态精度", meta = (DisplayName = "空中额外散布", ClampMin = "0.0"))
	float AirborneExtraAngle = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "姿态精度", meta = (DisplayName = "移动判定速度", ClampMin = "0.0"))
	float MovementThreshold = 10.0f;
};

UCLASS(BlueprintType, meta = (DisplayName = "共用姿态精度预设"))
class WEAPONKIT_API UWeaponKitAccuracyPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "姿态精度", meta = (ShowOnlyInnerProperties))
	FWeaponKitPostureSettings Settings;
};
