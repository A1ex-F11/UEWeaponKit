// 配置迁移、预设共享、初始弹药独立性和准星散布投影的回归测试。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Data/WeaponKitAccuracyPreset.h"
#include "Data/WeaponKitCameraFeedbackPreset.h"
#include "Data/WeaponKitDefinitionData.h"
#include "Data/WeaponKitRecoilData.h"
#include "Runtime/WeaponKitRuntime.h"
#include "UObject/Package.h"
#include "WeaponKitTestWorld.h"
#include "UObject/UnrealType.h"

namespace
{
bool NearlyEqual(float A, float B, float Tolerance = 0.0001f)
{
	return FMath::IsNearlyEqual(A, B, Tolerance);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeaponKitNewAssetFirstEditTest,
	"WeaponKit.Configuration.NewPersistentAssetFirstEditKeepsUnifiedDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeaponKitNewAssetFirstEditTest::RunTest(const FString& Parameters)
{
	const FString PackageName = FString::Printf(
		TEXT("/Temp/WeaponKitNewDefinition_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	UPackage* Package = CreatePackage(*PackageName);
	if (!TestNotNull(TEXT("Persistent test package created"), Package))
	{
		return false;
	}

	UWeaponKitDefinitionData* NewDefinition = NewObject<UWeaponKitDefinitionData>(
		Package,
		NAME_None,
		RF_Public | RF_Standalone);
	if (!TestNotNull(TEXT("Persistent new definition created"), NewDefinition))
	{
		return false;
	}

	TestEqual(TEXT("A newly created persistent asset starts on configuration version 1"),
		NewDefinition->GetConfigurationVersion(), 1);
	TestTrue(TEXT("A new asset initializes both unified accuracy profiles and posture settings"),
		NewDefinition->bUseIntuitiveHipAccuracy && NewDefinition->bUseIntuitiveAimAccuracy &&
		NewDefinition->HipAccuracy.bUsePostureSettings && NewDefinition->AimAccuracy.bUsePostureSettings);
	TestTrue(TEXT("New accuracy defaults survive initialization"),
		NearlyEqual(NewDefinition->HipAccuracy.BaseSpreadAngle, 0.2f) &&
		NearlyEqual(NewDefinition->HipAccuracy.MaxFiringExtraAngle, 0.6f) &&
		NearlyEqual(NewDefinition->HipAccuracy.GetPosture(0.0f).MovingExtraAngle, 2.0f));
	TestTrue(TEXT("New recoil defaults expose the fixed separated pattern editor"),
		NewDefinition->RecoilData && NewDefinition->RecoilData->bUseFixedPattern &&
		NewDefinition->RecoilData->bUseIndependentShotRecoil);

#if WITH_EDITOR
	FProperty* BaseProperty = FindFProperty<FProperty>(
		FWeaponKitIntuitiveAccuracy::StaticStruct(),
		GET_MEMBER_NAME_CHECKED(FWeaponKitIntuitiveAccuracy, BaseSpreadAngle));
	FProperty* HipMember = FindFProperty<FProperty>(
		UWeaponKitDefinitionData::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UWeaponKitDefinitionData, HipAccuracy));
	if (!TestNotNull(TEXT("Accuracy edit field found"), BaseProperty) ||
		!TestNotNull(TEXT("Hip accuracy member found"), HipMember))
	{
		return false;
	}

	NewDefinition->PreEditChange(BaseProperty);
	NewDefinition->HipAccuracy.BaseSpreadAngle = 0.45f;
	FPropertyChangedEvent EditEvent(BaseProperty);
	EditEvent.SetActiveMemberProperty(HipMember);
	NewDefinition->PostEditChangeProperty(EditEvent);
	TestTrue(TEXT("The first editor change does not run legacy migration over new fields"),
		NewDefinition->GetConfigurationVersion() == 1 &&
		NearlyEqual(NewDefinition->HipAccuracy.BaseSpreadAngle, 0.45f) &&
		NearlyEqual(NewDefinition->HipAccuracy.MaxFiringExtraAngle, 0.6f));
#endif

	UWeaponKitDefinitionData* LegacyPackageObject = NewObject<UWeaponKitDefinitionData>(
		Package,
		MakeUniqueObjectName(Package, UWeaponKitDefinitionData::StaticClass(), TEXT("LegacyDefinition")),
		RF_Public | RF_Standalone | RF_NeedLoad);
	if (!TestNotNull(TEXT("Legacy package object created with load flag"), LegacyPackageObject))
	{
		return false;
	}
	TestEqual(TEXT("An object marked for package load is not initialized as a new asset"),
		LegacyPackageObject->GetConfigurationVersion(), 0);
	LegacyPackageObject->DefaultFireMode = EWeaponKitFireMode::FullAutomatic;
	LegacyPackageObject->AllowedFireModes = {
		EWeaponKitFireMode::SemiAutomatic,
		EWeaponKitFireMode::FullAutomatic };
	LegacyPackageObject->HipFire.StationarySpreadAngle = 4.0f;
	LegacyPackageObject->HipFire.MovingSpreadAngle = 6.0f;
	LegacyPackageObject->HipFire.bUseFiringSpread = true;
	LegacyPackageObject->HipFire.SpreadPerShot = 0.3f;
	LegacyPackageObject->HipFire.MaxFiringSpread = 1.0f;
	LegacyPackageObject->ClearFlags(RF_NeedLoad);
	LegacyPackageObject->PostLoad();
	TestTrue(TEXT("PostLoad migrates the old serialized schema and preserves its firing profile"),
		LegacyPackageObject->GetConfigurationVersion() == 1 &&
		LegacyPackageObject->AllowedFireModes.Num() == 2 &&
		LegacyPackageObject->AllowedFireModes[0] == EWeaponKitFireMode::FullAutomatic &&
		NearlyEqual(LegacyPackageObject->HipAccuracy.BaseSpreadAngle, 4.0f) &&
		NearlyEqual(LegacyPackageObject->GetSpreadProfile(false).SpreadPerShot, 0.3f));
	TestFalse(TEXT("Old package recoil defaults are not changed by new-asset initialization"),
		LegacyPackageObject->RecoilData->bUseFixedPattern ||
		LegacyPackageObject->RecoilData->bUseIndependentShotRecoil);

	NewDefinition->ClearFlags(RF_Public | RF_Standalone);
	LegacyPackageObject->ClearFlags(RF_Public | RF_Standalone);
	return true;
}

AWeaponKitRuntime* SpawnConfiguredWeapon(UWorld* World, UWeaponKitDefinitionData* Definition)
{
	if (!World || !Definition)
	{
		return nullptr;
	}
	AWeaponKitRuntime* Weapon = World->SpawnActorDeferred<AWeaponKitRuntime>(
		AWeaponKitRuntime::StaticClass(),
		FTransform::Identity,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Weapon)
	{
		return nullptr;
	}
	Weapon->WeaponDefinition = Definition;
	Weapon->FinishSpawning(FTransform::Identity);
	return Weapon;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeaponKitConfigurationMigrationTest,
	"WeaponKit.Configuration.LegacyMigrationAndIndependentInitialAmmo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeaponKitConfigurationMigrationTest::RunTest(const FString& Parameters)
{
	UWeaponKitDefinitionData* Definition = NewObject<UWeaponKitDefinitionData>(GetTransientPackage());
	Definition->DefaultFireMode = EWeaponKitFireMode::FullAutomatic;
	Definition->AllowedFireModes = {
		EWeaponKitFireMode::SemiAutomatic,
		EWeaponKitFireMode::Burst,
		EWeaponKitFireMode::FullAutomatic };
	Definition->bReloadOneRoundAtATime = true;
	Definition->bUseBatchReloadForFullEmptyReload = true;
	Definition->bUseSeparateAimingSpread = true;
	Definition->MagazineCapacity = 30;
	Definition->InitialMagazineAmmo = 7;
	Definition->InitialReserveAmmo = 41;
	Definition->FireInterval = 0.2f;
	Definition->BurstShotInterval = 0.08f;
	Definition->HipFire.StationarySpreadAngle = 4.0f;
	Definition->HipFire.MovingSpreadAngle = 6.0f;
	Definition->HipFire.SlowWalkSpreadAngle = 5.0f;
	Definition->HipFire.JumpSpreadAngle = 12.0f;
	Definition->HipFire.MovementSpreadSpeedThreshold = 11.0f;
	Definition->HipFire.bUseFiringSpread = true;
	Definition->HipFire.SpreadPerShot = 0.3f;
	Definition->HipFire.MaxFiringSpread = 1.2f;
	Definition->HipFire.SpreadRecoveryDelay = 0.1f;
	Definition->HipFire.SpreadRecoverySpeed = 2.5f;

	TestEqual(TEXT("Transient legacy data is not silently migrated on construction"), Definition->GetConfigurationVersion(), 0);
	TestTrue(TEXT("Legacy default fire mode remains readable before migration"),
		Definition->GetInitialFireMode() == EWeaponKitFireMode::FullAutomatic);
	const FWeaponKitSpreadProfile LegacyProfile = Definition->GetSpreadProfile(false, 0.08f);
	TestFalse(TEXT("Legacy profile keeps its absolute-angle interpretation"), LegacyProfile.bUseAdditiveStateSpread);
	TestTrue(TEXT("Legacy absolute movement angle remains intact"), NearlyEqual(LegacyProfile.MovingSpreadAngle, 6.0f));

	Definition->MigrateConfiguration();
	TestEqual(TEXT("Explicit migration advances the config version"), Definition->GetConfigurationVersion(), 1);
	TestTrue(TEXT("Default mode is rotated to the first allowed entry"),
		Definition->AllowedFireModes.Num() == 3 &&
		Definition->AllowedFireModes[0] == EWeaponKitFireMode::FullAutomatic &&
		Definition->AllowedFireModes[1] == EWeaponKitFireMode::SemiAutomatic &&
		Definition->AllowedFireModes[2] == EWeaponKitFireMode::Burst);
	TestTrue(TEXT("Reload booleans migrate to the combined empty-batch option"),
		Definition->ReloadStyle == EWeaponKitReloadStyle::PerRoundOrEmptyBatch &&
		Definition->UsesPerRoundReload() && Definition->UsesEmptyBatchReload());
	TestTrue(TEXT("Legacy spread values become non-negative base plus posture extras"),
		NearlyEqual(Definition->HipAccuracy.BaseSpreadAngle, 4.0f) &&
		NearlyEqual(Definition->HipAccuracy.GetPosture(10.0f).MovingExtraAngle, 2.0f) &&
		NearlyEqual(Definition->HipAccuracy.GetPosture(10.0f).SlowWalkExtraAngle, 1.0f));
	const FWeaponKitSpreadProfile MigratedProfile = Definition->GetSpreadProfile(false, 0.08f);
	TestTrue(TEXT("Migrated state uses additive posture spread"), MigratedProfile.bUseAdditiveStateSpread);
	TestTrue(TEXT("Unedited firing timing stays at its legacy values"),
		MigratedProfile.bUseFiringSpread && NearlyEqual(MigratedProfile.SpreadPerShot, 0.3f) &&
		NearlyEqual(MigratedProfile.MaxFiringSpread, 1.2f) &&
		NearlyEqual(MigratedProfile.SpreadRecoveryDelay, 0.1f) &&
		NearlyEqual(MigratedProfile.SpreadRecoverySpeed, 2.5f));
	TestTrue(TEXT("Legacy C++ properties remain reflected for compatibility"),
		FindFProperty<FProperty>(UWeaponKitDefinitionData::StaticClass(), GET_MEMBER_NAME_CHECKED(UWeaponKitDefinitionData, DefaultFireMode)) != nullptr &&
		FindFProperty<FProperty>(UWeaponKitDefinitionData::StaticClass(), GET_MEMBER_NAME_CHECKED(UWeaponKitDefinitionData, HipFire)) != nullptr &&
		FindFProperty<FProperty>(UWeaponKitDefinitionData::StaticClass(), GET_MEMBER_NAME_CHECKED(UWeaponKitDefinitionData, Aiming)) != nullptr);
	TestTrue(TEXT("Initial ammo remains independently configurable from magazine capacity"),
		Definition->MagazineCapacity == 30 && Definition->InitialMagazineAmmo == 7);

	FWeaponKitTestWorld TestWorld;
	if (!TestNotNull(TEXT("Transient world created"), TestWorld.Get()))
	{
		return false;
	}
	AWeaponKitRuntime* Weapon = SpawnConfiguredWeapon(TestWorld.Get(), Definition);
	if (!TestNotNull(TEXT("Migrated weapon spawned"), Weapon))
	{
		return false;
	}
	TestEqual(TEXT("Runtime retains the configured magazine capacity"), Weapon->GetMagazineCapacity(), 30);
	TestEqual(TEXT("Runtime loads the separate partial initial ammo"), Weapon->GetCurrentAmmo(), 7);
	TestTrue(TEXT("Runtime initial mode comes from the reordered first entry"),
		Weapon->GetFireMode() == EWeaponKitFireMode::FullAutomatic);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeaponKitSharedPresetTest,
	"WeaponKit.Configuration.SharedAccuracyAndCameraPresets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeaponKitSharedPresetTest::RunTest(const FString& Parameters)
{
	UWeaponKitAccuracyPreset* AccuracyPreset = NewObject<UWeaponKitAccuracyPreset>(GetTransientPackage());
	AccuracyPreset->Settings.MovingExtraAngle = 2.5f;
	AccuracyPreset->Settings.SlowWalkExtraAngle = 0.4f;
	AccuracyPreset->Settings.AirborneExtraAngle = 7.0f;
	AccuracyPreset->Settings.MovementThreshold = 9.0f;

	UWeaponKitDefinitionData* Rifle = NewObject<UWeaponKitDefinitionData>(GetTransientPackage());
	UWeaponKitDefinitionData* SMG = NewObject<UWeaponKitDefinitionData>(GetTransientPackage());
	for (UWeaponKitDefinitionData* Definition : { Rifle, SMG })
	{
		Definition->ConfigurationVersion = 1;
		Definition->HipAccuracy.bUsePostureSettings = true;
		Definition->HipAccuracy.PosturePreset = AccuracyPreset;
	}

	TestTrue(TEXT("Both weapons resolve their shared accuracy preset"),
		NearlyEqual(Rifle->GetSpreadProfile(false).MovingSpreadAngle, 2.5f) &&
		NearlyEqual(SMG->GetSpreadProfile(false).MovingSpreadAngle, 2.5f));
	AccuracyPreset->Settings.MovingExtraAngle = 3.5f;
	TestTrue(TEXT("Changing one preset updates both weapon profiles"),
		NearlyEqual(Rifle->GetSpreadProfile(false).MovingSpreadAngle, 3.5f) &&
		NearlyEqual(SMG->GetSpreadProfile(false).MovingSpreadAngle, 3.5f));

	Rifle->HipAccuracy.bOverridePosture = true;
	Rifle->HipAccuracy.PostureOverride.MovingExtraAngle = 1.0f;
	AccuracyPreset->Settings.MovingExtraAngle = 4.25f;
	TestTrue(TEXT("A weapon override is isolated from shared preset edits"),
		NearlyEqual(Rifle->GetSpreadProfile(false).MovingSpreadAngle, 1.0f) &&
		NearlyEqual(SMG->GetSpreadProfile(false).MovingSpreadAngle, 4.25f));

	UWeaponKitCameraFeedbackPreset* CameraPreset = NewObject<UWeaponKitCameraFeedbackPreset>(GetTransientPackage());
	CameraPreset->ReturnSpeed = 18.0f;
	CameraPreset->SmoothKickDuration = 0.08f;
	CameraPreset->ImmediateKickFraction = 0.35f;
	UWeaponKitRecoilData* RifleRecoil = NewObject<UWeaponKitRecoilData>(GetTransientPackage());
	UWeaponKitRecoilData* SMGRecoil = NewObject<UWeaponKitRecoilData>(GetTransientPackage());
	RifleRecoil->CameraFeedbackPreset = CameraPreset;
	SMGRecoil->CameraFeedbackPreset = CameraPreset;
	TestTrue(TEXT("Both weapons read the shared camera preset"),
		NearlyEqual(RifleRecoil->GetCameraReturnSpeed(), 18.0f) &&
		NearlyEqual(SMGRecoil->GetSmoothKickDuration(), 0.08f) &&
		NearlyEqual(SMGRecoil->GetImmediateKickFraction(), 0.35f));

	CameraPreset->ReturnSpeed = 24.0f;
	RifleRecoil->bOverrideCameraFeedback = true;
	RifleRecoil->CameraFeedbackOverrides.ReturnSpeed = 6.0f;
	RifleRecoil->CameraFeedbackOverrides.SmoothKickDuration = 0.02f;
	RifleRecoil->CameraFeedbackOverrides.ImmediateKickFraction = 0.1f;
	TestTrue(TEXT("Camera override stays local while the other weapon follows the preset"),
		NearlyEqual(RifleRecoil->GetCameraReturnSpeed(), 6.0f) &&
		NearlyEqual(RifleRecoil->GetSmoothKickDuration(), 0.02f) &&
		NearlyEqual(SMGRecoil->GetCameraReturnSpeed(), 24.0f));

	UWeaponKitRecoilData* LegacyRecoil = NewObject<UWeaponKitRecoilData>(GetTransientPackage());
	UWeaponKitCameraFeedbackPreset* MatchingPreset = NewObject<UWeaponKitCameraFeedbackPreset>(GetTransientPackage());
	TestTrue(TEXT("Exact legacy camera settings can be migrated to a shared preset"),
		LegacyRecoil->MigrateCameraFeedbackToPreset(MatchingPreset));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeaponKitTotalSpreadProjectionTest,
	"WeaponKit.Runtime.TotalSpreadProjectionUsesShotProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeaponKitTotalSpreadProjectionTest::RunTest(const FString& Parameters)
{
	FWeaponKitTestWorld TestWorld;
	if (!TestNotNull(TEXT("Transient world created"), TestWorld.Get()))
	{
		return false;
	}

	UWeaponKitDefinitionData* Definition = NewObject<UWeaponKitDefinitionData>(GetTransientPackage());
	Definition->ConfigurationVersion = 1;
	Definition->FireInterval = 0.1f;
	Definition->HipAccuracy.bUsePostureSettings = true;
	Definition->HipAccuracy.BaseSpreadAngle = 2.0f;
	Definition->HipAccuracy.PostureOverride.MovingExtraAngle = 3.0f;
	Definition->HipAccuracy.PostureOverride.SlowWalkExtraAngle = 1.0f;
	Definition->HipAccuracy.PostureOverride.AirborneExtraAngle = 7.0f;
	Definition->HipAccuracy.PostureOverride.MovementThreshold = 10.0f;
	Definition->HipAccuracy.MaxFiringExtraAngle = 4.0f;
	Definition->HipAccuracy.ShotsToMaximum = 2;
	Definition->HipAccuracy.FullRecoverySeconds = 1.0f;
	Definition->HipAccuracy.bPreserveMigratedFiring = false;
	Definition->InitialMagazineAmmo = 10;
	Definition->MagazineCapacity = 10;
	AWeaponKitRuntime* Weapon = SpawnConfiguredWeapon(TestWorld.Get(), Definition);
	if (!TestNotNull(TEXT("Unified accuracy weapon spawned"), Weapon))
	{
		return false;
	}

	TestTrue(TEXT("Projection uses base plus moving posture from the real profile"),
		NearlyEqual(Weapon->GetCurrentTotalSpreadAngle(15.0f, false, false, false), 5.0f));
	TestTrue(TEXT("Airborne posture adds alongside horizontal movement and the same base"),
		NearlyEqual(Weapon->GetCurrentTotalSpreadAngle(15.0f, true, false, false), 12.0f));
	TestTrue(TEXT("Slow walking selects its separate extra when movement threshold is crossed"),
		NearlyEqual(Weapon->GetCurrentTotalSpreadAngle(15.0f, false, false, true), 3.0f));

	TestTrue(TEXT("A successful shot is accepted"), Weapon->TryCommitFire());
	const FWeaponKitSpreadProfile ActiveProfile = Definition->GetSpreadProfile(false, Weapon->GetEffectiveFireInterval());
	TestTrue(TEXT("The runtime and getter use the additive shot profile"), ActiveProfile.bUseAdditiveStateSpread);
	TestTrue(TEXT("Shot bloom is projected on top of the same base and movement spread"),
		NearlyEqual(Weapon->GetCurrentTotalSpreadAngle(15.0f, false, false, false), 7.2f));

	Definition->bUseAimingSpread = true;
	Definition->AimAccuracy.bUsePostureSettings = true;
	Definition->AimAccuracy.BaseSpreadAngle = 0.5f;
	Definition->AimAccuracy.PostureOverride.MovingExtraAngle = 0.25f;
	Definition->AimAccuracy.PostureOverride.SlowWalkExtraAngle = 0.0f;
	Definition->AimAccuracy.PostureOverride.AirborneExtraAngle = 1.0f;
	Definition->AimAccuracy.PostureOverride.MovementThreshold = 10.0f;
	Definition->AimAccuracy.MaxFiringExtraAngle = 0.0f;
	Weapon->SetAiming(true);
	TestTrue(TEXT("Independent aim precision projects its active profile"),
		NearlyEqual(Weapon->GetCurrentTotalSpreadAngle(15.0f, false, false, false), 0.75f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
