// 枪械配置的旧值迁移、共享精度预设解析和散布快照生成。
#include "Data/WeaponKitDefinitionData.h"
#include "Data/WeaponKitBallisticsData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
float WeaponKitSafeNonNegative(float Value)
{
	return FMath::IsFinite(Value) ? FMath::Max(0.0f, Value) : 0.0f;
}
}

FWeaponKitSpreadProfile FWeaponKitIntuitiveAccuracy::BuildProfile(float ShotInterval, float LegacyThreshold) const
{
	FWeaponKitSpreadProfile Profile;
	Profile.bUseAdditiveStateSpread = true;
	Profile.StationarySpreadAngle = FMath::Min(89.0f, WeaponKitSafeNonNegative(BaseSpreadAngle));
	const FWeaponKitPostureSettings Posture = GetPosture(LegacyThreshold);
	Profile.MovingSpreadAngle = WeaponKitSafeNonNegative(Posture.MovingExtraAngle);
	Profile.SlowWalkSpreadAngle = WeaponKitSafeNonNegative(Posture.SlowWalkExtraAngle);
	Profile.JumpSpreadAngle = WeaponKitSafeNonNegative(Posture.AirborneExtraAngle);
	Profile.MovementSpreadSpeedThreshold = WeaponKitSafeNonNegative(Posture.MovementThreshold);
	Profile.MaxFiringSpread = FMath::Min(89.0f, WeaponKitSafeNonNegative(MaxFiringExtraAngle));
	Profile.bUseFiringSpread = Profile.MaxFiringSpread > 0.0f;
	Profile.SpreadRecoveryDelay = 0.0f;
	const float RecoverySeconds = FMath::Max(0.01f, WeaponKitSafeNonNegative(FullRecoverySeconds));
	Profile.SpreadRecoverySpeed = Profile.MaxFiringSpread / RecoverySeconds;
	const int32 ShotCount = FMath::Clamp(ShotsToMaximum, 1, 1000);
	const double PerShot = static_cast<double>(Profile.MaxFiringSpread) / ShotCount
		+ static_cast<double>(Profile.SpreadRecoverySpeed) * WeaponKitSafeNonNegative(ShotInterval)
			* (ShotCount - 1) / ShotCount;
	Profile.SpreadPerShot = static_cast<float>(FMath::Min(PerShot, static_cast<double>(Profile.MaxFiringSpread)));

	// Until one of the four author-facing firing controls is edited, keep old timing exactly.
	if (bPreserveMigratedFiring)
	{
		Profile.bUseFiringSpread = MigratedFiring.bUseFiringSpread;
		Profile.SpreadPerShot = MigratedFiring.SpreadPerShot;
		Profile.MaxFiringSpread = MigratedFiring.MaxFiringSpread;
		Profile.SpreadRecoveryDelay = MigratedFiring.SpreadRecoveryDelay;
		Profile.SpreadRecoverySpeed = MigratedFiring.SpreadRecoverySpeed;
	}
	return Profile;
}

FWeaponKitPostureSettings FWeaponKitIntuitiveAccuracy::GetPosture(float LegacyThreshold) const
{
	if (bUsePostureSettings)
	{
		if (PosturePreset && !bOverridePosture)
		{
			return PosturePreset->Settings;
		}
		return PostureOverride;
	}

	FWeaponKitPostureSettings Posture;
	Posture.MovingExtraAngle = MovingExtraAngle;
	Posture.SlowWalkExtraAngle = SlowWalkExtraAngle;
	Posture.AirborneExtraAngle = AirborneExtraAngle;
	Posture.MovementThreshold = LegacyThreshold;
	return Posture;
}

void FWeaponKitIntuitiveAccuracy::MigratePosture(float LegacyThreshold)
{
	if (bUsePostureSettings)
	{
		return;
	}
	PostureOverride = GetPosture(LegacyThreshold);
	bUsePostureSettings = true;
}

FWeaponKitIntuitiveAccuracy FWeaponKitIntuitiveAccuracy::FromLegacy(
	const FWeaponKitSpreadProfile& Profile,
	float ShotInterval)
{
	FWeaponKitIntuitiveAccuracy Accuracy;
	Accuracy.BaseSpreadAngle = WeaponKitSafeNonNegative(Profile.StationarySpreadAngle);
	const float LegacyBase = Profile.bUseAdditiveStateSpread ? 0.0f : Accuracy.BaseSpreadAngle;
	Accuracy.MovingExtraAngle = FMath::Max(0.0f, WeaponKitSafeNonNegative(Profile.MovingSpreadAngle) - LegacyBase);
	Accuracy.SlowWalkExtraAngle = FMath::Max(0.0f, WeaponKitSafeNonNegative(Profile.SlowWalkSpreadAngle) - LegacyBase);
	Accuracy.AirborneExtraAngle = FMath::Max(0.0f, WeaponKitSafeNonNegative(Profile.JumpSpreadAngle) - LegacyBase);
	Accuracy.MaxFiringExtraAngle = Profile.bUseFiringSpread
		? WeaponKitSafeNonNegative(Profile.MaxFiringSpread) : 0.0f;
	Accuracy.FullRecoverySeconds = Profile.SpreadRecoverySpeed > 0.0f && FMath::IsFinite(Profile.SpreadRecoverySpeed)
		? Accuracy.MaxFiringExtraAngle / Profile.SpreadRecoverySpeed : 1.0f;
	const float DelayAdjustedInterval = FMath::Max(0.0f,
		WeaponKitSafeNonNegative(ShotInterval) - WeaponKitSafeNonNegative(Profile.SpreadRecoveryDelay));
	const float NetPerShot = WeaponKitSafeNonNegative(Profile.SpreadPerShot)
		- WeaponKitSafeNonNegative(Profile.SpreadRecoverySpeed) * DelayAdjustedInterval;
	Accuracy.ShotsToMaximum = NetPerShot > UE_SMALL_NUMBER
		? FMath::Clamp(FMath::CeilToInt(Accuracy.MaxFiringExtraAngle / NetPerShot), 1, 1000)
		: 1000;
	Accuracy.MigratedFiring = Profile;
	Accuracy.bPreserveMigratedFiring = true;
	Accuracy.MigratePosture(Profile.MovementSpreadSpeedThreshold);
	return Accuracy;
}

UWeaponKitDefinitionData::UWeaponKitDefinitionData()
{
	RecoilData = CreateDefaultSubobject<UWeaponKitRecoilData>(TEXT("Recoil"));
	RecoilData->SetFlags(RF_Public);
	BallisticsData = CreateDefaultSubobject<UWeaponKitBallisticsData>(TEXT("Ballistics"));
	BallisticsData->SetFlags(RF_Public);
}

void UWeaponKitDefinitionData::PostInitProperties()
{
	Super::PostInitProperties();

	UPackage* Package = GetOutermost();
	if (!HasAnyFlags(RF_Public | RF_Standalone) || !Package || Package == GetTransientPackage() || Package->HasAnyFlags(RF_Transient) ||
		HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_NeedLoad))
	{
		return;
	}

	// New persistent assets start on the unified schema. Loaded legacy packages are migrated in PostLoad.
	ConfigurationVersion = 1;
	bUseIntuitiveHipAccuracy = true;
	bUseIntuitiveAimAccuracy = true;
	bUseAimingSpread = false;
	HipAccuracy.bUsePostureSettings = true;
	AimAccuracy.bUsePostureSettings = true;
	if (RecoilData)
	{
		RecoilData->bUseFixedPattern = true;
		RecoilData->bUseIndependentShotRecoil = true;
	}
}

void UWeaponKitDefinitionData::PostLoad()
{
	Super::PostLoad();
	MigrateConfiguration();
}

void UWeaponKitDefinitionData::MigrateConfiguration()
{
	if (ConfigurationVersion >= 1)
	{
		return;
	}

	const int32 InitialModeIndex = AllowedFireModes.IndexOfByKey(DefaultFireMode);
	if (InitialModeIndex > 0)
	{
		const TArray<EWeaponKitFireMode> LegacyOrder = AllowedFireModes;
		for (int32 Index = 0; Index < LegacyOrder.Num(); ++Index)
		{
			AllowedFireModes[Index] = LegacyOrder[(InitialModeIndex + Index) % LegacyOrder.Num()];
		}
	}
	else if (InitialModeIndex == INDEX_NONE)
	{
		AllowedFireModes.Insert(DefaultFireMode, 0);
	}

	ReloadStyle = !bReloadOneRoundAtATime
		? EWeaponKitReloadStyle::Magazine
		: bUseBatchReloadForFullEmptyReload
			? EWeaponKitReloadStyle::PerRoundOrEmptyBatch
			: EWeaponKitReloadStyle::PerRound;

	const float LegacyShotInterval = DefaultFireMode == EWeaponKitFireMode::Burst && BurstShotInterval > 0.0f
		? BurstShotInterval
		: FireInterval;
	if (!bUseIntuitiveHipAccuracy)
	{
		HipAccuracy = FWeaponKitIntuitiveAccuracy::FromLegacy(HipFire, LegacyShotInterval);
	}
	else
	{
		HipAccuracy.MigratePosture(HipFire.MovementSpreadSpeedThreshold);
	}

	if (!bUseIntuitiveAimAccuracy)
	{
		AimAccuracy = FWeaponKitIntuitiveAccuracy::FromLegacy(Aiming, LegacyShotInterval);
		bUseAimingSpread = bUseSeparateAimingSpread;
	}
	else
	{
		AimAccuracy.MigratePosture(Aiming.MovementSpreadSpeedThreshold);
	}

	bUseIntuitiveHipAccuracy = true;
	bUseIntuitiveAimAccuracy = true;
	ConfigurationVersion = 1;
}

EWeaponKitFireMode UWeaponKitDefinitionData::GetInitialFireMode() const
{
	return ConfigurationVersion >= 1 && !AllowedFireModes.IsEmpty()
		? AllowedFireModes[0]
		: DefaultFireMode;
}

bool UWeaponKitDefinitionData::UsesSeparateAimingSpread() const
{
	return ConfigurationVersion >= 1 ? bUseAimingSpread : bUseSeparateAimingSpread;
}

bool UWeaponKitDefinitionData::UsesPerRoundReload() const
{
	return ConfigurationVersion >= 1
		? ReloadStyle != EWeaponKitReloadStyle::Magazine
		: bReloadOneRoundAtATime;
}

bool UWeaponKitDefinitionData::UsesEmptyBatchReload() const
{
	return ConfigurationVersion >= 1
		? ReloadStyle == EWeaponKitReloadStyle::PerRoundOrEmptyBatch
		: bUseBatchReloadForFullEmptyReload;
}

FWeaponKitSpreadProfile UWeaponKitDefinitionData::GetSpreadProfile(bool bAiming, float ShotInterval) const
{
	if (ConfigurationVersion < 1)
	{
		return bAiming && bUseSeparateAimingSpread ? Aiming : HipFire;
	}

	const float EffectiveInterval = ShotInterval > 0.0f ? ShotInterval : FireInterval;
	return bAiming && bUseAimingSpread
		? AimAccuracy.BuildProfile(EffectiveInterval, HipFire.MovementSpreadSpeedThreshold)
		: HipAccuracy.BuildProfile(EffectiveInterval, HipFire.MovementSpreadSpeedThreshold);
}

#if WITH_EDITOR
void UWeaponKitDefinitionData::PreEditChange(FProperty* PropertyAboutToChange)
{
	MigrateConfiguration();
	Super::PreEditChange(PropertyAboutToChange);
}

void UWeaponKitDefinitionData::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	const FName Field = Event.Property ? Event.Property->GetFName() : NAME_None;
	const FName Member = Event.MemberProperty ? Event.MemberProperty->GetFName() : NAME_None;
	if (Field == GET_MEMBER_NAME_CHECKED(FWeaponKitIntuitiveAccuracy, BaseSpreadAngle)
		|| Field == GET_MEMBER_NAME_CHECKED(FWeaponKitIntuitiveAccuracy, MaxFiringExtraAngle)
		|| Field == GET_MEMBER_NAME_CHECKED(FWeaponKitIntuitiveAccuracy, ShotsToMaximum)
		|| Field == GET_MEMBER_NAME_CHECKED(FWeaponKitIntuitiveAccuracy, FullRecoverySeconds))
	{
		if (Member == GET_MEMBER_NAME_CHECKED(UWeaponKitDefinitionData, HipAccuracy))
		{
			HipAccuracy.bPreserveMigratedFiring = false;
		}
		else if (Member == GET_MEMBER_NAME_CHECKED(UWeaponKitDefinitionData, AimAccuracy))
		{
			AimAccuracy.bPreserveMigratedFiring = false;
		}
	}
	if (ConfigurationVersion < 1)
	{
		MigrateConfiguration();
	}
	Super::PostEditChangeProperty(Event);
}
#endif
