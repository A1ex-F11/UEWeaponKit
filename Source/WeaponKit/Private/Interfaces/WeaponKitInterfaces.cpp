// 三个接口的 C++ 默认实现。蓝图未覆盖时使用这些安全回退。
#include "Interfaces/WeaponKitInterfaces.h"

// 未覆盖时视为存活，背包和扫描器不会因为没实现接口而停工。
bool IWeaponKitCarrier::IsWeaponKitCarrierAlive_Implementation() const
{
	return true;
}

// 未覆盖时没有首抽记录。
bool IWeaponKitCarrier::HasConsumedWeaponInitialDraw_Implementation(AWeaponKitRuntime*) const
{
	return false;
}

// 未覆盖时忽略首抽标记。
void IWeaponKitCarrier::ApplyWeaponInitialDrawConsumed_Implementation(AWeaponKitRuntime*)
{
}

// 未覆盖的槽位不能接收武器。
bool IWeaponKitSlotHost::IsSlotHostReady_Implementation() const
{
	return false;
}

// 未覆盖时把内容视为有效，避免空的自定义槽被初始化拒绝。
bool IWeaponKitSlotHost::IsHostedContentValid_Implementation() const
{
	return true;
}

// 未覆盖时槽位是空的。
AWeaponKitRuntime* IWeaponKitSlotHost::GetHostedWeapon_Implementation() const
{
	return nullptr;
}

// 未覆盖时不能生成武器。
bool IWeaponKitSlotHost::SpawnHostedWeapon_Implementation(TSubclassOf<AWeaponKitRuntime>)
{
	return false;
}

// 未覆盖时清空是空操作。
void IWeaponKitSlotHost::ClearHostedWeapon_Implementation()
{
}

// 未覆盖时没有武器类，背包会拒绝拾取。
TSubclassOf<AWeaponKitRuntime> IWeaponKitPickup::GetPickupWeaponClass_Implementation() const
{
	return nullptr;
}

// 未覆盖时返回没有存档标记的空状态。
FWeaponKitPickupState IWeaponKitPickup::GetPickupSavedState_Implementation() const
{
	return FWeaponKitPickupState();
}

// 未覆盖时不能被拾取。
bool IWeaponKitPickup::CanBePickedUpBy_Implementation(const APawn*, bool) const
{
	return false;
}

// 未覆盖时占用失败。
bool IWeaponKitPickup::TryClaimPickup_Implementation(APawn*, bool)
{
	return false;
}

// 未覆盖时释放占用是空操作。
void IWeaponKitPickup::ReleasePickupClaim_Implementation()
{
}

// 未覆盖时忽略丢枪初始化。
void IWeaponKitPickup::InitializeDroppedWeapon_Implementation(
	TSubclassOf<AWeaponKitRuntime>,
	const FWeaponKitPickupState&,
	APawn*,
	float)
{
}

// 未覆盖时没有自动拾取体积，自动拾取关闭。
UPrimitiveComponent* IWeaponKitPickup::GetAutoPickupVolume_Implementation() const
{
	return nullptr;
}
