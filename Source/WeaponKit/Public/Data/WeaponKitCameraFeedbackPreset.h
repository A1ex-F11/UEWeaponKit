/** 多把枪可共用的镜头冲量和平滑反馈参数。 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponKitCameraFeedbackPreset.generated.h"

USTRUCT(BlueprintType)
struct WEAPONKIT_API FWeaponKitCameraFeedbackParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "镜头反馈", meta = (DisplayName = "镜头回落速度", ClampMin = "0.1"))
	float ReturnSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "镜头反馈", meta = (DisplayName = "平滑施加时长", ClampMin = "0.0"))
	float SmoothKickDuration = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "镜头反馈", meta = (DisplayName = "立即施加比例", ClampMin = "0.0", ClampMax = "1.0"))
	float ImmediateKickFraction = 0.25f;
};

UCLASS(BlueprintType, meta = (DisplayName = "镜头反馈预设"))
class WEAPONKIT_API UWeaponKitCameraFeedbackPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "镜头反馈", meta = (DisplayName = "镜头回落速度", ClampMin = "0.1"))
	float ReturnSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "镜头反馈", meta = (DisplayName = "平滑施加时长", ClampMin = "0.0"))
	float SmoothKickDuration = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "镜头反馈", meta = (DisplayName = "立即施加比例", ClampMin = "0.0", ClampMax = "1.0"))
	float ImmediateKickFraction = 0.25f;
};
