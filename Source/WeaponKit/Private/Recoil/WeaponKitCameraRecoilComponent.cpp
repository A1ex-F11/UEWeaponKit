// 镜头和控制器后坐。瞄准写控制旋转；分离模式的腰射只偏摄像机。
#include "Recoil/WeaponKitCameraRecoilComponent.h"
#include "Camera/CameraComponent.h"
#include "Data/WeaponKitRecoilData.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

namespace
{
// 非有限数用 Fallback，负数收成 0。
float WeaponKitSafeNonNegative(float Value, float Fallback = 0.0f)
{
	return FMath::IsFinite(Value) ? FMath::Max(0.0f, Value) : Fallback;
}
}

// 可以 Tick，但默认关闭。只有还有尾巴或镜头偏移时才打开。
UWeaponKitCameraRecoilComponent::UWeaponKitCameraRecoilComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

// 状态没变时保留正在播放的尾巴。真正切换时清掉未施加部分，已写入的角度不动。
void UWeaponKitCameraRecoilComponent::SetAiming(bool bInAiming)
{
	if (bAiming == bInAiming)
	{
		return;
	}
	ClearRecoil();
	bAiming = bInAiming;
}

// 唯一施加入口。无效的后坐资产直接忽略。
void UWeaponKitCameraRecoilComponent::ApplyRecoil(
	float PitchKick,
	float YawKick,
	UWeaponKitRecoilData* RecoilSettings,
	UCameraComponent* RecoilCamera)
{
	if (!IsValid(RecoilSettings))
	{
		return;
	}

	// 瞄准：冲量乘瞄准倍率后写入控制旋转，下一发采样到同一条视线。
	if (bAiming)
	{
		if (!FMath::IsFinite(PitchKick) || !FMath::IsFinite(YawKick))
		{
			return;
		}
		ClearIndependentVisual();
		ActiveRecoveryDelay = WeaponKitSafeNonNegative(RecoilSettings->RecoveryDelay);
		ActiveRecoverySpeed = WeaponKitSafeNonNegative(RecoilSettings->ShotRecoilRecoverySpeed);
		ActiveRecoveryRatio = FMath::IsFinite(RecoilSettings->RecoveryPitchRatio)
			? FMath::Clamp(RecoilSettings->RecoveryPitchRatio, 0.0f, 1.0f)
			: 0.0f;
		const float Scale = WeaponKitSafeNonNegative(RecoilSettings->AimingRecoilScale);
		QueueKick(
			PitchKick * Scale,
			YawKick * Scale,
			WeaponKitSafeNonNegative(RecoilSettings->SmoothKickDuration),
			FMath::IsFinite(RecoilSettings->ImmediateKickFraction)
				? FMath::Clamp(RecoilSettings->ImmediateKickFraction, 0.0f, 1.0f)
				: 1.0f,
			false);
		return;
	}

	// 只有固定序列并且开启分离时，腰射才不改控制旋转。
	const bool bIndependentVisual = RecoilSettings->bUseFixedPattern && RecoilSettings->bUseIndependentShotRecoil;
	if (!bIndependentVisual)
	{
		ClearIndependentVisual();
		// 随机腰射走控制器输入，并受组件上的累计上限约束。
		if (!RecoilSettings->bUseFixedPattern)
		{
			ActiveRecoveryDelay = WeaponKitSafeNonNegative(RecoveryDelay);
			ActiveRecoverySpeed = WeaponKitSafeNonNegative(RecoverySpeed);
			ActiveRecoveryRatio = FMath::Clamp(RecoveryPitchRatio, 0.0f, 1.0f);
			QueueKick(PitchKick, YawKick, 0.0f, 1.0f, true);
			return;
		}

		ActiveRecoveryDelay = WeaponKitSafeNonNegative(RecoilSettings->RecoveryDelay);
		ActiveRecoverySpeed = WeaponKitSafeNonNegative(RecoilSettings->RecoverySpeed);
		ActiveRecoveryRatio = FMath::IsFinite(RecoilSettings->RecoveryPitchRatio)
			? FMath::Clamp(RecoilSettings->RecoveryPitchRatio, 0.0f, 1.0f)
			: 0.0f;
		QueueKick(
			PitchKick,
			YawKick,
			WeaponKitSafeNonNegative(RecoilSettings->SmoothKickDuration),
			FMath::IsFinite(RecoilSettings->ImmediateKickFraction)
				? FMath::Clamp(RecoilSettings->ImmediateKickFraction, 0.0f, 1.0f)
				: 1.0f,
			false);
		return;
	}

	if (!IsValid(RecoilCamera) || !FMath::IsFinite(PitchKick) || !FMath::IsFinite(YawKick))
	{
		return;
	}
	if (!bIndependentVisualMode || VisualCamera.Get() != RecoilCamera)
	{
		ClearRecoil();
		bIndependentVisualMode = true;
		VisualCamera = RecoilCamera;
	}

	const float Scale = FMath::IsFinite(RecoilSettings->CameraFeedbackScale)
		? FMath::Max(0.0f, RecoilSettings->CameraFeedbackScale)
		: 0.0f;
	VisualReturnSpeed = FMath::Max(0.1f, WeaponKitSafeNonNegative(RecoilSettings->CameraReturnSpeed, 12.0f));
	const FVector2D Kick(FMath::Max(0.0f, PitchKick) * Scale, YawKick * Scale);
	const float Duration = WeaponKitSafeNonNegative(RecoilSettings->SmoothKickDuration);
	const float Immediate = FMath::IsFinite(RecoilSettings->ImmediateKickFraction)
		? FMath::Clamp(RecoilSettings->ImmediateKickFraction, 0.0f, 1.0f)
		: 1.0f;
	VisualOffset += Duration <= KINDA_SMALL_NUMBER ? Kick : Kick * Immediate;
	if (Duration > KINDA_SMALL_NUMBER)
	{
		FWeaponKitRecoilKickEnvelope Tail;
		Tail.Total = Kick * (1.0f - Immediate);
		Tail.Duration = Duration;
		if (!Tail.Total.IsNearlyZero())
		{
			VisualKicks.Add(Tail);
		}
	}
	WriteVisualOffset();
	if (!VisualKicks.IsEmpty() || !VisualOffset.IsNearlyZero())
	{
		SetComponentTickEnabled(true);
	}
}

// 没有控制器时无法写视角。立即部分马上施加，其余放进平滑尾巴。
void UWeaponKitCameraRecoilComponent::QueueKick(
	float PitchKick,
	float YawKick,
	float Duration,
	float ImmediateFraction,
	bool bUseCumulativeLimits)
{
	APawn* PawnOwner = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	if (!PawnOwner || !PawnOwner->GetController() || !World || !FMath::IsFinite(PitchKick) || !FMath::IsFinite(YawKick))
	{
		return;
	}

	StopRecoveryTimer();
	bRecoveryStarted = false;
	bRecoverImmediately = false;
	FVector2D Accepted(FMath::Max(0.0f, PitchKick), YawKick);
	// 把已累计和未播完的尾巴一起算进俯仰 / 偏航上限，超出的部分丢掉。
	if (bUseCumulativeLimits)
	{
		FVector2D Reserved(AccumulatedPitch, AccumulatedYaw);
		for (const FWeaponKitRecoilKickEnvelope& Kick : PendingKicks)
		{
			Reserved += Kick.GetRemaining();
		}
		const float PitchLimit = WeaponKitSafeNonNegative(MaxPitchOffset);
		const float YawLimit = WeaponKitSafeNonNegative(MaxYawOffset);
		Accepted.X = FMath::Clamp(Reserved.X + Accepted.X, 0.0f, PitchLimit) - Reserved.X;
		Accepted.Y = FMath::Clamp(Reserved.Y + Accepted.Y, -YawLimit, YawLimit) - Reserved.Y;
	}

	LastKickTime = World->GetTimeSeconds();
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		ApplyDelta(Accepted, true);
	}
	else
	{
		ApplyDelta(Accepted * ImmediateFraction, true);
		FWeaponKitRecoilKickEnvelope Tail;
		Tail.Total = Accepted * (1.0f - ImmediateFraction);
		Tail.Duration = Duration;
		if (!Tail.Total.IsNearlyZero())
		{
			PendingKicks.Add(Tail);
			SetComponentTickEnabled(true);
		}
	}
	StartRecoveryTimer();
}

// 瞄准直接改控制旋转；腰射走控制器输入，便于和玩家鼠标同一条通道。
void UWeaponKitCameraRecoilComponent::ApplyDelta(const FVector2D& Delta, bool bRecoverable)
{
	APawn* PawnOwner = Cast<APawn>(GetOwner());
	if (!PawnOwner || !PawnOwner->GetController())
	{
		return;
	}

	// 瞄准直接改控制旋转，俯仰钳在 ±89 度；腰射改走控制器输入。
	if (bAiming)
	{
		FRotator View = PawnOwner->GetController()->GetControlRotation();
		View.Pitch = FMath::ClampAngle(View.Pitch + Delta.X, -89.0f, 89.0f);
		View.Yaw += Delta.Y;
		PawnOwner->GetController()->SetControlRotation(View);
	}
	else
	{
		PawnOwner->AddControllerPitchInput(-Delta.X);
		PawnOwner->AddControllerYawInput(Delta.Y);
	}

	if (bRecoverable)
	{
		AccumulatedPitch += Delta.X;
		AccumulatedYaw += Delta.Y;
	}
}

// 分离模式指数回落摄像机偏移；否则推进写到控制器的平滑尾巴。
void UWeaponKitCameraRecoilComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bIndependentVisualMode)
	{
		if (!VisualCamera.IsValid() || !FMath::IsFinite(DeltaTime))
		{
			ClearRecoil();
			return;
		}
		const float Dt = FMath::Max(0.0f, DeltaTime);
		for (int32 Index = VisualKicks.Num() - 1; Index >= 0; --Index)
		{
			VisualOffset += VisualKicks[Index].Advance(Dt);
			if (VisualKicks[Index].IsComplete())
			{
				VisualKicks.RemoveAt(Index);
			}
		}
		// 指数回落，帧率变化时接近同一条曲线。
		VisualOffset *= FMath::Exp(-VisualReturnSpeed * Dt);
		if (VisualKicks.IsEmpty() && VisualOffset.IsNearlyZero(0.0001f))
		{
			VisualOffset = FVector2D::ZeroVector;
			SetComponentTickEnabled(false);
		}
		WriteVisualOffset();
		return;
	}

	APawn* PawnOwner = Cast<APawn>(GetOwner());
	if (!PawnOwner || !PawnOwner->GetController() || !FMath::IsFinite(DeltaTime))
	{
		ClearRecoil();
		return;
	}

	for (int32 Index = PendingKicks.Num() - 1; Index >= 0; --Index)
	{
		FWeaponKitRecoilKickEnvelope& Kick = PendingKicks[Index];
		ApplyDelta(Kick.Advance(DeltaTime), Kick.bRecoverable);
		if (Kick.IsComplete())
		{
			PendingKicks.RemoveAt(Index);
		}
	}
	if (PendingKicks.IsEmpty())
	{
		SetComponentTickEnabled(false);
	}
}

// 没有剩余冲量时不再计时。立即恢复不等待延迟。
void UWeaponKitCameraRecoilComponent::StartRecoveryTimer()
{
	if (PendingKicks.IsEmpty() && FMath::IsNearlyZero(AccumulatedPitch) && FMath::IsNearlyZero(AccumulatedYaw))
	{
		StopRecoveryTimer();
		return;
	}
	if (UWorld* World = GetWorld())
	{
		const float Interval = FMath::Max(0.001f, WeaponKitSafeNonNegative(RecoveryInterval, 0.02f));
		World->GetTimerManager().SetTimer(
			RecoveryTimerHandle,
			this,
			&UWeaponKitCameraRecoilComponent::RecoveryStep,
			bRecoverImmediately ? Interval : FMath::Max(0.001f, ActiveRecoveryDelay),
			false);
	}
}

// 清掉自动恢复定时器。视角保持当前值。
void UWeaponKitCameraRecoilComponent::StopRecoveryTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	}
}

// 延迟结束后只收回一部分俯仰，偏航累计直接清零，不再自动拉回左右。
void UWeaponKitCameraRecoilComponent::RecoveryStep()
{
	UWorld* World = GetWorld();
	APawn* PawnOwner = Cast<APawn>(GetOwner());
	if (!World || !PawnOwner || !PawnOwner->GetController())
	{
		ClearRecoil();
		return;
	}

	const float Interval = FMath::Max(0.001f, WeaponKitSafeNonNegative(RecoveryInterval, 0.02f));
	const float QuietRemaining = ActiveRecoveryDelay - (World->GetTimeSeconds() - LastKickTime);
	if (!PendingKicks.IsEmpty() || (!bRecoveryStarted && !bRecoverImmediately && QuietRemaining > KINDA_SMALL_NUMBER))
	{
		World->GetTimerManager().SetTimer(
			RecoveryTimerHandle,
			this,
			&UWeaponKitCameraRecoilComponent::RecoveryStep,
			FMath::Max(Interval, bRecoverImmediately ? Interval : QuietRemaining),
			false);
		return;
	}

	if (!bRecoveryStarted)
	{
		bRecoveryStarted = true;
		bRecoverImmediately = false;
		// 按比例留下要收回的俯仰，其余视为玩家可以保留的上抬。
		AccumulatedPitch *= ActiveRecoveryRatio;
		AccumulatedYaw = 0.0f;
		if (FMath::IsNearlyZero(AccumulatedPitch))
		{
			StopRecoveryTimer();
			return;
		}
		World->GetTimerManager().SetTimer(
			RecoveryTimerHandle,
			this,
			&UWeaponKitCameraRecoilComponent::RecoveryStep,
			Interval,
			true);
	}

	const float PitchStep = FMath::Min(AccumulatedPitch, ActiveRecoverySpeed * Interval);
	if (PitchStep <= KINDA_SMALL_NUMBER)
	{
		NotifyManualLook();
		return;
	}

	if (bAiming)
	{
		FRotator View = PawnOwner->GetController()->GetControlRotation();
		View.Pitch = FMath::ClampAngle(View.Pitch - PitchStep, -89.0f, 89.0f);
		PawnOwner->GetController()->SetControlRotation(View);
	}
	else
	{
		PawnOwner->AddControllerPitchInput(PitchStep);
	}
	AccumulatedPitch -= PitchStep;
	if (FMath::IsNearlyZero(AccumulatedPitch))
	{
		StopRecoveryTimer();
		AccumulatedPitch = 0.0f;
	}
}

// 跳过等待，下一步就开始收俯仰。分离模式的镜头偏移不走这条路径。
void UWeaponKitCameraRecoilComponent::StartRecovery()
{
	if (bIndependentVisualMode)
	{
		return;
	}
	StopRecoveryTimer();
	bRecoveryStarted = false;
	bRecoverImmediately = true;
	StartRecoveryTimer();
}

// 停止恢复记账。已接受的尾巴继续播完，但不再计入可恢复角度。
void UWeaponKitCameraRecoilComponent::NotifyManualLook()
{
	if (bIndependentVisualMode)
	{
		return;
	}
	StopRecoveryTimer();
	AccumulatedPitch = 0.0f;
	AccumulatedYaw = 0.0f;
	bRecoveryStarted = false;
	bRecoverImmediately = false;
	for (FWeaponKitRecoilKickEnvelope& Kick : PendingKicks)
	{
		// 尾巴仍会施加到视角，恢复逻辑不会把这段再拽回去。
		Kick.bRecoverable = false;
	}
}

// 去掉镜头附加偏移、恢复记账和未播完的控制器尾巴。
void UWeaponKitCameraRecoilComponent::ClearRecoil()
{
	ClearIndependentVisual();
	NotifyManualLook();
	PendingKicks.Empty();
	LastKickTime = 0.0f;
	SetComponentTickEnabled(false);
}

// 结束时清后坐，避免摄像机上留下附加旋转。
void UWeaponKitCameraRecoilComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearRecoil();
	Super::EndPlay(EndPlayReason);
}

// 先抵消上一帧写过的附加变换，再写入当前俯仰 / 偏航。
void UWeaponKitCameraRecoilComponent::WriteVisualOffset()
{
	if (UCameraComponent* Camera = VisualCamera.Get())
	{
		const FTransform NewTransform(FRotator(VisualOffset.X, VisualOffset.Y, 0.0).Quaternion());
		Camera->AddAdditiveOffset(AppliedVisualTransform.Inverse() * NewTransform, 0.0f);
		AppliedVisualTransform = NewTransform;
	}
}

// 把摄像机附加偏移减掉，并退出分离模式。
void UWeaponKitCameraRecoilComponent::ClearIndependentVisual()
{
	if (UCameraComponent* Camera = VisualCamera.Get())
	{
		Camera->AddAdditiveOffset(AppliedVisualTransform.Inverse(), 0.0f);
	}
	VisualCamera.Reset();
	VisualKicks.Reset();
	VisualOffset = FVector2D::ZeroVector;
	AppliedVisualTransform = FTransform::Identity;
	bIndependentVisualMode = false;
}
