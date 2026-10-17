/**
 * 登记附近的拾取物，并按距离、视线和存活状态选出手动或自动目标。
 * 背包不会自己寻找「当前瞄准的那把枪」。玩家按下拾取键时，把 GetAimedPickup 或 GetManualPickup 的结果传给 TryPickup。
 */
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Interfaces/WeaponKitInterfaces.h"
#include "TimerManager.h"
#include "WeaponKitInteractionScanner.generated.h"

class APawn;

/** 当前瞄准的拾取物发生变化时广播。没有目标时参数为空。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponKitAimedPickupChanged, AActor*, AimedPickup);

/**
 * 交互扫描器。挂在携带者上，由拾取物重叠或游戏逻辑调用 RegisterPickup。
 * 有登记目标时按 RefreshInterval 刷新瞄准结果；一个都不剩时停掉定时器。
 */
UCLASS(ClassGroup = (WeaponKit), meta = (BlueprintSpawnableComponent))
class WEAPONKIT_API UWeaponKitInteractionScanner : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 关闭 Tick，改用定时器刷新瞄准拾取物。 */
	UWeaponKitInteractionScanner();

	/** 把实现了 IWeaponKitPickupInterface 的 Actor 登记到附近列表，并立刻刷新一次瞄准。重复登记会被忽略。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Scanner")
	bool RegisterPickup(AActor* Pickup);

	/** 从附近列表移除该拾取物以及已经失效的项，然后刷新瞄准。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Scanner")
	void UnregisterPickup(AActor* Pickup);

	/** 当前最适合手动拾取的目标：先看视线命中，否则取距离内、视线不被挡住的最近一个。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Scanner")
	AActor* GetManualPickup() const;

	/**
	 * 当前可自动拾取的目标。
	 * ExcludedPickup 会被忽略，丢枪后用来寻找脚边的另一把，而不是刚丢掉的那把。
	 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Scanner")
	AActor* GetAutomaticPickup(AActor* ExcludedPickup) const;

	/** 手动候选里、视线射线确实打中的那一个。没打中则返回空，用来驱动准星或提示。 */
	UFUNCTION(BlueprintPure, Category = "WeaponKit|Scanner")
	AActor* GetAimedPickup() const;

	/** 瞄准目标出现、消失或换成另一把时广播。 */
	UPROPERTY(BlueprintAssignable, Category = "WeaponKit|Scanner")
	FOnWeaponKitAimedPickupChanged OnAimedPickupChanged;

	/** 拾取物与携带者的最大距离（厘米）。超出的登记项不会被选中。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponKit|Scanner", meta = (ClampMin = "1.0"))
	float MaxDistance = 300.0f;

	/** 视线检测使用的碰撞通道。默认 Visibility。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponKit|Scanner")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** 刷新瞄准目标的间隔（秒）。至少按 0.01 秒使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponKit|Scanner", meta = (ClampMin = "0.01"))
	float RefreshInterval = 0.1f;

protected:
	/** 停掉刷新定时器，并在还有瞄准目标时广播空，避免提示残留。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 在已登记、距离内且 CanBePickedUpBy 通过的拾取物中选择一个。bAutomaticOnly 为 true 时走自动拾取条件。 */
	AActor* FindPickup(bool bAutomaticOnly, AActor* ExcludedPickup) const;

	/** 拥有者实现了携带者接口时询问是否存活；没实现则视为存活。 */
	bool IsCarrierAlive() const;

	/** 重新计算瞄准目标。只有存活状态或目标变化时才广播。 */
	void RefreshAimedPickup();

	/** 附近列表里是否还有有效拾取物。没有时停止定时器。 */
	bool HasRegisteredPickup() const;

	/** 附近已登记的拾取物。使用弱指针，销毁后会被剔除。 */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> NearbyPickups;

	/** 上一帧广播过的瞄准目标，用来判断要不要再次广播。 */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LastAimedPickup;

	/** 周期性调用 RefreshAimedPickup 的定时器。 */
	FTimerHandle RefreshTimerHandle;

	/** 上一轮是否存在瞄准目标。从有到无也要广播一次空指针。 */
	bool bHadAimedPickup = false;
};
