/**
 * 默认可拾取武器 Actor，实现 IWeaponKitPickupInterface。
 * 丢枪时由背包按 PickupClassByWeaponClass 生成。自动拾取体积用 SetAutoPickupVolume 指定，不按组件名搜索。
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/WeaponKitInterfaces.h"
#include "WeaponKitPickup.generated.h"

class APawn;
class UPrimitiveComponent;

/**
 * 世界中的武器拾取物。
 * 蓝图子类设置武器类；运行时丢枪会改写武器类、存档状态和自动拾取冷却。
 */
UCLASS(Blueprintable)
class WEAPONKIT_API AWeaponKitPickup : public AActor, public IWeaponKitPickupInterface
{
	GENERATED_BODY()

public:
	/** 关闭 Tick。拾取判定由背包和扫描器主动查询。 */
	AWeaponKitPickup();

	/** 返回 PickupWeaponClass。 */
	virtual TSubclassOf<AWeaponKitRuntime> GetPickupWeaponClass_Implementation() const override;

	/** 返回 PickupSavedState。未写入存档时 bHasSavedState 为 false。 */
	virtual FWeaponKitPickupState GetPickupSavedState_Implementation() const override;

	/**
	 * 手动拾取只检查携带者存活、武器类有效且未被占用。
	 * 自动拾取还要求冷却已过，并且自动拾取体积正在与该 Pawn 重叠。
	 */
	virtual bool CanBePickedUpBy_Implementation(const APawn* Pawn, bool bAutomatic) const override;

	/** 能捡时置占用标记并返回 true。 */
	virtual bool TryClaimPickup_Implementation(APawn* Pawn, bool bAutomatic) override;

	/** 清除占用，允许再次拾取。 */
	virtual void ReleasePickupClaim_Implementation() override;

	/** 写入丢出的武器类和状态，并在 AutoPickupBlockSeconds 内禁止 DroppedBy 自动捡回。 */
	virtual void InitializeDroppedWeapon_Implementation(
		TSubclassOf<AWeaponKitRuntime> WeaponClass,
		const FWeaponKitPickupState& State,
		APawn* DroppedBy,
		float AutoPickupBlockSeconds) override;

	/** 返回当前指定的自动拾取体积。空指针表示不能自动拾取。 */
	virtual UPrimitiveComponent* GetAutoPickupVolume_Implementation() const override;

	/** 直接指定自动拾取用的重叠组件。传空可关闭自动拾取。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Pickup")
	void SetAutoPickupVolume(UPrimitiveComponent* Volume);

protected:
	/** 放在关卡里时要生成的武器类。丢枪流程会覆盖它。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponKit|Pickup")
	TSubclassOf<AWeaponKitRuntime> PickupWeaponClass;

	/** 捡起后恢复的弹药和开火模式。关卡中的新枪通常保持 bHasSavedState 为 false。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponKit|Pickup")
	FWeaponKitPickupState PickupSavedState;

	/** 自动拾取重叠体。由 SetAutoPickupVolume 或编辑器指定，插件不按名字查找组件。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponKit|Pickup")
	TObjectPtr<UPrimitiveComponent> AutoPickupVolume;

private:
	/** 丢枪后暂时不能自动捡回这把枪的 Pawn。 */
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> AutoPickupBlockedPawn;

	/** 世界时间小于该值时，AutoPickupBlockedPawn 的自动拾取失败。 */
	double AutoPickupBlockedUntil = 0.0;

	/** 已被某次拾取占用。生成武器失败时由 ReleasePickupClaim 清掉。 */
	bool bPickupClaimed = false;
};
