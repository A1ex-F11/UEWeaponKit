/**
 * 用 Child Actor 组件实现的默认武器槽。
 * 背包不依赖这个具体类。自定义槽位只要实现 IWeaponKitSlotHost，并在初始化时传给背包。
 */
#pragma once

#include "CoreMinimal.h"
#include "Components/ChildActorComponent.h"
#include "Interfaces/WeaponKitInterfaces.h"
#include "Runtime/WeaponKitChildActorSlot.generated.h"

class AWeaponKitRuntime;

/**
 * 默认槽位宿主。挂在携带者 Actor 上，一个组件对应一个槽。
 * 生成武器时会先清空再设置 Child Actor 类，避免残留上一把枪。
 */
UCLASS(ClassGroup = (WeaponKit), meta = (BlueprintSpawnableComponent))
class WEAPONKIT_API UWeaponKitChildActorSlot : public UChildActorComponent, public IWeaponKitSlotHost
{
	GENERATED_BODY()

public:
	/** 组件已注册且拥有者有效时，槽位可以接收武器。 */
	virtual bool IsSlotHostReady_Implementation() const override;

	/** 没有子 Actor，或子 Actor 是 AWeaponKitRuntime 时内容有效。其他类型视为坏槽。 */
	virtual bool IsHostedContentValid_Implementation() const override;

	/** 返回子 Actor 转成的武器；类型不对或为空时返回空。 */
	virtual AWeaponKitRuntime* GetHostedWeapon_Implementation() const override;

	/** 生成非抽象的武器子类。先清空再设置类，成功时子 Actor 必须是 AWeaponKitRuntime。 */
	virtual bool SpawnHostedWeapon_Implementation(TSubclassOf<AWeaponKitRuntime> WeaponClass) override;

	/** 清空 Child Actor 类，销毁槽里的武器。 */
	virtual void ClearHostedWeapon_Implementation() override;
};
