/**
 * 把武器冲量写到控制旋转，或只加到摄像机上。
 * 瞄准状态由 SetAiming 传入，角色自己决定何时瞄准。组件不认识具体的玩家类。
 */
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Math/RecoilKickEnvelope.h"
#include "TimerManager.h"
#include "WeaponKitCameraRecoilComponent.generated.h"

class UCameraComponent;
class UWeaponKitRecoilData;

/**
 * 镜头 / 控制器后坐。
 * 挂在 Pawn 上。武器 GenerateRecoilKick 得到俯仰和偏航后，调用 ApplyRecoil。
 */
UCLASS(ClassGroup = (WeaponKit), meta = (BlueprintSpawnableComponent))
class WEAPONKIT_API UWeaponKitCameraRecoilComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 默认不 Tick。只有平滑尾巴或分离模式的镜头偏移还在时才打开 Tick。 */
	UWeaponKitCameraRecoilComponent();

	/**
	 * 设置是否正在瞄准。
	 * 状态真正变化时会清掉还没施加完的尾巴；已经写进视角的角度保持不动。
	 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Recoil")
	void SetAiming(bool bAiming);

	/** 当前是否处于瞄准后坐模式。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Recoil")
	bool IsAiming() const { return bAiming; }

	/**
	 * 施加一发已经被武器接受的冲量。这是唯一的施加入口。
	 * 瞄准：按 AimingRecoilScale 写入控制旋转，下一发采样到的就是同一条视线。
	 * 腰射且开启「射击后坐与镜头分离」：只给 RecoilCamera 加偏移，不改控制旋转。
	 * 其他腰射：走控制器输入。随机后坐仍受 MaxPitchOffset / MaxYawOffset 约束。
	 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Recoil")
	void ApplyRecoil(float PitchKick, float YawKick, UWeaponKitRecoilData* RecoilSettings, UCameraComponent* RecoilCamera);

	/** 立刻开始控制器后坐的恢复，不等待恢复延迟。分离模式的镜头偏移不走这里。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Recoil")
	void StartRecovery();

	/** 玩家手动转视角时调用。取消自动恢复，已经接受的冲量尾巴继续播完，视角不会被拽回去。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Recoil")
	void NotifyManualLook();

	/** 取消恢复，并丢掉尚未施加完的冲量尾巴。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Recoil")
	void ClearRecoil();

	/** 推进平滑尾巴，或让分离模式的镜头偏移按指数回落。 */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** 随机腰射后坐的俯仰累计上限（度）。固定序列不受这一项限制。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	float MaxPitchOffset = 3.0f;

	/** 随机腰射后坐的偏航累计上限（度），左右对称。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	float MaxYawOffset = 0.5f;

	/** 随机腰射使用的恢复延迟（秒）。固定序列改用资产上的 RecoveryDelay。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	float RecoveryDelay = 0.20f;

	/** 随机腰射的恢复速度（度/秒）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	float RecoverySpeed = 1.0f;

	/** 随机腰射停火后收回的俯仰比例。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	float RecoveryPitchRatio = 0.35f;

	/** 恢复计时器的步进间隔（秒）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	float RecoveryInterval = 0.02f;

	/** 结束时清掉后坐和镜头附加偏移。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 把一发冲量分成立即部分和平滑尾巴，并开始恢复计时。bUseCumulativeLimits 为 true 时先按组件上限裁剪。 */
	void QueueKick(float PitchKick, float YawKick, float Duration, float ImmediateFraction, bool bUseCumulativeLimits);

	/** 把增量写到控制旋转（瞄准）或控制器输入（腰射）。bRecoverable 为 true 时计入可恢复累计角。 */
	void ApplyDelta(const FVector2D& Delta, bool bRecoverable);

	/** 安排下一次 RecoveryStep。没有剩余冲量时清掉定时器。 */
	void StartRecoveryTimer();

	/** 停止自动恢复定时器。 */
	void StopRecoveryTimer();

	/** 延迟结束后按比例收回俯仰，偏航累计直接清零。恢复期间再次开火会重新等待。 */
	void RecoveryStep();

	/** 把当前视觉偏移写成摄像机的附加旋转，先抵消上一帧写过的偏移。 */
	void WriteVisualOffset();

	/** 去掉摄像机上的附加偏移，并退出分离模式。 */
	void ClearIndependentVisual();

	/** 是否正在瞄准。与武器上的瞄准标记分开保存，角色应两边一起设置。 */
	bool bAiming = false;

	/** 当前腰射是否把后坐只画在摄像机上。 */
	bool bIndependentVisualMode = false;

	/** 分离模式使用的摄像机。切换摄像机时会清掉旧偏移。 */
	TWeakObjectPtr<UCameraComponent> VisualCamera;

	/** 分离模式下已经叠加、尚未回落完的镜头偏移。 */
	FVector2D VisualOffset = FVector2D::ZeroVector;

	/** 上一帧写到摄像机上的附加变换，下一帧先把它减掉。 */
	FTransform AppliedVisualTransform = FTransform::Identity;

	/** 分离模式里还没播完的镜头尾巴。 */
	TArray<FWeaponKitRecoilKickEnvelope> VisualKicks;

	/** 分离模式镜头回落速度，来自当前后坐资产。 */
	float VisualReturnSpeed = 12.0f;

	/** 已写入控制器、尚待恢复的俯仰。 */
	float AccumulatedPitch = 0.0f;

	/** 已写入控制器的偏航。恢复开始时清零，不自动拉回左右。 */
	float AccumulatedYaw = 0.0f;

	/** 最近一次接受冲量的世界时间，用来计算恢复延迟。 */
	float LastKickTime = 0.0f;

	/** 当前这一串后坐使用的恢复延迟。 */
	float ActiveRecoveryDelay = 0.2f;

	/** 当前这一串后坐使用的恢复速度。 */
	float ActiveRecoverySpeed = 1.0f;

	/** 当前这一串后坐要收回的俯仰比例。 */
	float ActiveRecoveryRatio = 0.35f;

	/** 自动恢复定时器。 */
	FTimerHandle RecoveryTimerHandle;

	/** StartRecovery 置位后，下一步不再等待延迟。 */
	bool bRecoverImmediately = false;

	/** 已经进入按间隔收回俯仰的阶段。 */
	bool bRecoveryStarted = false;

	/** 写到控制器、尚未播完的冲量尾巴。 */
	TArray<FWeaponKitRecoilKickEnvelope> PendingKicks;
};
