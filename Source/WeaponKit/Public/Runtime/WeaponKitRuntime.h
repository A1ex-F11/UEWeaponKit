/**
 * 武器 Actor：开火间隔、弹药、换弹、开火模式、后坐序列和散布采样。
 * 静态数从 UWeaponKitDefinitionData 读。瞄准通过 SetAiming 传入，插件不引用具体玩家类。
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/WeaponKitTypes.h"
#include "Data/WeaponKitDefinitionData.h"
#include "Runtime/WeaponKitRuntime.generated.h"

class UWeaponKitBallisticsData;
class UWeaponKitRecoilData;

/** 一发射击已经被接受（弹药已扣、间隔已刷新）时广播。表现层在这里播枪口和动画。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponKitFireCommitted);

/** TryStartReload 成功时广播。表现层开始换弹动画，并在合适的时机调用 FinishReload 或 TryCommitReloadRound。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponKitReloadStarted);

/**
 * 运行时武器。
 * 没有绑配置时只用构造函数里的默认值。射击不会改共享 DataAsset。
 * 角色在右键瞄准时调用 SetAiming，开火时先 TryCommitFire，再 GenerateShotDirection 和 GenerateRecoilKick。
 */
UCLASS(Blueprintable)
class WEAPONKIT_API AWeaponKitRuntime : public AActor
{
	GENERATED_BODY()

public:
	/** 关闭 Tick，并允许半自动作为默认开火模式。 */
	AWeaponKitRuntime();

	/** 编辑器里改属性时重新套用配置，不重置已经存在的运行时弹药。 */
	virtual void OnConstruction(const FTransform& Transform) override;

	/**
	 * 生成完成后套用配置。
	 * 若之前没有 RestoreAmmo / RestoreFireMode，才写入资产上的初始弹药和默认开火模式。
	 */
	virtual void PostInitializeComponents() override;

	/** 这把枪的静态配置。为空时开火间隔、散布和后坐都走代码默认值。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponKit", meta = (DisplayName = "枪械玩法配置"))
	TObjectPtr<UWeaponKitDefinitionData> WeaponDefinition;

	/**
	 * 由角色、AI 或测试代码设置是否正在瞄准。
	 * 瞄准期间提交的那一发会记下「不要再叠加弹道后坐」。
	 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Aim")
	void SetAiming(bool bAiming);

	/** 当前是否瞄准。镜头组件的瞄准状态需要角色另行设置。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Aim")
	bool IsAiming() const { return bOwnerAiming; }

	/**
	 * 从 WeaponDefinition 复制容量、间隔、后坐和换弹开关。
	 * 不改当前弹药和开火模式，方便换配置后保留弹匣。
	 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Config", meta = (DisplayName = "应用枪械配置（保留当前弹药）"))
	void ApplyDefinitionSettings();

	/** 当前瞄准状态下、经过停火恢复后的连续射击散布（度）。未启用时为 0。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Config")
	float GetCurrentFiringSpread() const;

	/** 弹匣容量。来自配置，至少为 1。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Config")
	int32 GetMagazineCapacity() const { return MagazineCapacity; }

	/** 三连发一轮结束后的等待时间（秒）。没有配置时为 0。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Config")
	float GetBurstRecoveryInterval() const;

	/** 配置里的弹道资产。没有配置或没有弹道子对象时返回空。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Config")
	UWeaponKitBallisticsData* GetBallistics() const;

	/** 展示名。优先读配置资产，否则读上次套用时缓存的名字。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Config")
	FText GetWeaponDisplayName() const;

	/** 当前开火模式。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit")
	EWeaponKitFireMode GetFireMode() const { return CurrentFireMode; }

	/** 读档或捡枪时恢复开火模式，并阻止随后的首次初始化覆盖它。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit")
	void RestoreFireMode(EWeaponKitFireMode SavedFireMode);

	/** 在允许列表里循环到下一个开火模式。列表不足两项时不变。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit")
	void CycleFireMode();

	/** 是否正在换弹。换弹期间 TryCommitFire 会失败。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Reload")
	bool IsReloading() const { return bIsReloading; }

	/**
	 * 开始换弹。弹匣已满、没有备弹或已经在换弹时返回 false。
	 * 会重置后坐和连续射击散布，并广播 OnReloadStarted。
	 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Reload")
	bool TryStartReload();

	/**
	 * 结束换弹并把计划中的子弹装入。
	 * 逐发换弹时子弹已由 TryCommitReloadRound 装入，这里只清状态。
	 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Reload")
	bool FinishReload();

	/** 取消换弹，不装入剩余计划弹药。已逐发装入的子弹保留。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Reload")
	bool CancelReload();

	/** 当前这次换弹的编号。逐发装弹时传回 TryCommitReloadRound，避免旧动画通知写到新的一次换弹上。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Reload")
	int32 GetReloadRequestId() const { return ReloadRequestId; }

	/** 逐发换弹时装入一发。ExpectedReloadRequestId 必须等于 GetReloadRequestId。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Reload")
	bool TryCommitReloadRound(int32 ExpectedReloadRequestId);

	/** 换弹开始。表现层监听后播放换弹，不要在插件里做动画。 */
	UPROPERTY(BlueprintAssignable, Category = "WeaponKit|Reload")
	FOnWeaponKitReloadStarted OnReloadStarted;

	/** 弹匣内当前子弹数。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Ammo")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }

	/** 备用子弹数。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Ammo")
	int32 GetReserveAmmo() const { return ReserveAmmo; }

	/** 扣掉弹匣里的一发。没有子弹时返回 false，调用方不应再生成弹道。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Ammo")
	bool TryConsumeAmmo();

	/** 增加备用弹药。Amount 小于等于 0 时失败。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Ammo")
	bool AddReserveAmmo(int32 Amount);

	/** 立刻把备弹装入弹匣，直到满匣或备弹耗尽。逐发换弹进行中时拒绝，以免和逐发流程打架。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Ammo")
	bool ReloadAmmo();

	/** 直接写入弹药，并标记为已恢复，使随后的首次初始化不再覆盖。进行中的逐发换弹会被取消。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Ammo")
	void RestoreAmmo(int32 NewCurrentAmmo, int32 NewReserveAmmo);

	/**
	 * 尝试提交一发射击：检查换弹和间隔、扣弹、缓存这一发的散布和后坐，并广播 OnFireCommitted。
	 * 不发射线。游戏接着调用 GenerateShotDirection 和 GenerateRecoilKick。
	 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Fire")
	bool TryCommitFire();

	/** 下一发要遵守的间隔（秒）。三连发且连发间隔大于 0 时用连发间隔，否则用普通射击间隔。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Fire")
	float GetEffectiveFireInterval() const;

	/** 一发射击已提交。表现层在这里播枪口、音效，并驱动曳光。 */
	UPROPERTY(BlueprintAssignable, Category = "WeaponKit|Fire")
	FOnWeaponKitFireCommitted OnFireCommitted;

	/** 清空后坐序列、弹道累计和两套连续射击散布。换弹开始和读档时调用。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Recoil")
	void ResetRecoilPattern();

	/**
	 * 取出 TryCommitFire 缓存的这一发镜头冲量。
	 * 没有待取冲量或数值非法时 bGenerated 为 false。每次成功提交只能取一次。
	 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Recoil")
	void GenerateRecoilKick(float& PitchKick, float& YawKick, bool& bGenerated);

	/**
	 * 根据调用方给的视线、速度和姿态，采样这一发的射击方向。
	 * 必须先 TryCommitFire。瞄准提交的那一发不会再叠加累计弹道后坐。
	 * TraceDirection 接近零，或分离后坐尚未提交时，bGenerated 为 false。
	 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Spread")
	void GenerateShotDirection(
		const FVector& TraceDirection,
		float CurrentHorizontalSpeed,
		bool bIsFalling,
		bool bIsCrouched,
		bool bIsSlowWalking,
		FVector& ShotDirection,
		bool& bGenerated);

	/** 读取某个武器类默认对象上的展示名。类无效时返回空文本。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Display")
	static FText GetDisplayNameForWeaponClass(TSubclassOf<AWeaponKitRuntime> WeaponClass);

	/** 若状态带有存档标记，则恢复弹药和开火模式。没有标记时保持资产初始值。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Pickup")
	void RestorePickupRuntimeState(const FWeaponKitPickupState& State);

	/** 抓取当前弹药和开火模式，供丢枪或存档。bInitialDrawConsumed 由携带者接口提供。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Pickup")
	FWeaponKitPickupState CapturePickupRuntimeState(bool bInitialDrawConsumed) const;

private:
	/** 从配置取腰射或瞄准散布。没有配置时返回一份代码默认的腰射形状。 */
	FWeaponKitSpreadProfile GetSpreadProfile(bool bAiming) const;

	/** 指定那一套连续射击散布在当前时间恢复后的值。 */
	float GetFiringSpreadForProfile(bool bAiming) const;

	/** 在成功射击的时刻采样后坐，处理序列重置、弹道恢复，并缓存待取的镜头冲量。 */
	void CacheCommittedRecoil(float SuccessfulShotTime);

	/** 记下这一发使用的连续射击散布，并让腰射、瞄准两套累计各自增加，切换时互不清零。 */
	void CacheCommittedFiringSpread(float SuccessfulShotTime);

	/** 固定序列前进一格。走完后停在末项，或跳到 LoopStartIndex。 */
	void AdvanceRecoilPatternIndex(int32 PatternSize);

	/** 丢掉尚未被 GenerateRecoilKick 取走的冲量。 */
	void ClearPendingRecoilKick();

	/** 弹匣内子弹。首次初始化前的默认值只作占位，会被配置覆盖。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WeaponKit|Ammo", meta = (AllowPrivateAccess = "true"))
	int32 CurrentAmmo = 30;

	/** 当前弹匣容量。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WeaponKit|Ammo", meta = (AllowPrivateAccess = "true"))
	int32 MagazineCapacity = 30;

	/** 备用弹药。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WeaponKit|Ammo", meta = (AllowPrivateAccess = "true"))
	int32 ReserveAmmo = 90;

	/** 当前开火模式。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WeaponKit", meta = (AllowPrivateAccess = "true"))
	EWeaponKitFireMode CurrentFireMode = EWeaponKitFireMode::SemiAutomatic;

	/** 允许切换的开火模式，从配置复制。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WeaponKit", meta = (AllowPrivateAccess = "true"))
	TArray<EWeaponKitFireMode> AllowedFireModes;

	/** 普通射击间隔（秒）。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WeaponKit|Fire", meta = (AllowPrivateAccess = "true"))
	float FireInterval = 0.15f;

	/** 三连发相邻子弹间隔（秒）。0 表示退回普通间隔。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WeaponKit|Fire", meta = (AllowPrivateAccess = "true"))
	float BurstShotInterval = 0.0f;

	/** 上次从配置复制的展示名。配置随后被清空时仍能显示。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WeaponKit|Display", meta = (AllowPrivateAccess = "true"))
	FText WeaponDisplayName;

	/** 是否正在换弹。 */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "WeaponKit|Reload", meta = (AllowPrivateAccess = "true"))
	bool bIsReloading = false;

	/** 下一次允许 TryCommitFire 的世界时间。 */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "WeaponKit|Fire", meta = (AllowPrivateAccess = "true"))
	float NextFireTime = 0.0f;

	/** 当前使用的后坐资产，从配置复制指针，不拥有它。 */
	TObjectPtr<UWeaponKitRecoilData> RecoilData;

	/** 换弹开始时是否锁定计划装弹量。 */
	bool bLockReloadAmountAtStart = false;

	/** 配置是否要求逐发装弹。 */
	bool bReloadOneRoundAtATime = false;

	/** 空仓且备弹足够时，是否把逐发换弹改成一次装入。 */
	bool bUseBatchReloadForFullEmptyReload = false;

	/** 当前这一次换弹是否按发装入。空仓整匣快捷换弹时为 false。 */
	bool bCurrentReloadUsesPerRound = false;

	/** 这一次换弹计划装入的发数。逐发流程每成功一发减一。 */
	int32 PendingReloadAmount = 0;

	/** 换弹编号。到 int32 上限后从 1 重新开始，避免和「未开始」的 0 混淆。 */
	int32 ReloadRequestId = 0;

	/** 角色通过 SetAiming 写入的瞄准状态。 */
	bool bOwnerAiming = false;

	/** TryCommitFire 当时选定的散布配置。之后切换瞄准不会改已经提交的这一发。 */
	FWeaponKitSpreadProfile CommittedSpreadProfile;

	/** 是否有一发已经提交、尚未被新的 TryCommitFire 清掉的散布配置。 */
	bool bHasCommittedSpreadProfile = false;

	/** 这一发提交时正在瞄准，采样方向时不再叠加累计弹道后坐。 */
	bool bCommittedIgnoreShotRecoil = false;

	/** 瞄准散布那一套的连续射击累计。 */
	float AccumulatedAimFiringSpread = 0.0f;

	/** 瞄准散布上一次成功射击的时间。 */
	float LastAimSpreadShotTime = 0.0f;

	/** 瞄准散布是否已经打过至少一发。 */
	bool bHasAimSpreadShot = false;

	/** 腰射散布那一套的连续射击累计。 */
	float AccumulatedFiringSpread = 0.0f;

	/** 最近一次提交使用的连续射击散布，加在基础锥角上。 */
	float CommittedFiringSpread = 0.0f;

	/** 腰射散布上一次成功射击的时间。 */
	float LastSpreadShotTime = 0.0f;

	/** 腰射散布是否已经打过至少一发。 */
	bool bHasSpreadShot = false;

	/** RestoreAmmo 已调用。PostInitialize 不再写入初始弹药。 */
	bool bRestoredAmmo = false;

	/** RestoreFireMode 已调用。PostInitialize 不再写入默认开火模式。 */
	bool bRestoredFireMode = false;

	/** 首次初始化是否已经把初始弹药和默认模式写过一次。 */
	bool bInitialDefinitionApplied = false;

	/** 固定后坐序列的当前下标。 */
	int32 RecoilPatternIndex = 0;

	/** 上一发成功射击的世界时间，用于序列重置和弹道恢复。 */
	float LastSuccessfulShotTime = 0.0f;

	/** 是否已有上一发成功射击的时间。 */
	bool bHasSuccessfulShotTime = false;

	/** 缓存的、尚未取出的俯仰冲量。 */
	float CachedRecoilPitchKick = 0.0f;

	/** 缓存的、尚未取出的偏航冲量。 */
	float CachedRecoilYawKick = 0.0f;

	/** GenerateRecoilKick 是否还有一发可取。 */
	bool bHasPendingRecoilKick = false;

	/** 分离模式下累计的弹道后坐。X 俯仰，Y 偏航。 */
	FVector2D AccumulatedShotRecoil = FVector2D::ZeroVector;

	/** 这一发提交时冻结的弹道后坐。瞄准开火时采样会忽略它。 */
	FVector2D CommittedShotRecoil = FVector2D::ZeroVector;

	/** 这一发是否已经缓存过弹道方向。分离模式在此之前拒绝采样。 */
	bool bHasCommittedShotDirection = false;
};

/** 一个槽位的存档：武器类加上掉落时那份运行时状态。类为空表示空槽。 */
USTRUCT(BlueprintType)
struct WEAPONKIT_API FWeaponKitSlotSave
{
	GENERATED_BODY()

	/** 该槽要生成的武器类。空类表示这个槽没有枪。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponKit|Inventory")
	TSubclassOf<AWeaponKitRuntime> WeaponClass;

	/** 该槽武器的弹药、开火模式和首抽标记。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponKit|Inventory")
	FWeaponKitPickupState RuntimeState;
};

/**
 * 一份覆盖每个槽位的存档。
 * 交给 UWeaponKitInventoryComponent::RestoreLoadout。槽数组长度必须和宿主数量一致。
 */
USTRUCT(BlueprintType)
struct WEAPONKIT_API FWeaponKitLoadoutSave
{
	GENERATED_BODY()

	/** 与背包槽位一一对应。下标即槽位下标。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponKit|Inventory")
	TArray<FWeaponKitSlotSave> Slots;

	/** 读档后握在手里的槽。INDEX_NONE 表示没有当前武器。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponKit|Inventory")
	int32 ActiveSlotIndex = INDEX_NONE;
};

/** 背包快照，随 OnInventoryChanged 广播。只反映当时的槽位和当前武器。 */
USTRUCT(BlueprintType)
struct WEAPONKIT_API FWeaponKitInventorySnapshot
{
	GENERATED_BODY()

	/** 每个槽当前的武器，空槽是空指针。顺序与初始化时的宿主数组一致。 */
	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Inventory")
	TArray<TObjectPtr<AWeaponKitRuntime>> SlotWeapons;

	/** 当前装备的武器。没有则空。 */
	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Inventory")
	TObjectPtr<AWeaponKitRuntime> CurrentWeapon;

	/** 当前武器所在槽。没有当前武器时为 INDEX_NONE。 */
	UPROPERTY(BlueprintReadOnly, Category = "WeaponKit|Inventory")
	int32 ActiveSlotIndex = INDEX_NONE;
};
