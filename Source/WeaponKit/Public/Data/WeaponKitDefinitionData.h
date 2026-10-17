/** 一把枪的静态玩法配置和旧资产迁移入口。运行时状态仍由 AWeaponKitRuntime 持有。 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/WeaponKitTypes.h"
#include "Data/WeaponKitAccuracyPreset.h"
#include "Data/WeaponKitRecoilData.h"
#include "WeaponKitDefinitionData.generated.h"

class UWeaponKitBallisticsData;

/** 可编辑的精度参数。射击增长、恢复速度由当前有效射击间隔计算。 */
USTRUCT(BlueprintType)
struct WEAPONKIT_API FWeaponKitIntuitiveAccuracy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "基础散布", ClampMin = "0.0", ClampMax = "89.0"))
	float BaseSpreadAngle = 0.2f;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Compatibility")
	float MovingExtraAngle = 2.0f;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Compatibility")
	float SlowWalkExtraAngle = 0.5f;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Compatibility")
	float AirborneExtraAngle = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "最大射击额外散布", ClampMin = "0.0"))
	float MaxFiringExtraAngle = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "连续到顶参考发数", ClampMin = "1", ClampMax = "1000"))
	int32 ShotsToMaximum = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "满累计恢复秒数", ClampMin = "0.01"))
	float FullRecoverySeconds = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "姿态精度预设"))
	TObjectPtr<UWeaponKitAccuracyPreset> PosturePreset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "覆盖姿态预设", EditCondition = "PosturePreset != nullptr", EditConditionHides))
	bool bOverridePosture = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "独立姿态参数", EditCondition = "PosturePreset == nullptr || bOverridePosture", EditConditionHides))
	FWeaponKitPostureSettings PostureOverride;

	/** 以下为迁移状态；不作为第二套可编辑配置。 */
	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Compatibility")
	bool bUsePostureSettings = false;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Compatibility")
	bool bPreserveMigratedFiring = false;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Compatibility")
	FWeaponKitSpreadProfile MigratedFiring;

	FWeaponKitPostureSettings GetPosture(float LegacyThreshold) const;
	void MigratePosture(float LegacyThreshold);
	static FWeaponKitIntuitiveAccuracy FromLegacy(const FWeaponKitSpreadProfile& Profile, float ShotInterval);
	FWeaponKitSpreadProfile BuildProfile(float ShotInterval, float LegacyThreshold) const;
};

/** 一把枪的静态玩法配置。弹匣容量与初始装弹数是独立字段。 */
UCLASS(BlueprintType, meta = (DisplayName = "枪械玩法配置"))
class WEAPONKIT_API UWeaponKitDefinitionData : public UDataAsset
{
	GENERATED_BODY()

public:
	UWeaponKitDefinitionData();
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

	/** 将旧分离字段迁移到统一配置。PostLoad 自动调用；transient 对象可由测试或工具显式调用。 */
	void MigrateConfiguration();
	EWeaponKitFireMode GetInitialFireMode() const;
	bool UsesSeparateAimingSpread() const;
	bool UsesPerRoundReload() const;
	bool UsesEmptyBatchReload() const;
	int32 GetConfigurationVersion() const { return ConfigurationVersion; }

	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Compatibility")
	int32 ConfigurationVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Compatibility")
	bool bUseIntuitiveHipAccuracy = false;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Compatibility")
	bool bUseIntuitiveAimAccuracy = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|基础与开火", meta = (DisplayName = "武器展示名"))
	FText WeaponDisplayName;

	/** 一个有序列表；迁移后首项同时作为初始开火模式。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|基础与开火", meta = (DisplayName = "开火模式（首项为初始）"))
	TArray<EWeaponKitFireMode> AllowedFireModes = { EWeaponKitFireMode::SemiAutomatic };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|基础与开火", meta = (DisplayName = "射击间隔", ClampMin = "0.0"))
	float FireInterval = 0.15f;

	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = "枪械配置|基础与开火", meta = (DisplayName = "连发间隔", ClampMin = "0.0"))
	float BurstShotInterval = 0.0f;

	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = "枪械配置|基础与开火", meta = (DisplayName = "连发恢复间隔", ClampMin = "0.0"))
	float BurstRecoveryInterval = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|弹药与装填", meta = (DisplayName = "弹匣容量", ClampMin = "1"))
	int32 MagazineCapacity = 30;

	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = "枪械配置|弹药与装填", meta = (DisplayName = "初始装弹数", ClampMin = "0"))
	int32 InitialMagazineAmmo = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|弹药与装填", meta = (DisplayName = "初始备弹数", ClampMin = "0"))
	int32 InitialReserveAmmo = 90;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|弹药与装填", meta = (DisplayName = "装填方式"))
	EWeaponKitReloadStyle ReloadStyle = EWeaponKitReloadStyle::Magazine;

	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = "枪械配置|弹药与装填", meta = (DisplayName = "开始换弹时锁定装弹数量"))
	bool bLockReloadAmountAtStart = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "枪械配置|伤害与射程", meta = (DisplayName = "伤害与弹道参数"))
	TObjectPtr<UWeaponKitBallisticsData> BallisticsData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|精度", meta = (DisplayName = "腰射精度"))
	FWeaponKitIntuitiveAccuracy HipAccuracy;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|精度", meta = (DisplayName = "启用独立瞄准精度"))
	bool bUseAimingSpread = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "枪械配置|精度", meta = (DisplayName = "瞄准精度", EditCondition = "bUseAimingSpread", EditConditionHides))
	FWeaponKitIntuitiveAccuracy AimAccuracy;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "枪械配置|后坐与镜头", meta = (DisplayName = "后坐参数"))
	TObjectPtr<UWeaponKitRecoilData> RecoilData;

	/** 旧配置字段仍可被现有 C++ / 蓝图读取和反序列化，但不再显示为第二套编辑入口。 */
	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Compatibility")
	EWeaponKitFireMode DefaultFireMode = EWeaponKitFireMode::SemiAutomatic;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Compatibility")
	FWeaponKitSpreadProfile HipFire;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Compatibility")
	bool bUseSeparateAimingSpread = false;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Compatibility")
	FWeaponKitSpreadProfile Aiming;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Compatibility")
	bool bReloadOneRoundAtATime = false;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Compatibility")
	bool bUseBatchReloadForFullEmptyReload = false;

	/** 返回当前模式下参与真实射击的配置快照。ShotInterval <= 0 时用资产的普通射击间隔。 */
	FWeaponKitSpreadProfile GetSpreadProfile(bool bAiming, float ShotInterval = 0.0f) const;
};
