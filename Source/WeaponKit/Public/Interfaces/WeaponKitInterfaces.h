/**
 * 背包和武器运行时依赖的三个接口：携带者、槽位宿主、拾取物。
 * 游戏可以实现它们，也可以直接使用插件提供的 Child Actor 槽位和拾取 Actor。未实现携带者接口时，拥有者视为存活，且没有首抽记录。
 */
#pragma once

#include "CoreMinimal.h"
#include "Data/WeaponKitTypes.h"
#include "Runtime/WeaponKitRuntime.h"
#include "WeaponKitInterfaces.generated.h"

class APawn;
class UPrimitiveComponent;

/**
 * 携带武器的 Pawn 可选查询。
 * 不实现时，背包把拥有者当成存活，并且没有「首抽已消耗」的记录。
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UWeaponKitCarrier : public UInterface
{
	GENERATED_BODY()
};

/** 让角色回答是否存活，以及某把枪的首抽动画是否已经播过。死亡、换弹取消瞄准等表现留在角色上。 */
class WEAPONKIT_API IWeaponKitCarrier
{
	GENERATED_BODY()

public:
	/** 携带者是否还活着。返回 false 时背包操作和拾取扫描都会停。默认实现返回 true。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WeaponKit|Carrier")
	bool IsWeaponKitCarrierAlive() const;
	virtual bool IsWeaponKitCarrierAlive_Implementation() const;

	/** 这把枪的首抽是否已经播过。默认返回 false。武器指针用来区分多把枪。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WeaponKit|Carrier")
	bool HasConsumedWeaponInitialDraw(AWeaponKitRuntime* Weapon) const;
	virtual bool HasConsumedWeaponInitialDraw_Implementation(AWeaponKitRuntime* Weapon) const;

	/** 记录这把枪的首抽已经播过。丢枪和读档会把该标记放进 FWeaponKitPickupState。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WeaponKit|Carrier")
	void ApplyWeaponInitialDrawConsumed(AWeaponKitRuntime* Weapon);
	virtual void ApplyWeaponInitialDrawConsumed_Implementation(AWeaponKitRuntime* Weapon);
};

/**
 * 一个槽位如何生成和清空武器。
 * UWeaponKitChildActorSlot 是用 Child Actor 做的默认实现，背包只认这个接口。
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UWeaponKitSlotHost : public UInterface
{
	GENERATED_BODY()
};

/** 槽位宿主。初始化背包时传入数组，槽位个数等于宿主个数。宿主必须是拥有者身上已注册的组件。 */
class WEAPONKIT_API IWeaponKitSlotHost
{
	GENERATED_BODY()

public:
	/** 槽位是否已注册、可以生成武器。默认返回 false。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WeaponKit|Slot")
	bool IsSlotHostReady() const;
	virtual bool IsSlotHostReady_Implementation() const;

	/** 当前内容是否可接受。空槽返回 true；放了非武器 Actor 时应返回 false。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WeaponKit|Slot")
	bool IsHostedContentValid() const;
	virtual bool IsHostedContentValid_Implementation() const;

	/** 槽位里的武器。空槽返回空。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WeaponKit|Slot")
	AWeaponKitRuntime* GetHostedWeapon() const;
	virtual AWeaponKitRuntime* GetHostedWeapon_Implementation() const;

	/** 在槽位里生成指定武器类。抽象类或生成失败时返回 false。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WeaponKit|Slot")
	bool SpawnHostedWeapon(TSubclassOf<AWeaponKitRuntime> WeaponClass);
	virtual bool SpawnHostedWeapon_Implementation(TSubclassOf<AWeaponKitRuntime> WeaponClass);

	/** 销毁槽位里的武器，使该槽变空。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WeaponKit|Slot")
	void ClearHostedWeapon();
	virtual void ClearHostedWeapon_Implementation();
};

/**
 * 世界里的一把可拾取武器。
 * 背包只调用这个接口，不依赖具体 Actor 类。丢枪时生成的类必须实现它。
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UWeaponKitPickupInterface : public UInterface
{
	GENERATED_BODY()
};

/** 拾取物。AWeaponKitPickup 是默认实现。自定义拾取物至少要能报告武器类、存档状态，并完成占用和释放。 */
class WEAPONKIT_API IWeaponKitPickupInterface
{
	GENERATED_BODY()

public:
	/** 捡起后要生成的武器类。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WeaponKit|Pickup")
	TSubclassOf<AWeaponKitRuntime> GetPickupWeaponClass() const;
	virtual TSubclassOf<AWeaponKitRuntime> GetPickupWeaponClass_Implementation() const;

	/** 捡起后写回武器的弹药和开火模式。bHasSavedState 为 false 时使用配置资产的初始值。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WeaponKit|Pickup")
	FWeaponKitPickupState GetPickupSavedState() const;
	virtual FWeaponKitPickupState GetPickupSavedState_Implementation() const;

	/**
	 * 这个 Pawn 现在能否拾取。
	 * bAutomatic 为 true 时还要求自动拾取体积正在与 Pawn 重叠，并遵守丢枪后的短暂禁止。
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WeaponKit|Pickup")
	bool CanBePickedUpBy(const APawn* Pawn, bool bAutomatic) const;
	virtual bool CanBePickedUpBy_Implementation(const APawn* Pawn, bool bAutomatic) const;

	/** 占用这把拾取物，避免同一次交互被捡两次。不能捡时返回 false。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WeaponKit|Pickup")
	bool TryClaimPickup(APawn* Pawn, bool bAutomatic);
	virtual bool TryClaimPickup_Implementation(APawn* Pawn, bool bAutomatic);

	/** 生成武器失败时释放占用，让拾取物可以再次被捡。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WeaponKit|Pickup")
	void ReleasePickupClaim();
	virtual void ReleasePickupClaim_Implementation();

	/**
	 * 把丢出的武器写成拾取物。
	 * AutoPickupBlockSeconds 内，DroppedBy 不能自动捡回这一把。
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WeaponKit|Pickup")
	void InitializeDroppedWeapon(
		TSubclassOf<AWeaponKitRuntime> WeaponClass,
		const FWeaponKitPickupState& State,
		APawn* DroppedBy,
		float AutoPickupBlockSeconds);
	virtual void InitializeDroppedWeapon_Implementation(
		TSubclassOf<AWeaponKitRuntime> WeaponClass,
		const FWeaponKitPickupState& State,
		APawn* DroppedBy,
		float AutoPickupBlockSeconds);

	/** 自动拾取用的重叠体。返回空则不会自动拾取。不要靠组件名字查找。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WeaponKit|Pickup")
	UPrimitiveComponent* GetAutoPickupVolume() const;
	virtual UPrimitiveComponent* GetAutoPickupVolume_Implementation() const;

	/** 丢枪成功、碰撞打开后由背包调用。蓝图在这里施加抛出速度。前向由背包传入。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "WeaponKit|Pickup")
	void LaunchDroppedWeapon(FVector ForwardDirection);
};
