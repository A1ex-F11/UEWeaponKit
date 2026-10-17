/**
 * 可变槽位背包。槽位个数等于初始化时传入的 IWeaponKitSlotHost 数量。
 * 拾取物只通过 IWeaponKitPickupInterface 交流。扫描器只在丢枪成功后用来找脚边的下一把，选择逻辑不写在背包里。
 */
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/WeaponKitTypes.h"
#include "UObject/ScriptInterface.h"
#include "Interfaces/WeaponKitInterfaces.h"
#include "Runtime/WeaponKitRuntime.h"
#include "WeaponKitInventoryComponent.generated.h"

class UWeaponKitInteractionScanner;

/** 槽位内容或当前武器变化时广播当前快照。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponKitInventoryChanged, FWeaponKitInventorySnapshot, Snapshot);

/** 当前武器即将被换下或丢掉之前广播，表现层可在这里停火、收枪。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponKitBeforeWeaponChange, AWeaponKitRuntime*, OldWeapon);

/** 新手枪已经装备时广播。ShouldRefreshEquippedPresentation 为 true 时表现层应播放装备动画。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponKitEquippedChanged, AWeaponKitRuntime*, NewWeapon);

/**
 * 武器背包。挂在携带者 Pawn 上。
 * 先放好槽位组件，再 InitializeInventory。至少还留一把枪时才允许 TryDropCurrent。
 */
UCLASS(ClassGroup = (WeaponKit), meta = (BlueprintSpawnableComponent))
class WEAPONKIT_API UWeaponKitInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 关闭 Tick。切枪、拾取和丢枪都由调用方驱动。 */
	UWeaponKitInventoryComponent();

	/**
	 * 用一组槽位宿主完成初始化，并可选地装备 InitialSlotIndex。
	 * 只能成功一次。宿主必须是拥有者身上已注册、互不重复的组件。INDEX_NONE 表示开始时不装备。
	 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Inventory")
	bool InitializeInventory(const TArray<TScriptInterface<IWeaponKitSlotHost>>& SlotHosts, int32 InitialSlotIndex);

	/**
	 * 为外部读档清掉当前背包状态。
	 * 成功后背包未初始化，并记住下一次装备要刷新表现。不要在操作进行中调用。
	 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Inventory")
	bool PrepareForExternalRestore();

	/**
	 * 按存档重建每个槽，然后等待下一次 InitializeInventory 使用同一组宿主完成初始化。
	 * Save.Slots 的长度必须等于宿主数量。失败时会标记读档失败，随后的初始化也会被拒绝。
	 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Inventory")
	bool RestoreLoadout(const TArray<TScriptInterface<IWeaponKitSlotHost>>& SlotHosts, const FWeaponKitLoadoutSave& Save);

	/** 当前装备的武器。没有则空。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Inventory")
	AWeaponKitRuntime* GetCurrentWeapon() const;

	/** 指定槽里的武器。下标无效或槽是空的时返回空。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Inventory")
	AWeaponKitRuntime* GetSlotWeapon(int32 SlotIndex) const;

	/** 槽位数量，等于初始化时传入的宿主数量。未初始化时为 0。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Inventory")
	int32 GetSlotCount() const { return SlotHosts.Num(); }

	/** 当前武器的槽下标。没有当前武器时为 INDEX_NONE。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Inventory")
	int32 GetActiveSlotIndex() const { return ActiveSlotIndex; }

	/** 拾取、切枪、丢枪或读档正在进行。为 true 时新的背包操作会失败。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Inventory")
	bool IsInventoryBusy() const { return bInventoryBusy; }

	/** 最近一次装备广播是否要求表现层刷新。切枪时为 false，避免重复播放装备动画。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Inventory")
	bool ShouldRefreshEquippedPresentation() const { return bEquippedPresentationRefreshRequested; }

	/**
	 * 捡起 Source。Source 必须实现 IWeaponKitPickupInterface。
	 * 有空槽时放进第一个空槽；没有空槽时，仅当 bAllowReplaceCurrent 为 true 才替换当前武器。
	 * 玩家手动拾取传 true，自动拾取传 false。
	 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Inventory")
	bool TryPickup(AActor* Source, bool bAllowReplaceCurrent);

	/** 装备指定槽。槽里没有枪时失败。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Inventory")
	bool TryEquipSlot(int32 SlotIndex);

	/** 装备已经在某个槽里的武器。已经是当前武器时返回 true。会取消旧武器的换弹。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Inventory")
	bool TryEquipWeapon(AWeaponKitRuntime* NewWeapon);

	/** 切到下一个有枪的槽。只有一把枪或没有当前武器时失败，并且不刷新装备表现。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Inventory")
	bool TrySwitchWeapon();

	/**
	 * 丢掉当前武器。身上至少还要留一把枪。
	 * 丢出的 Actor 类来自 PickupClassByWeaponClass，并且必须实现 IWeaponKitPickupInterface。
	 * 成功后若指定了扫描器，会再问一次自动拾取候选。
	 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Inventory")
	bool TryDropCurrent();

	/** 复制当前槽位和当前武器，供界面或存档读取。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Inventory")
	FWeaponKitInventorySnapshot GetSnapshot() const;

	/** 背包内容变化。 */
	UPROPERTY(BlueprintAssignable, Category = "WeaponKit|Inventory")
	FOnWeaponKitInventoryChanged OnInventoryChanged;

	/** 当前武器即将改变。参数是还拿在手里的那把。 */
	UPROPERTY(BlueprintAssignable, Category = "WeaponKit|Inventory")
	FOnWeaponKitBeforeWeaponChange OnBeforeWeaponChange;

	/** 新的当前武器已经装上。配合 ShouldRefreshEquippedPresentation 决定要不要播装备表现。 */
	UPROPERTY(BlueprintAssignable, Category = "WeaponKit|Inventory")
	FOnWeaponKitEquippedChanged OnEquippedWeaponChanged;

	/** 只在丢枪成功后用来查找替换的自动拾取物。拾取选择本身仍由扫描器负责。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponKit|Inventory")
	TObjectPtr<UWeaponKitInteractionScanner> InteractionScanner;

	/** 武器类到拾取物类的对照。丢枪时用它生成 Actor，值必须实现 IWeaponKitPickupInterface。 */
	UPROPERTY(EditDefaultsOnly, Category = "WeaponKit|Inventory|Drop")
	TMap<TSubclassOf<AWeaponKitRuntime>, TSubclassOf<AActor>> PickupClassByWeaponClass;

	/** 丢枪生成点相对拥有者前方的距离（厘米）。 */
	UPROPERTY(EditDefaultsOnly, Category = "WeaponKit|Inventory|Drop", meta = (ClampMin = "0.0"))
	float DropForwardDistance = 200.0f;

	/** 丢枪生成点相对脚底向上的偏移（厘米）。 */
	UPROPERTY(EditDefaultsOnly, Category = "WeaponKit|Inventory|Drop", meta = (ClampMin = "0.0"))
	float DropUpOffset = 50.0f;

	/** 丢出后，原携带者不能自动捡回这一把的秒数。 */
	UPROPERTY(EditDefaultsOnly, Category = "WeaponKit|Inventory|Drop", meta = (ClampMin = "0.0"))
	float AutoPickupBlockSeconds = 0.75f;

protected:
	/** 结束时取消当前武器动作并清空槽位记录。不销毁槽里的武器 Actor。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 已初始化、组件仍注册、携带者存活，且槽位宿主仍然有效。 */
	bool IsOperational() const;

	/** 宿主非空、互不重复，并且都是拥有者身上已就绪的槽位组件。 */
	bool ValidateHosts(const TArray<TScriptInterface<IWeaponKitSlotHost>>& Hosts) const;

	/** 传入的宿主数组是否和当前记录逐项相同。读档后的初始化必须匹配。 */
	bool HostsMatch(const TArray<TScriptInterface<IWeaponKitSlotHost>>& Hosts) const;

	/** 读出每个槽的武器。内容无效或同一把枪出现在两个槽时失败。 */
	bool ReadSlotWeapons(TArray<AWeaponKitRuntime*>& OutWeapons) const;

	/** 取消旧武器的换弹。bNotify 为 true 时先广播 OnBeforeWeaponChange。 */
	void CancelCurrentWeaponActions(AWeaponKitRuntime* OldWeapon, bool bNotify);

	/** 非当前武器隐藏，当前武器显示。插件不切换网格或动画。 */
	void SetWeaponActorActive(AWeaponKitRuntime* Weapon, bool bActive) const;

	/** 把 NewWeapon 设为当前武器，隐藏其他槽，并按需要广播装备变化。 */
	void CommitCurrentWeapon(AWeaponKitRuntime* NewWeapon, bool bRefreshPresentation = true);

	/** 广播一份新的背包快照。 */
	void BroadcastInventoryChanged();

	/** 在槽里生成武器并恢复存档。失败时清空该槽。 */
	bool CreateWeaponInSlot(const TScriptInterface<IWeaponKitSlotHost>& Slot, TSubclassOf<AWeaponKitRuntime> WeaponClass, const FWeaponKitPickupState& State, AWeaponKitRuntime*& OutWeapon);

	/** 在拥有者前方生成隐藏、无碰撞的拾取物，成功丢出前不会被扫到。 */
	AActor* SpawnStagedPickup(TSubclassOf<AWeaponKitRuntime> WeaponClass, const FWeaponKitPickupState& State) const;

	/** 打开拾取物的显示和碰撞，并调用 LaunchDroppedWeapon。 */
	void LaunchStagedPickup(AActor* Pickup) const;

	/** 从扫描器注销并销毁已经捡起的拾取物。 */
	void ConsumePickupSource(AActor* Source);

	/** 生成失败时释放拾取物占用。 */
	void ReleasePickupClaim(AActor* Source) const;

	/**
	 * 用地上的枪替换当前武器，并把旧枪丢到地上。
	 * 新手枪生成失败时尝试把旧枪放回槽里；放不回才让旧枪留在地上。
	 */
	bool TryReplaceCurrentFromPickup(AActor* Source, TSubclassOf<AWeaponKitRuntime> IncomingClass, const FWeaponKitPickupState& IncomingState);

	/** 武器所在槽下标。不在任何槽里时返回 INDEX_NONE。 */
	int32 FindSlotIndex(const AWeaponKitRuntime* Weapon) const;

	/** 从 FromIndex 往后找下一个有枪的槽，会绕回开头。 */
	int32 FindNextOccupiedSlot(int32 FromIndex) const;

	/** 当前有枪的槽数量。少于 2 时不允许丢枪。 */
	int32 CountOccupiedSlots() const;

	/** 拥有者实现了携带者接口时询问是否存活；否则视为存活。拥有者无效时返回 false。 */
	bool IsCarrierAlive() const;

	/** 询问携带者这把枪的首抽是否已经播过。没实现接口时返回 false。 */
	bool HasConsumedInitialDraw(AWeaponKitRuntime* Weapon) const;

	/** 通知携带者这把枪的首抽已经消耗。没实现接口时什么都不做。 */
	void ApplyInitialDrawConsumed(AWeaponKitRuntime* Weapon) const;

	/** 武器类非空、派生自 AWeaponKitRuntime，且不是抽象类。 */
	static bool IsUsableWeaponClass(TSubclassOf<AWeaponKitRuntime> WeaponClass);

	/** 拾取物类派生自 Actor、实现 IWeaponKitPickupInterface，且不是抽象类。 */
	static bool IsUsablePickupActorClass(TSubclassOf<AActor> PickupClass);

	/** 初始化时记下的槽位宿主。 */
	UPROPERTY(Transient)
	TArray<TScriptInterface<IWeaponKitSlotHost>> SlotHosts;

	/** 当前武器。槽位清空后弱指针会失效。 */
	UPROPERTY(Transient)
	TWeakObjectPtr<AWeaponKitRuntime> CurrentWeapon;

	/** 当前武器槽下标。 */
	int32 ActiveSlotIndex = INDEX_NONE;

	/** 重入保护。公开操作开始时置位，结束时恢复。 */
	bool bInventoryBusy = false;

	/** InitializeInventory 已经成功。 */
	bool bInitialized = false;

	/** 下一次 OnEquippedWeaponChanged 是否要求刷新表现。由广播期间的守卫写入。 */
	bool bEquippedPresentationRefreshRequested = false;

	/** PrepareForExternalRestore 之后，初始化完成时要补一次装备广播。 */
	bool bExternalRestorePresentationPending = false;

	/** RestoreLoadout 已写好武器，但还要等匹配的 InitializeInventory。 */
	bool bCheckpointLoadoutPendingInitialization = false;

	/** 最近一次读档失败。置位后初始化会被拒绝，直到成功的 PrepareForExternalRestore。 */
	bool bCheckpointRestoreFailed = false;
};
