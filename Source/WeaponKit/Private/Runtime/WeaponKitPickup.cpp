// 默认拾取物。手动拾取不看体积；自动拾取要求指定体积正在重叠，并遵守丢枪冷却。
#include "Runtime/WeaponKitPickup.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/WeaponKitInterfaces.h"
#include "Runtime/WeaponKitRuntime.h"

// 不 Tick。能否拾取由背包和扫描器查询。
AWeaponKitPickup::AWeaponKitPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("PickupRoot")));
}

// 返回当前武器类。丢枪初始化会改写它。
TSubclassOf<AWeaponKitRuntime> AWeaponKitPickup::GetPickupWeaponClass_Implementation() const
{
	return PickupWeaponClass;
}

// 返回弹药和开火模式。没有存档标记时武器会用配置初始值。
FWeaponKitPickupState AWeaponKitPickup::GetPickupSavedState_Implementation() const
{
	return PickupSavedState;
}

// 返回指定体积。空则不能自动拾取。
UPrimitiveComponent* AWeaponKitPickup::GetAutoPickupVolume_Implementation() const
{
	return AutoPickupVolume;
}

// 直接保存重叠组件，不按名字查找。
void AWeaponKitPickup::SetAutoPickupVolume(UPrimitiveComponent* Volume)
{
	AutoPickupVolume = Volume;
}

// 携带者死亡、武器类抽象、正在销毁或已被占用时都不能捡。
bool AWeaponKitPickup::CanBePickedUpBy_Implementation(const APawn* Pawn, bool bAutomatic) const
{
	UClass* WeaponClass = PickupWeaponClass.Get();
	const bool bCarrierAlive = !IsValid(Pawn) ||
		!Pawn->GetClass()->ImplementsInterface(UWeaponKitCarrier::StaticClass()) ||
		IWeaponKitCarrier::Execute_IsWeaponKitCarrierAlive(Pawn);
	if (!IsValid(Pawn) || !bCarrierAlive || IsActorBeingDestroyed() || bPickupClaimed ||
		!WeaponClass || WeaponClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return false;
	}
	// 手动拾取到这里即可。自动拾取还要看冷却和重叠。
	if (!bAutomatic)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	if (!World || (AutoPickupBlockedPawn.Get() == Pawn && World->GetTimeSeconds() < AutoPickupBlockedUntil))
	{
		return false;
	}
	const UPrimitiveComponent* Volume = IWeaponKitPickupInterface::Execute_GetAutoPickupVolume(this);
	// 只认 SetAutoPickupVolume 指定的体积，不搜索名为 AutoPickupCollision 的组件。
	return Volume && Volume->IsRegistered() && Volume->IsOverlappingActor(Pawn);
}

// 再次检查条件后置占用标记，避免同一次交互生成两把枪。
bool AWeaponKitPickup::TryClaimPickup_Implementation(APawn* Pawn, bool bAutomatic)
{
	if (!IWeaponKitPickupInterface::Execute_CanBePickedUpBy(this, Pawn, bAutomatic))
	{
		return false;
	}
	bPickupClaimed = true;
	return true;
}

// 生成失败时清占用，拾取物可以再被捡起。
void AWeaponKitPickup::ReleasePickupClaim_Implementation()
{
	bPickupClaimed = false;
}

// 记下武器、存档，以及哪一个 Pawn 在冷却结束前不能自动捡回。
void AWeaponKitPickup::InitializeDroppedWeapon_Implementation(
	TSubclassOf<AWeaponKitRuntime> WeaponClass,
	const FWeaponKitPickupState& State,
	APawn* DroppedBy,
	float AutoPickupBlockSeconds)
{
	PickupWeaponClass = WeaponClass;
	PickupSavedState = State;
	AutoPickupBlockedPawn = DroppedBy;
	if (const UWorld* World = GetWorld())
	{
		AutoPickupBlockedUntil = World->GetTimeSeconds() + FMath::Max(0.0f, AutoPickupBlockSeconds);
	}
	bPickupClaimed = false;
}
