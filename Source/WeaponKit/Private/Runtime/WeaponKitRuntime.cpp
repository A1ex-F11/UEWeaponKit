// 武器运行时：间隔、弹药、换弹、后坐序列和散布采样。不发射线，也不改共享配置资产。
#include "Runtime/WeaponKitRuntime.h"
#include "Data/WeaponKitBallisticsData.h"
#include "Data/WeaponKitRecoilData.h"
#include "Engine/World.h"
#include "Math/WeaponKitAccuracyMath.h"
#include "Math/FiringSpreadMath.h"
#include "Math/ShotRecoilMath.h"

namespace
{
// 非有限数和负数都收成 0，避免间隔或散布变成 NaN。
float WeaponKitNonNegative(float Value)
{
	return FMath::IsFinite(Value) ? FMath::Max(0.0f, Value) : 0.0f;
}

// 没有配置资产时使用的腰射锥角。连续射击散布保持关闭。
FWeaponKitSpreadProfile WeaponKitFallbackSpread()
{
	FWeaponKitSpreadProfile Profile;
	Profile.StationarySpreadAngle = 0.0f;
	Profile.MovingSpreadAngle = 3.0f;
	Profile.JumpSpreadAngle = 12.0f;
	Profile.SlowWalkSpreadAngle = 1.0f;
	Profile.MovementSpreadSpeedThreshold = 10.0f;
	return Profile;
}
}

// 不 Tick。默认只允许半自动。
AWeaponKitRuntime::AWeaponKitRuntime()
{
	PrimaryActorTick.bCanEverTick = false;
	AllowedFireModes = { EWeaponKitFireMode::SemiAutomatic };
}

// 只记录瞄准状态。镜头组件需要角色另外调用。
void AWeaponKitRuntime::SetAiming(bool bAiming)
{
	bOwnerAiming = bAiming;
}

// 编辑器改属性时重套配置，不重置弹药。
void AWeaponKitRuntime::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyDefinitionSettings();
}

// 首次生成时写入初始弹药和默认开火模式；读档标记已经置位则跳过。
void AWeaponKitRuntime::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ApplyDefinitionSettings();
	if (WeaponDefinition && !bInitialDefinitionApplied)
	{
		// 读档或捡枪已经写过弹药时，不用资产上的初始装弹覆盖。
		if (!bRestoredAmmo)
		{
			CurrentAmmo = FMath::Clamp(WeaponDefinition->InitialMagazineAmmo, 0, MagazineCapacity);
			ReserveAmmo = FMath::Max(0, WeaponDefinition->InitialReserveAmmo);
		}
		if (!bRestoredFireMode)
		{
			CurrentFireMode = WeaponDefinition->GetInitialFireMode();
		}
		bInitialDefinitionApplied = true;
	}
}

// 复制容量、间隔、后坐和换弹开关。当前弹药和开火模式保持不动。
void AWeaponKitRuntime::ApplyDefinitionSettings()
{
	const UWeaponKitDefinitionData* Definition = WeaponDefinition;
	if (!Definition)
	{
		return;
	}

	WeaponDisplayName = Definition->WeaponDisplayName;
	AllowedFireModes = Definition->AllowedFireModes;
	MagazineCapacity = FMath::Max(1, Definition->MagazineCapacity);
	FireInterval = FMath::Max(0.001f, WeaponKitNonNegative(Definition->FireInterval));
	BurstShotInterval = WeaponKitNonNegative(Definition->BurstShotInterval);
	RecoilData = Definition->RecoilData;
	bLockReloadAmountAtStart = Definition->bLockReloadAmountAtStart;
	bReloadOneRoundAtATime = Definition->UsesPerRoundReload();
	bUseBatchReloadForFullEmptyReload = Definition->UsesEmptyBatchReload();
}

// 没有配置时连发恢复间隔为 0。
float AWeaponKitRuntime::GetBurstRecoveryInterval() const
{
	return WeaponDefinition ? WeaponKitNonNegative(WeaponDefinition->BurstRecoveryInterval) : 0.0f;
}

// 返回配置里的弹道子对象，可能为空。
UWeaponKitBallisticsData* AWeaponKitRuntime::GetBallistics() const
{
	return WeaponDefinition ? WeaponDefinition->BallisticsData : nullptr;
}

// 优先读配置；配置被清空时退回上次复制的名字。
FText AWeaponKitRuntime::GetWeaponDisplayName() const
{
	return WeaponDefinition ? WeaponDefinition->WeaponDisplayName : WeaponDisplayName;
}

// 置读档标记，阻止随后的首次初始化改回默认模式。
void AWeaponKitRuntime::RestoreFireMode(EWeaponKitFireMode SavedFireMode)
{
	bRestoredFireMode = true;
	CurrentFireMode = SavedFireMode;
}

// 弹匣没弹时失败，调用方不应再生成弹道。
bool AWeaponKitRuntime::TryConsumeAmmo()
{
	if (CurrentAmmo <= 0)
	{
		return false;
	}
	--CurrentAmmo;
	return true;
}

// 只接受正数备弹。
bool AWeaponKitRuntime::AddReserveAmmo(int32 Amount)
{
	if (Amount <= 0)
	{
		return false;
	}
	ReserveAmmo += Amount;
	return true;
}

// 立刻装满能装的部分。逐发换弹进行中时拒绝。
bool AWeaponKitRuntime::ReloadAmmo()
{
	if (bCurrentReloadUsesPerRound && bIsReloading)
	{
		return false;
	}

	const int32 MissingAmmo = MagazineCapacity - CurrentAmmo;
	if (MissingAmmo <= 0 || ReserveAmmo <= 0)
	{
		return false;
	}

	const int32 ReloadAmount = FMath::Min(ReserveAmmo, MissingAmmo);
	CurrentAmmo += ReloadAmount;
	ReserveAmmo -= ReloadAmount;
	return true;
}

// 直接写弹药并重置后坐。进行中的逐发换弹会被取消。
void AWeaponKitRuntime::RestoreAmmo(int32 NewCurrentAmmo, int32 NewReserveAmmo)
{
	bRestoredAmmo = true;
	ApplyDefinitionSettings();
	ResetRecoilPattern();
	if (bReloadOneRoundAtATime && bIsReloading)
	{
		CancelReload();
	}
	CurrentAmmo = FMath::Clamp(NewCurrentAmmo, 0, MagazineCapacity);
	ReserveAmmo = FMath::Max(NewReserveAmmo, 0);
}

// 检查换弹和间隔、扣一发、缓存这一发的散布和后坐。不发射线。
bool AWeaponKitRuntime::TryCommitFire()
{
	bHasCommittedSpreadProfile = false;
	bCommittedIgnoreShotRecoil = false;
	CommittedFiringSpread = 0.0f;
	ClearPendingRecoilKick();
	bHasCommittedShotDirection = false;
	if (bIsReloading)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime < NextFireTime)
	{
		return false;
	}
	if (!TryConsumeAmmo())
	{
		return false;
	}

	CommittedSpreadProfile = GetSpreadProfile(bOwnerAiming);
	bHasCommittedSpreadProfile = true;
	// 瞄准时提交的这一发，采样方向时不再叠加累计弹道后坐。
	bCommittedIgnoreShotRecoil = bOwnerAiming;
	CacheCommittedFiringSpread(CurrentTime);
	CacheCommittedRecoil(CurrentTime);
	NextFireTime = CurrentTime + GetEffectiveFireInterval();
	OnFireCommitted.Broadcast();
	return true;
}

// 三连发且连发间隔大于 0 时用连发间隔，否则用普通射击间隔。
float AWeaponKitRuntime::GetEffectiveFireInterval() const
{
	if (CurrentFireMode == EWeaponKitFireMode::Burst && BurstShotInterval > 0.0f)
	{
		return BurstShotInterval;
	}
	return FireInterval;
}

// 取出并清掉缓存冲量。每发只能取一次。
void AWeaponKitRuntime::GenerateRecoilKick(float& PitchKick, float& YawKick, bool& bGenerated)
{
	PitchKick = 0.0f;
	YawKick = 0.0f;
	bGenerated = false;
	if (!bHasPendingRecoilKick)
	{
		return;
	}

	const float CommittedPitchKick = CachedRecoilPitchKick;
	const float CommittedYawKick = CachedRecoilYawKick;
	ClearPendingRecoilKick();
	if (!FMath::IsFinite(CommittedPitchKick) || !FMath::IsFinite(CommittedYawKick))
	{
		return;
	}

	PitchKick = CommittedPitchKick;
	YawKick = CommittedYawKick;
	bGenerated = true;
}

// 清空两套连续射击散布、弹道累计和序列下标。
void AWeaponKitRuntime::ResetRecoilPattern()
{
	AccumulatedAimFiringSpread = 0.0f;
	bHasAimSpreadShot = false;
	bHasCommittedSpreadProfile = false;
	bCommittedIgnoreShotRecoil = false;
	AccumulatedFiringSpread = 0.0f;
	CommittedFiringSpread = 0.0f;
	bHasSpreadShot = false;
	AccumulatedShotRecoil = FVector2D::ZeroVector;
	CommittedShotRecoil = FVector2D::ZeroVector;
	bHasCommittedShotDirection = false;
	RecoilPatternIndex = 0;
	LastSuccessfulShotTime = 0.0f;
	bHasSuccessfulShotTime = false;
	ClearPendingRecoilKick();
}

// 按停火时间重置或恢复序列，再采样这一发镜头冲量。
void AWeaponKitRuntime::CacheCommittedRecoil(float SuccessfulShotTime)
{
	const bool bUseFixedPattern = RecoilData && RecoilData->bUseFixedPattern;
	const bool bIndependent = bUseFixedPattern && RecoilData->bUseIndependentShotRecoil;
	if (bUseFixedPattern && bHasSuccessfulShotTime)
	{
		const float ResetDelay = FMath::IsFinite(RecoilData->PatternResetDelay)
			? FMath::Max(0.0f, RecoilData->PatternResetDelay)
			: 0.0f;
		const float TimeSinceLastShot = SuccessfulShotTime - LastSuccessfulShotTime;
		// 停火达到重置时间：序列回到第一项，弹道累计清零。
		if (FMath::IsFinite(TimeSinceLastShot) && TimeSinceLastShot >= ResetDelay)
		{
			RecoilPatternIndex = 0;
			AccumulatedShotRecoil = FVector2D::ZeroVector;
		}
		else if (bIndependent)
		{
			const float Delay = FMath::IsFinite(RecoilData->RecoveryDelay) ? FMath::Max(0.0f, RecoilData->RecoveryDelay) : 0.0f;
			const float Speed = FMath::IsFinite(RecoilData->ShotRecoilRecoverySpeed)
				? FMath::Max(0.0f, RecoilData->ShotRecoilRecoverySpeed)
				: 0.0f;
			AccumulatedShotRecoil = FWeaponKitShotRecoilMath::Recover(AccumulatedShotRecoil, TimeSinceLastShot, Delay, Speed);
		}
	}
	// 非分离模式不保留弹道后坐，射击方向只来自调用方视线。
	if (!bIndependent)
	{
		AccumulatedShotRecoil = FVector2D::ZeroVector;
	}
	CommittedShotRecoil = bIndependent ? AccumulatedShotRecoil : FVector2D::ZeroVector;
	bHasCommittedShotDirection = true;
	LastSuccessfulShotTime = SuccessfulShotTime;
	bHasSuccessfulShotTime = true;

	if (!RecoilData)
	{
		return;
	}

	if (RecoilData->bUseFixedPattern)
	{
		const int32 PatternSize = RecoilData->FixedPattern.Num();
		if (PatternSize <= 0)
		{
			RecoilPatternIndex = 0;
			return;
		}

		RecoilPatternIndex = FMath::Clamp(RecoilPatternIndex, 0, PatternSize - 1);
		const FVector2D& PatternKick = RecoilData->FixedPattern[RecoilPatternIndex];
		if (FMath::IsFinite(PatternKick.X) && FMath::IsFinite(PatternKick.Y))
		{
			const float PitchScale = FMath::IsFinite(RecoilData->FixedPatternPitchScale)
				? FMath::Max(0.0f, RecoilData->FixedPatternPitchScale)
				: 0.0f;
			const float YawScale = FMath::IsFinite(RecoilData->FixedPatternYawScale)
				? FMath::Max(0.0f, RecoilData->FixedPatternYawScale)
				: 0.0f;
			const float ScaledPitch = FMath::Max(0.0f, PatternKick.X) * PitchScale;
			const float ScaledYaw = PatternKick.Y * YawScale;
			if (FMath::IsFinite(ScaledPitch) && FMath::IsFinite(ScaledYaw))
			{
				CachedRecoilPitchKick = ScaledPitch;
				CachedRecoilYawKick = ScaledYaw;
				bHasPendingRecoilKick = true;
				if (bIndependent)
				{
					// 分离模式把这一发加进后续弹道。俯仰限制在 0 到 60 度，偏航限制在 ±60 度。
					AccumulatedShotRecoil.X = FMath::Clamp(AccumulatedShotRecoil.X + ScaledPitch, 0.0, 60.0);
					AccumulatedShotRecoil.Y = FMath::Clamp(AccumulatedShotRecoil.Y + ScaledYaw, -60.0, 60.0);
				}
			}
		}
		AdvanceRecoilPatternIndex(PatternSize);
		return;
	}

	if (!FMath::IsFinite(RecoilData->VerticalKickMin) ||
		!FMath::IsFinite(RecoilData->VerticalKickMax) ||
		!FMath::IsFinite(RecoilData->HorizontalKickMax))
	{
		return;
	}

	const float RawVerticalMin = FMath::Min(RecoilData->VerticalKickMin, RecoilData->VerticalKickMax);
	const float RawVerticalMax = FMath::Max(RecoilData->VerticalKickMin, RecoilData->VerticalKickMax);
	const float VerticalMin = FMath::Max(0.0f, RawVerticalMin);
	const float VerticalMax = FMath::Max(VerticalMin, RawVerticalMax);
	const float HorizontalMax = FMath::Abs(RecoilData->HorizontalKickMax);
	const float PitchKick = FMath::FRandRange(VerticalMin, VerticalMax);
	const float YawKick = FMath::FRandRange(-HorizontalMax, HorizontalMax);
	if (!FMath::IsFinite(PitchKick) || !FMath::IsFinite(YawKick))
	{
		return;
	}

	CachedRecoilPitchKick = PitchKick;
	CachedRecoilYawKick = YawKick;
	bHasPendingRecoilKick = true;
}

// 未到末项就前进。LoopStartIndex 小于 0 时停在最后一项。
void AWeaponKitRuntime::AdvanceRecoilPatternIndex(int32 PatternSize)
{
	if (PatternSize <= 0)
	{
		RecoilPatternIndex = 0;
		return;
	}

	const int32 NextIndex = RecoilPatternIndex + 1;
	if (NextIndex < PatternSize)
	{
		RecoilPatternIndex = NextIndex;
		return;
	}
	if (!RecoilData || RecoilData->LoopStartIndex < 0)
	{
		RecoilPatternIndex = PatternSize - 1;
		return;
	}
	RecoilPatternIndex = FMath::Clamp(RecoilData->LoopStartIndex, 0, PatternSize - 1);
}

// 丢掉还没被 GenerateRecoilKick 取走的冲量。
void AWeaponKitRuntime::ClearPendingRecoilKick()
{
	CachedRecoilPitchKick = 0.0f;
	CachedRecoilYawKick = 0.0f;
	bHasPendingRecoilKick = false;
}

// 用提交时的散布配置采样方向。跳跃优先于下蹲、静止、慢走和普通移动。
void AWeaponKitRuntime::GenerateShotDirection(
	const FVector& TraceDirection,
	float CurrentHorizontalSpeed,
	bool bIsFalling,
	bool bIsCrouched,
	bool bIsSlowWalking,
	FVector& ShotDirection,
	bool& bGenerated)
{
	ShotDirection = FVector::ZeroVector;
	bGenerated = false;
	FVector BaseDirection = TraceDirection.GetSafeNormal();
	if (BaseDirection.IsNearlyZero())
	{
		return;
	}

	if (RecoilData && RecoilData->bUseFixedPattern && RecoilData->bUseIndependentShotRecoil)
	{
		if (!bHasCommittedShotDirection)
		{
			return;
		}
		// 腰射提交的那一发才把冻结的弹道后坐加到方向上。
		if (!bCommittedIgnoreShotRecoil)
		{
			BaseDirection = FWeaponKitShotRecoilMath::Apply(BaseDirection, CommittedShotRecoil);
		}
		if (BaseDirection.IsNearlyZero())
		{
			return;
		}
	}

	const FWeaponKitSpreadProfile Profile = bHasCommittedSpreadProfile
		? CommittedSpreadProfile
		: GetSpreadProfile(bOwnerAiming);
	const float SafeSpreadAngle = FWeaponKitAccuracyMath::ResolveTotalSpreadAngle(
		Profile,
		CurrentHorizontalSpeed,
		bIsFalling,
		bIsCrouched,
		bIsSlowWalking,
		CommittedFiringSpread);
	ShotDirection = FMath::VRandCone(BaseDirection, FMath::DegreesToRadians(SafeSpreadAngle));
	bGenerated = true;
}

// 在允许列表里循环。不足两项或当前模式不在列表里时收束到第一项或保持不变。
void AWeaponKitRuntime::CycleFireMode()
{
	if (AllowedFireModes.Num() < 2)
	{
		return;
	}
	const int32 CurrentIndex = AllowedFireModes.Find(CurrentFireMode);
	if (CurrentIndex == INDEX_NONE)
	{
		CurrentFireMode = AllowedFireModes[0];
		return;
	}
	CurrentFireMode = AllowedFireModes[(CurrentIndex + 1) % AllowedFireModes.Num()];
}

// 弹匣已满、没有备弹或已在换弹时失败。开始时重置后坐。
bool AWeaponKitRuntime::TryStartReload()
{
	if (bIsReloading || CurrentAmmo >= MagazineCapacity || ReserveAmmo <= 0)
	{
		return false;
	}

	PendingReloadAmount = (bLockReloadAmountAtStart || bReloadOneRoundAtATime)
		? FMath::Min(ReserveAmmo, MagazineCapacity - CurrentAmmo)
		: 0;
	// 空仓且备弹足以装满时，允许把逐发换弹改成一次装入。
	bCurrentReloadUsesPerRound = bReloadOneRoundAtATime &&
		!(bUseBatchReloadForFullEmptyReload && CurrentAmmo == 0 && PendingReloadAmount == MagazineCapacity);
	ReloadRequestId = (ReloadRequestId == MAX_int32) ? 1 : ReloadRequestId + 1;
	bIsReloading = true;
	ResetRecoilPattern();
	OnReloadStarted.Broadcast();
	return true;
}

// 整匣换弹在这里装入计划弹药。逐发换弹只清状态，子弹已经逐发装入。
bool AWeaponKitRuntime::FinishReload()
{
	if (!bIsReloading)
	{
		return false;
	}

	if (!bCurrentReloadUsesPerRound)
	{
		if (bLockReloadAmountAtStart || bReloadOneRoundAtATime)
		{
			const int32 MissingAmmo = FMath::Max(0, MagazineCapacity - CurrentAmmo);
			const int32 AvailableAmmo = FMath::Max(0, ReserveAmmo);
			const int32 AmountToLoad = FMath::Min(
				FMath::Max(0, PendingReloadAmount),
				FMath::Min(MissingAmmo, AvailableAmmo));
			CurrentAmmo += AmountToLoad;
			ReserveAmmo -= AmountToLoad;
		}
		else
		{
			ReloadAmmo();
		}
	}

	PendingReloadAmount = 0;
	bCurrentReloadUsesPerRound = false;
	bIsReloading = false;
	return true;
}

// 结束换弹状态，不装入剩余计划。已逐发装入的子弹保留。
bool AWeaponKitRuntime::CancelReload()
{
	if (!bIsReloading)
	{
		return false;
	}
	PendingReloadAmount = 0;
	bCurrentReloadUsesPerRound = false;
	bIsReloading = false;
	return true;
}

// 编号不匹配说明是上一次换弹的动画通知，拒绝装入。
bool AWeaponKitRuntime::TryCommitReloadRound(int32 ExpectedReloadRequestId)
{
	if (!bIsReloading || !bCurrentReloadUsesPerRound)
	{
		return false;
	}
	if (ExpectedReloadRequestId <= 0 || ExpectedReloadRequestId != ReloadRequestId)
	{
		return false;
	}
	if (PendingReloadAmount <= 0 || CurrentAmmo >= MagazineCapacity || ReserveAmmo <= 0)
	{
		return false;
	}

	++CurrentAmmo;
	--ReserveAmmo;
	--PendingReloadAmount;
	return true;
}

// 读类默认对象上的展示名，不需要生成实例。
FText AWeaponKitRuntime::GetDisplayNameForWeaponClass(TSubclassOf<AWeaponKitRuntime> WeaponClass)
{
	const AWeaponKitRuntime* Defaults = WeaponClass.GetDefaultObject();
	if (!Defaults)
	{
		return FText::GetEmpty();
	}
	return Defaults->WeaponDefinition ? Defaults->WeaponDefinition->WeaponDisplayName : Defaults->WeaponDisplayName;
}

// 没有存档标记时保持资产初始弹药和模式。
void AWeaponKitRuntime::RestorePickupRuntimeState(const FWeaponKitPickupState& State)
{
	if (!State.bHasSavedState)
	{
		return;
	}
	RestoreAmmo(State.CurrentAmmo, State.ReserveAmmo);
	RestoreFireMode(State.FireMode);
}

// 抓取当前弹药和开火模式。首抽标记由携带者传入。
FWeaponKitPickupState AWeaponKitRuntime::CapturePickupRuntimeState(bool bInitialDrawConsumed) const
{
	FWeaponKitPickupState State;
	State.bHasSavedState = true;
	State.CurrentAmmo = GetCurrentAmmo();
	State.ReserveAmmo = GetReserveAmmo();
	State.FireMode = GetFireMode();
	State.bInitialDrawConsumed = bInitialDrawConsumed;
	return State;
}

// 按当前是否瞄准，取对应那一套已经恢复过的连续射击散布。
float AWeaponKitRuntime::GetCurrentFiringSpread() const
{
	return GetFiringSpreadForProfile(bOwnerAiming);
}

float AWeaponKitRuntime::GetCurrentTotalSpreadAngle(
	float CurrentHorizontalSpeed,
	bool bIsFalling,
	bool bIsCrouched,
	bool bIsSlowWalking) const
{
	const FWeaponKitSpreadProfile Profile = GetSpreadProfile(bOwnerAiming);
	return FWeaponKitAccuracyMath::ResolveTotalSpreadAngle(
		Profile,
		CurrentHorizontalSpeed,
		bIsFalling,
		bIsCrouched,
		bIsSlowWalking,
		GetFiringSpreadForProfile(bOwnerAiming));
}

// 没有配置时退回代码默认的腰射形状。
FWeaponKitSpreadProfile AWeaponKitRuntime::GetSpreadProfile(bool bAiming) const
{
	if (WeaponDefinition)
	{
		return WeaponDefinition->GetSpreadProfile(bAiming, GetEffectiveFireInterval());
	}
	return WeaponKitFallbackSpread();
}

// 未启用连续射击散布，或这套还没开过枪时返回 0。
float AWeaponKitRuntime::GetFiringSpreadForProfile(bool bAiming) const
{
	const bool bAimProfile = bAiming && WeaponDefinition && WeaponDefinition->UsesSeparateAimingSpread();
	const FWeaponKitSpreadProfile Profile = GetSpreadProfile(bAimProfile);
	const bool bHasShot = bAimProfile ? bHasAimSpreadShot : bHasSpreadShot;
	if (!Profile.bUseFiringSpread || !bHasShot || !GetWorld())
	{
		return 0.0f;
	}
	return FWeaponKitFiringSpreadMath::Recover(
		bAimProfile ? AccumulatedAimFiringSpread : AccumulatedFiringSpread,
		GetWorld()->GetTimeSeconds() - (bAimProfile ? LastAimSpreadShotTime : LastSpreadShotTime),
		Profile.SpreadRecoveryDelay,
		Profile.SpreadRecoverySpeed,
		Profile.MaxFiringSpread);
}

// 这一发使用恢复后的当前散布，然后两套累计各自增加，切换瞄准不清另一套。
void AWeaponKitRuntime::CacheCommittedFiringSpread(float SuccessfulShotTime)
{
	CommittedFiringSpread = GetCurrentFiringSpread();
	const float AimBefore = GetFiringSpreadForProfile(true);
	const float HipBefore = GetFiringSpreadForProfile(false);
	// Mode 0 是腰射累计，Mode 1 是瞄准累计。
	for (int32 Mode = 0; Mode < 2; ++Mode)
	{
		const FWeaponKitSpreadProfile Profile = GetSpreadProfile(Mode == 1);
		float& Accumulated = Mode == 1 ? AccumulatedAimFiringSpread : AccumulatedFiringSpread;
		float& LastTime = Mode == 1 ? LastAimSpreadShotTime : LastSpreadShotTime;
		bool& bHasShot = Mode == 1 ? bHasAimSpreadShot : bHasSpreadShot;
		Accumulated = Profile.bUseFiringSpread
			? FWeaponKitFiringSpreadMath::AddShot(Mode == 1 ? AimBefore : HipBefore, Profile.SpreadPerShot, Profile.MaxFiringSpread)
			: 0.0f;
		LastTime = SuccessfulShotTime;
		bHasShot = Profile.bUseFiringSpread;
	}
}
