// Child Actor 槽位。背包只通过 IWeaponKitSlotHost 调用这些函数。
#include "Runtime/WeaponKitChildActorSlot.h"
#include "Runtime/WeaponKitRuntime.h"

// 组件已注册且挂在有效 Actor 上才允许生成。
bool UWeaponKitChildActorSlot::IsSlotHostReady_Implementation() const
{
	return IsRegistered() && IsValid(GetOwner());
}

// 空槽有效；非武器子 Actor 视为坏槽，初始化会失败。
bool UWeaponKitChildActorSlot::IsHostedContentValid_Implementation() const
{
	AActor* Child = GetChildActor();
	return !IsValid(Child) || IsValid(Cast<AWeaponKitRuntime>(Child));
}

// 子 Actor 不是武器时返回空。
AWeaponKitRuntime* UWeaponKitChildActorSlot::GetHostedWeapon_Implementation() const
{
	return Cast<AWeaponKitRuntime>(GetChildActor());
}

// 拒绝抽象类。先清空再设置类，避免残留上一把枪。
bool UWeaponKitChildActorSlot::SpawnHostedWeapon_Implementation(TSubclassOf<AWeaponKitRuntime> WeaponClass)
{
	UClass* Class = WeaponClass.Get();
	if (!Class || !Class->IsChildOf(AWeaponKitRuntime::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract))
	{
		return false;
	}
	// 连续设置两次：第一次清掉旧子 Actor，第二次才生成新武器。
	SetChildActorClass(nullptr);
	SetChildActorClass(WeaponClass);
	return IsValid(Cast<AWeaponKitRuntime>(GetChildActor()));
}

// 清空类即销毁槽里的武器。
void UWeaponKitChildActorSlot::ClearHostedWeapon_Implementation()
{
	SetChildActorClass(nullptr);
}
