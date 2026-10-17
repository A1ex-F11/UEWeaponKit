// 可变槽位背包：初始化、读档、拾取、切枪和丢枪。扫描器只在丢枪成功后查询。
#include "Inventory/WeaponKitInventoryComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Runtime/WeaponKitInteractionScanner.h"
#include "Runtime/WeaponKitRuntime.h"

// 初始化或读档被拒绝时写入警告，不改变返回值。
DEFINE_LOG_CATEGORY_STATIC(LogWeaponKitInventory, Log, All);

// 不 Tick。所有背包操作由调用方发起。
UWeaponKitInventoryComponent::UWeaponKitInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// 必须是具体的 AWeaponKitRuntime 子类。
bool UWeaponKitInventoryComponent::IsUsableWeaponClass(TSubclassOf<AWeaponKitRuntime> WeaponClass)
{
	UClass* Class = WeaponClass.Get();
	return Class && Class->IsChildOf(AWeaponKitRuntime::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract);
}

// 丢枪生成的类必须实现拾取接口，且不能是抽象类。
bool UWeaponKitInventoryComponent::IsUsablePickupActorClass(TSubclassOf<AActor> PickupClass)
{
	UClass* Class = PickupClass.Get();
	return Class && Class->IsChildOf(AActor::StaticClass()) &&
		Class->ImplementsInterface(UWeaponKitPickupInterface::StaticClass()) &&
		!Class->HasAnyClassFlags(CLASS_Abstract);
}

// 没实现携带者接口时视为存活。拥有者无效时返回 false。
bool UWeaponKitInventoryComponent::IsCarrierAlive() const
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return false;
	}
	if (OwnerActor->GetClass()->ImplementsInterface(UWeaponKitCarrier::StaticClass()))
	{
		return IWeaponKitCarrier::Execute_IsWeaponKitCarrierAlive(OwnerActor);
	}
	return true;
}

// 没实现接口时视为首抽还没播。
bool UWeaponKitInventoryComponent::HasConsumedInitialDraw(AWeaponKitRuntime* Weapon) const
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->GetClass()->ImplementsInterface(UWeaponKitCarrier::StaticClass()))
	{
		return false;
	}
	return IWeaponKitCarrier::Execute_HasConsumedWeaponInitialDraw(OwnerActor, Weapon);
}

// 读档带首抽标记时通知携带者。没实现接口则忽略。
void UWeaponKitInventoryComponent::ApplyInitialDrawConsumed(AWeaponKitRuntime* Weapon) const
{
	AActor* OwnerActor = GetOwner();
	if (IsValid(OwnerActor) && OwnerActor->GetClass()->ImplementsInterface(UWeaponKitCarrier::StaticClass()))
	{
		IWeaponKitCarrier::Execute_ApplyWeaponInitialDrawConsumed(OwnerActor, Weapon);
	}
}

// 宿主必须非空、互不重复，且是拥有者身上已就绪的组件。
bool UWeaponKitInventoryComponent::ValidateHosts(const TArray<TScriptInterface<IWeaponKitSlotHost>>& Hosts) const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || Hosts.Num() <= 0)
	{
		return false;
	}
	for (int32 Index = 0; Index < Hosts.Num(); ++Index)
	{
		UObject* HostObject = Hosts[Index].GetObject();
		const UActorComponent* HostComponent = Cast<UActorComponent>(HostObject);
		if (!HostObject || !HostObject->GetClass()->ImplementsInterface(UWeaponKitSlotHost::StaticClass()) ||
			!IWeaponKitSlotHost::Execute_IsSlotHostReady(HostObject) ||
			!HostComponent || HostComponent->GetOwner() != OwnerActor)
		{
			return false;
		}
		for (int32 Other = 0; Other < Index; ++Other)
		{
			if (Hosts[Other].GetObject() == HostObject)
			{
				return false;
			}
		}
	}
	return true;
}

// 读档后的初始化必须传入同一组宿主。
bool UWeaponKitInventoryComponent::HostsMatch(const TArray<TScriptInterface<IWeaponKitSlotHost>>& Hosts) const
{
	if (Hosts.Num() != SlotHosts.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < Hosts.Num(); ++Index)
	{
		if (Hosts[Index].GetObject() != SlotHosts[Index].GetObject())
		{
			return false;
		}
	}
	return true;
}

// 只能成功一次。读档失败后会被拒绝，直到重新 PrepareForExternalRestore。
bool UWeaponKitInventoryComponent::InitializeInventory(
	const TArray<TScriptInterface<IWeaponKitSlotHost>>& InSlotHosts,
	int32 InitialSlotIndex)
{
	if (bInventoryBusy || bInitialized || !IsRegistered() || bCheckpointRestoreFailed || !ValidateHosts(InSlotHosts))
	{
		UE_LOG(LogWeaponKitInventory, Warning, TEXT("Inventory init rejected: busy, already initialized, unregistered, failed restore, or invalid slots."));
		return false;
	}
	// 槽位和读档时不一致就拒绝，避免武器生成在另一组组件上。
	if (bCheckpointLoadoutPendingInitialization && !HostsMatch(InSlotHosts))
	{
		UE_LOG(LogWeaponKitInventory, Warning, TEXT("Inventory init rejected: checkpoint slots do not match the restored hosts."));
		return false;
	}

	TArray<AWeaponKitRuntime*> Weapons;
	Weapons.SetNum(InSlotHosts.Num());
	for (int32 Index = 0; Index < InSlotHosts.Num(); ++Index)
	{
		UObject* HostObject = InSlotHosts[Index].GetObject();
		if (!IWeaponKitSlotHost::Execute_IsHostedContentValid(HostObject))
		{
			return false;
		}
		Weapons[Index] = IWeaponKitSlotHost::Execute_GetHostedWeapon(HostObject);
	}
	for (int32 Index = 0; Index < Weapons.Num(); ++Index)
	{
		if (!IsValid(Weapons[Index]))
		{
			continue;
		}
		for (int32 Other = Index + 1; Other < Weapons.Num(); ++Other)
		{
			if (Weapons[Index] == Weapons[Other])
			{
				return false;
			}
		}
	}

	AWeaponKitRuntime* InitialWeapon = nullptr;
	int32 ResolvedIndex = INDEX_NONE;
	if (bCheckpointLoadoutPendingInitialization)
	{
		InitialWeapon = CurrentWeapon.Get();
	}
	else if (InitialSlotIndex != INDEX_NONE)
	{
		if (!InSlotHosts.IsValidIndex(InitialSlotIndex) || !IsValid(Weapons[InitialSlotIndex]))
		{
			return false;
		}
		InitialWeapon = Weapons[InitialSlotIndex];
		ResolvedIndex = InitialSlotIndex;
	}
	TGuardValue<bool> BusyGuard(bInventoryBusy, true);
	SlotHosts = InSlotHosts;
	if (!bCheckpointLoadoutPendingInitialization)
	{
		CurrentWeapon = InitialWeapon;
		ActiveSlotIndex = ResolvedIndex;
	}
	else if (IsValid(InitialWeapon))
	{
		ActiveSlotIndex = FindSlotIndex(InitialWeapon);
	}
	bInitialized = true;
	BroadcastInventoryChanged();
	bCheckpointLoadoutPendingInitialization = false;
	bCheckpointRestoreFailed = false;
	if (bExternalRestorePresentationPending)
	{
		bExternalRestorePresentationPending = false;
		if (IsValid(CurrentWeapon.Get()))
		{
			TGuardValue<bool> PresentationRefreshGuard(bEquippedPresentationRefreshRequested, true);
			OnEquippedWeaponChanged.Broadcast(CurrentWeapon.Get());
		}
	}
	return true;
}

// 取消当前武器动作并清空初始化标记，为整份读档让路。
bool UWeaponKitInventoryComponent::PrepareForExternalRestore()
{
	if (bInventoryBusy)
	{
		return false;
	}

	TGuardValue<bool> BusyGuard(bInventoryBusy, true);
	if (AWeaponKitRuntime* OldWeapon = GetCurrentWeapon())
	{
		CancelCurrentWeaponActions(OldWeapon, true);
	}
	CurrentWeapon.Reset();
	ActiveSlotIndex = INDEX_NONE;
	SlotHosts.Reset();
	bInitialized = false;
	bExternalRestorePresentationPending = true;
	bCheckpointLoadoutPendingInitialization = false;
	bCheckpointRestoreFailed = false;
	return true;
}

// 按存档重建每个槽。任一槽失败会清空全部并标记读档失败。
bool UWeaponKitInventoryComponent::RestoreLoadout(
	const TArray<TScriptInterface<IWeaponKitSlotHost>>& InSlotHosts,
	const FWeaponKitLoadoutSave& Save)
{
	if (bInventoryBusy || !IsRegistered() || !ValidateHosts(InSlotHosts) || Save.Slots.Num() != InSlotHosts.Num() ||
		(Save.ActiveSlotIndex != INDEX_NONE && !InSlotHosts.IsValidIndex(Save.ActiveSlotIndex)))
	{
		UE_LOG(LogWeaponKitInventory, Warning, TEXT("Loadout restore rejected: invalid hosts or save."));
		bCheckpointRestoreFailed = true;
		return false;
	}
	for (const FWeaponKitSlotSave& SlotSave : Save.Slots)
	{
		if (SlotSave.WeaponClass && !IsUsableWeaponClass(SlotSave.WeaponClass))
		{
			bCheckpointRestoreFailed = true;
			return false;
		}
	}
	if (!PrepareForExternalRestore())
	{
		bCheckpointRestoreFailed = true;
		return false;
	}

	TGuardValue<bool> BusyGuard(bInventoryBusy, true);
	TArray<AWeaponKitRuntime*> RestoredWeapons;
	RestoredWeapons.SetNum(InSlotHosts.Num());
	for (int32 Index = 0; Index < InSlotHosts.Num(); ++Index)
	{
		const FWeaponKitSlotSave& SlotSave = Save.Slots[Index];
		if (!SlotSave.WeaponClass)
		{
			IWeaponKitSlotHost::Execute_ClearHostedWeapon(InSlotHosts[Index].GetObject());
			continue;
		}
		if (!CreateWeaponInSlot(InSlotHosts[Index], SlotSave.WeaponClass, SlotSave.RuntimeState, RestoredWeapons[Index]))
		{
			for (const TScriptInterface<IWeaponKitSlotHost>& Host : InSlotHosts)
			{
				IWeaponKitSlotHost::Execute_ClearHostedWeapon(Host.GetObject());
			}
			bExternalRestorePresentationPending = false;
			bCheckpointRestoreFailed = true;
			UE_LOG(LogWeaponKitInventory, Error, TEXT("Loadout restore failed while rebuilding slot %d."), Index);
			return false;
		}
	}

	AWeaponKitRuntime* RestoredCurrent = Save.ActiveSlotIndex == INDEX_NONE ? nullptr : RestoredWeapons[Save.ActiveSlotIndex];
	for (AWeaponKitRuntime* Weapon : RestoredWeapons)
	{
		SetWeaponActorActive(Weapon, Weapon == RestoredCurrent);
	}
	SlotHosts = InSlotHosts;
	CurrentWeapon = RestoredCurrent;
	ActiveSlotIndex = IsValid(RestoredCurrent) ? Save.ActiveSlotIndex : INDEX_NONE;
	bCheckpointLoadoutPendingInitialization = true;
	return true;
}

// 当前武器已被销毁时弱指针自然变空。
AWeaponKitRuntime* UWeaponKitInventoryComponent::GetCurrentWeapon() const
{
	return CurrentWeapon.Get();
}

// 下标无效或槽里不是有效武器时返回空。
AWeaponKitRuntime* UWeaponKitInventoryComponent::GetSlotWeapon(int32 SlotIndex) const
{
	if (!SlotHosts.IsValidIndex(SlotIndex))
	{
		return nullptr;
	}
	UObject* HostObject = SlotHosts[SlotIndex].GetObject();
	if (!HostObject)
	{
		return nullptr;
	}
	AWeaponKitRuntime* Weapon = IWeaponKitSlotHost::Execute_GetHostedWeapon(HostObject);
	return IsValid(Weapon) ? Weapon : nullptr;
}

// 复制当时的槽位数组、当前武器和下标。
FWeaponKitInventorySnapshot UWeaponKitInventoryComponent::GetSnapshot() const
{
	FWeaponKitInventorySnapshot Snapshot;
	Snapshot.ActiveSlotIndex = ActiveSlotIndex;
	Snapshot.CurrentWeapon = GetCurrentWeapon();
	Snapshot.SlotWeapons.Reserve(SlotHosts.Num());
	for (int32 Index = 0; Index < SlotHosts.Num(); ++Index)
	{
		Snapshot.SlotWeapons.Add(GetSlotWeapon(Index));
	}
	return Snapshot;
}

// 未初始化、携带者死亡或槽位失效时，拾取和切枪都会失败。
bool UWeaponKitInventoryComponent::IsOperational() const
{
	return bInitialized && IsRegistered() && IsCarrierAlive() && ValidateHosts(SlotHosts);
}

// 同一把枪出现在两个槽时视为数据损坏。
bool UWeaponKitInventoryComponent::ReadSlotWeapons(TArray<AWeaponKitRuntime*>& OutWeapons) const
{
	OutWeapons.Reset();
	if (!IsOperational())
	{
		return false;
	}
	OutWeapons.SetNum(SlotHosts.Num());
	for (int32 Index = 0; Index < SlotHosts.Num(); ++Index)
	{
		UObject* HostObject = SlotHosts[Index].GetObject();
		if (!IWeaponKitSlotHost::Execute_IsHostedContentValid(HostObject))
		{
			return false;
		}
		OutWeapons[Index] = IWeaponKitSlotHost::Execute_GetHostedWeapon(HostObject);
	}
	for (int32 Index = 0; Index < OutWeapons.Num(); ++Index)
	{
		if (!IsValid(OutWeapons[Index]))
		{
			continue;
		}
		for (int32 Other = Index + 1; Other < OutWeapons.Num(); ++Other)
		{
			if (OutWeapons[Index] == OutWeapons[Other])
			{
				return false;
			}
		}
	}
	return true;
}

// 不在任何槽里时返回 INDEX_NONE。
int32 UWeaponKitInventoryComponent::FindSlotIndex(const AWeaponKitRuntime* Weapon) const
{
	if (!IsValid(Weapon))
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < SlotHosts.Num(); ++Index)
	{
		if (GetSlotWeapon(Index) == Weapon)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

// 向后环形查找下一把枪，包含绕回后的当前槽。
int32 UWeaponKitInventoryComponent::FindNextOccupiedSlot(int32 FromIndex) const
{
	if (SlotHosts.Num() == 0)
	{
		return INDEX_NONE;
	}
	for (int32 Step = 1; Step <= SlotHosts.Num(); ++Step)
	{
		const int32 Index = (FromIndex + Step + SlotHosts.Num() * 2) % SlotHosts.Num();
		if (IsValid(GetSlotWeapon(Index)))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

// 少于两把时不允许丢枪。
int32 UWeaponKitInventoryComponent::CountOccupiedSlots() const
{
	int32 Count = 0;
	for (int32 Index = 0; Index < SlotHosts.Num(); ++Index)
	{
		if (IsValid(GetSlotWeapon(Index)))
		{
			++Count;
		}
	}
	return Count;
}

// 换下之前取消换弹。EndPlay 传 false，避免销毁过程中再广播。
void UWeaponKitInventoryComponent::CancelCurrentWeaponActions(AWeaponKitRuntime* OldWeapon, bool bNotify)
{
	if (!IsValid(OldWeapon))
	{
		return;
	}
	OldWeapon->CancelReload();
	if (bNotify)
	{
		OnBeforeWeaponChange.Broadcast(OldWeapon);
	}
}

// 只切换隐藏。动画和网格由游戏表现层处理。
void UWeaponKitInventoryComponent::SetWeaponActorActive(AWeaponKitRuntime* Weapon, bool bActive) const
{
	if (IsValid(Weapon))
	{
		Weapon->SetActorHiddenInGame(!bActive);
	}
}

// 隐藏其他槽，显示新手枪，并广播背包和装备变化。
void UWeaponKitInventoryComponent::CommitCurrentWeapon(AWeaponKitRuntime* NewWeapon, bool bRefreshPresentation)
{
	AWeaponKitRuntime* OldWeapon = CurrentWeapon.Get();
	if (IsValid(OldWeapon) && OldWeapon != NewWeapon)
	{
		SetWeaponActorActive(OldWeapon, false);
	}
	CurrentWeapon = NewWeapon;
	ActiveSlotIndex = FindSlotIndex(NewWeapon);
	for (int32 Index = 0; Index < SlotHosts.Num(); ++Index)
	{
		AWeaponKitRuntime* SlotWeapon = GetSlotWeapon(Index);
		if (SlotWeapon != NewWeapon)
		{
			SetWeaponActorActive(SlotWeapon, false);
		}
	}
	SetWeaponActorActive(NewWeapon, true);
	BroadcastInventoryChanged();
	if (IsValid(NewWeapon))
	{
		TGuardValue<bool> PresentationRefreshGuard(bEquippedPresentationRefreshRequested, bRefreshPresentation);
		OnEquippedWeaponChanged.Broadcast(NewWeapon);
	}
}

// 广播一份新快照。
void UWeaponKitInventoryComponent::BroadcastInventoryChanged()
{
	OnInventoryChanged.Broadcast(GetSnapshot());
}

// 生成后先隐藏，再恢复存档。失败时清空该槽。
bool UWeaponKitInventoryComponent::CreateWeaponInSlot(
	const TScriptInterface<IWeaponKitSlotHost>& Slot,
	TSubclassOf<AWeaponKitRuntime> WeaponClass,
	const FWeaponKitPickupState& State,
	AWeaponKitRuntime*& OutWeapon)
{
	OutWeapon = nullptr;
	UObject* HostObject = Slot.GetObject();
	if (!HostObject || !IWeaponKitSlotHost::Execute_IsSlotHostReady(HostObject) || !IsUsableWeaponClass(WeaponClass))
	{
		return false;
	}
	if (!IWeaponKitSlotHost::Execute_SpawnHostedWeapon(HostObject, WeaponClass))
	{
		IWeaponKitSlotHost::Execute_ClearHostedWeapon(HostObject);
		return false;
	}

	AWeaponKitRuntime* CreatedWeapon = IWeaponKitSlotHost::Execute_GetHostedWeapon(HostObject);
	if (!IsValid(CreatedWeapon))
	{
		IWeaponKitSlotHost::Execute_ClearHostedWeapon(HostObject);
		return false;
	}

	SetWeaponActorActive(CreatedWeapon, false);
	CreatedWeapon->RestorePickupRuntimeState(State);
	if (State.bHasSavedState && State.bInitialDrawConsumed)
	{
		ApplyInitialDrawConsumed(CreatedWeapon);
	}
	if (!IsValid(CreatedWeapon) || IWeaponKitSlotHost::Execute_GetHostedWeapon(HostObject) != CreatedWeapon)
	{
		IWeaponKitSlotHost::Execute_ClearHostedWeapon(HostObject);
		return false;
	}
	OutWeapon = CreatedWeapon;
	return true;
}

// 先生成隐藏且无碰撞的拾取物，确认背包改完再显示，避免半路被扫到。
AActor* UWeaponKitInventoryComponent::SpawnStagedPickup(
	TSubclassOf<AWeaponKitRuntime> WeaponClass,
	const FWeaponKitPickupState& State) const
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(OwnerActor) || !World || !IsUsableWeaponClass(WeaponClass))
	{
		return nullptr;
	}
	const TSubclassOf<AActor>* PickupClass = PickupClassByWeaponClass.Find(WeaponClass);
	if (!PickupClass || !IsUsablePickupActorClass(*PickupClass))
	{
		return nullptr;
	}

	const FVector Forward = OwnerActor->GetActorForwardVector().GetSafeNormal();
	const FVector SpawnLocation = OwnerActor->GetActorLocation() +
		Forward * FMath::Max(0.0f, DropForwardDistance) +
		FVector::UpVector * FMath::Max(0.0f, DropUpOffset);
	const FTransform SpawnTransform(OwnerActor->GetActorRotation(), SpawnLocation);
	APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	AActor* Pickup = World->SpawnActorDeferred<AActor>(
		PickupClass->Get(),
		SpawnTransform,
		OwnerActor,
		OwnerPawn,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!IsValid(Pickup) || !Pickup->GetClass()->ImplementsInterface(UWeaponKitPickupInterface::StaticClass()))
	{
		return nullptr;
	}

	IWeaponKitPickupInterface::Execute_InitializeDroppedWeapon(
		Pickup,
		WeaponClass,
		State,
		OwnerPawn,
		FMath::Max(0.0f, AutoPickupBlockSeconds));
	Pickup->SetActorHiddenInGame(true);
	Pickup->FinishSpawning(SpawnTransform);
	if (!IsValid(Pickup))
	{
		return nullptr;
	}
	Pickup->SetActorHiddenInGame(true);
	Pickup->SetActorEnableCollision(false);
	return Pickup;
}

// 打开显示和碰撞，并把前向交给蓝图抛出。
void UWeaponKitInventoryComponent::LaunchStagedPickup(AActor* Pickup) const
{
	if (!IsValid(Pickup))
	{
		return;
	}
	Pickup->SetActorHiddenInGame(false);
	Pickup->SetActorEnableCollision(true);
	if (Pickup->GetClass()->ImplementsInterface(UWeaponKitPickupInterface::StaticClass()))
	{
		const FVector Forward = IsValid(GetOwner()) ? GetOwner()->GetActorForwardVector().GetSafeNormal() : FVector::ForwardVector;
		IWeaponKitPickupInterface::Execute_LaunchDroppedWeapon(Pickup, Forward);
	}
}

// 从扫描器注销后销毁，避免瞄准提示指向已捡起的 Actor。
void UWeaponKitInventoryComponent::ConsumePickupSource(AActor* Source)
{
	if (!IsValid(Source))
	{
		return;
	}
	if (IsValid(InteractionScanner))
	{
		InteractionScanner->UnregisterPickup(Source);
	}
	Source->Destroy();
}

// 生成武器失败时允许这把拾取物再次被捡。
void UWeaponKitInventoryComponent::ReleasePickupClaim(AActor* Source) const
{
	if (IsValid(Source) && Source->GetClass()->ImplementsInterface(UWeaponKitPickupInterface::StaticClass()))
	{
		IWeaponKitPickupInterface::Execute_ReleasePickupClaim(Source);
	}
}

// 有空槽就放入；没有空槽时只有允许替换才换掉当前武器。
bool UWeaponKitInventoryComponent::TryPickup(AActor* Source, bool bAllowReplaceCurrent)
{
	if (bInventoryBusy)
	{
		return false;
	}

	TGuardValue<bool> BusyGuard(bInventoryBusy, true);
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!IsOperational() || !IsValid(Pawn) || !IsValid(Source) || Source->IsActorBeingDestroyed() ||
		!Source->GetClass()->ImplementsInterface(UWeaponKitPickupInterface::StaticClass()))
	{
		return false;
	}

	TArray<AWeaponKitRuntime*> SlotWeapons;
	if (!ReadSlotWeapons(SlotWeapons))
	{
		return false;
	}
	AWeaponKitRuntime* Current = GetCurrentWeapon();
	if (IsValid(Current) && FindSlotIndex(Current) == INDEX_NONE)
	{
		return false;
	}

	int32 EmptySlotIndex = INDEX_NONE;
	for (int32 Index = 0; Index < SlotWeapons.Num(); ++Index)
	{
		if (!IsValid(SlotWeapons[Index]))
		{
			EmptySlotIndex = Index;
			break;
		}
	}
	if (EmptySlotIndex == INDEX_NONE && !bAllowReplaceCurrent)
	{
		return false;
	}
	// 手动替换不要求重叠体积；自动拾取（不允许替换）才检查体积。
	if (!IWeaponKitPickupInterface::Execute_CanBePickedUpBy(Source, Pawn, !bAllowReplaceCurrent))
	{
		return false;
	}

	const TSubclassOf<AWeaponKitRuntime> IncomingClass = IWeaponKitPickupInterface::Execute_GetPickupWeaponClass(Source);
	if (!IsUsableWeaponClass(IncomingClass) || !IWeaponKitPickupInterface::Execute_TryClaimPickup(Source, Pawn, !bAllowReplaceCurrent))
	{
		return false;
	}
	const FWeaponKitPickupState IncomingState = IWeaponKitPickupInterface::Execute_GetPickupSavedState(Source);
	if (EmptySlotIndex == INDEX_NONE)
	{
		return TryReplaceCurrentFromPickup(Source, IncomingClass, IncomingState);
	}

	AWeaponKitRuntime* IncomingWeapon = nullptr;
	if (!CreateWeaponInSlot(SlotHosts[EmptySlotIndex], IncomingClass, IncomingState, IncomingWeapon))
	{
		ReleasePickupClaim(Source);
		return false;
	}
	if (IsValid(GetCurrentWeapon()))
	{
		SetWeaponActorActive(IncomingWeapon, false);
		BroadcastInventoryChanged();
	}
	else
	{
		CommitCurrentWeapon(IncomingWeapon);
	}
	ConsumePickupSource(Source);
	return true;
}

// 转到该槽里的武器。空槽失败。
bool UWeaponKitInventoryComponent::TryEquipSlot(int32 SlotIndex)
{
	return TryEquipWeapon(GetSlotWeapon(SlotIndex));
}

// 已经装备时直接成功。否则取消旧枪换弹再提交。
bool UWeaponKitInventoryComponent::TryEquipWeapon(AWeaponKitRuntime* NewWeapon)
{
	if (bInventoryBusy)
	{
		return false;
	}
	TGuardValue<bool> BusyGuard(bInventoryBusy, true);
	if (!IsOperational() || !IsValid(NewWeapon) || FindSlotIndex(NewWeapon) == INDEX_NONE)
	{
		return false;
	}
	AWeaponKitRuntime* OldWeapon = GetCurrentWeapon();
	if (OldWeapon == NewWeapon)
	{
		return true;
	}
	if (IsValid(OldWeapon) && FindSlotIndex(OldWeapon) == INDEX_NONE)
	{
		return false;
	}
	CancelCurrentWeaponActions(OldWeapon, true);
	CommitCurrentWeapon(NewWeapon);
	return true;
}

// 切到下一把有枪的槽，不要求表现层重播装备动画。
bool UWeaponKitInventoryComponent::TrySwitchWeapon()
{
	if (bInventoryBusy)
	{
		return false;
	}
	TGuardValue<bool> BusyGuard(bInventoryBusy, true);
	if (!IsOperational())
	{
		return false;
	}
	AWeaponKitRuntime* Current = GetCurrentWeapon();
	const int32 CurrentIndex = FindSlotIndex(Current);
	if (!IsValid(Current) || CurrentIndex == INDEX_NONE)
	{
		return false;
	}
	const int32 NextIndex = FindNextOccupiedSlot(CurrentIndex);
	AWeaponKitRuntime* NextWeapon = GetSlotWeapon(NextIndex);
	if (!IsValid(NextWeapon) || NextWeapon == Current)
	{
		return false;
	}
	Current->CancelReload();
	CommitCurrentWeapon(NextWeapon, false);
	return true;
}

// 至少留一把枪。成功丢出后再问扫描器有没有脚边的另一把。
bool UWeaponKitInventoryComponent::TryDropCurrent()
{
	if (bInventoryBusy)
	{
		return false;
	}

	AActor* DroppedPickup = nullptr;
	{
		TGuardValue<bool> BusyGuard(bInventoryBusy, true);
		APawn* Pawn = Cast<APawn>(GetOwner());
		// 只剩一把时拒绝丢弃，避免身上没有武器。
		if (!IsOperational() || !IsValid(Pawn) || CountOccupiedSlots() < 2)
		{
			return false;
		}
		AWeaponKitRuntime* Current = GetCurrentWeapon();
		const int32 CurrentIndex = FindSlotIndex(Current);
		if (!IsValid(Current) || CurrentIndex == INDEX_NONE)
		{
			return false;
		}
		const FWeaponKitPickupState DropState = Current->CapturePickupRuntimeState(HasConsumedInitialDraw(Current));
		if (!DropState.bHasSavedState)
		{
			return false;
		}
		DroppedPickup = SpawnStagedPickup(Current->GetClass(), DropState);
		if (!IsValid(DroppedPickup))
		{
			return false;
		}

		const int32 OtherIndex = FindNextOccupiedSlot(CurrentIndex);
		AWeaponKitRuntime* OtherWeapon = GetSlotWeapon(OtherIndex);
		CancelCurrentWeaponActions(Current, true);
		IWeaponKitSlotHost::Execute_ClearHostedWeapon(SlotHosts[CurrentIndex].GetObject());
		CommitCurrentWeapon(OtherWeapon);
		LaunchStagedPickup(DroppedPickup);
	}

	if (IsValid(InteractionScanner) && IsValid(DroppedPickup))
	{
		// 排除刚丢掉的那把，只自动捡起另一把已登记的拾取物。
		if (AActor* NearbyPickup = InteractionScanner->GetAutomaticPickup(DroppedPickup))
		{
			TryPickup(NearbyPickup, false);
		}
	}
	return true;
}

// 先把旧枪暂存成隐藏拾取物。新手枪失败时尝试放回原槽。
bool UWeaponKitInventoryComponent::TryReplaceCurrentFromPickup(
	AActor* Source,
	TSubclassOf<AWeaponKitRuntime> IncomingClass,
	const FWeaponKitPickupState& IncomingState)
{
	TArray<AWeaponKitRuntime*> SlotWeapons;
	if (!ReadSlotWeapons(SlotWeapons))
	{
		ReleasePickupClaim(Source);
		return false;
	}
	AWeaponKitRuntime* OldWeapon = GetCurrentWeapon();
	const int32 OldIndex = FindSlotIndex(OldWeapon);
	if (!IsValid(OldWeapon) || OldIndex == INDEX_NONE)
	{
		ReleasePickupClaim(Source);
		return false;
	}

	const FWeaponKitPickupState OldState = OldWeapon->CapturePickupRuntimeState(HasConsumedInitialDraw(OldWeapon));
	if (!OldState.bHasSavedState)
	{
		ReleasePickupClaim(Source);
		return false;
	}
	AActor* StagedOldPickup = SpawnStagedPickup(OldWeapon->GetClass(), OldState);
	if (!IsValid(StagedOldPickup))
	{
		ReleasePickupClaim(Source);
		return false;
	}

	const TSubclassOf<AWeaponKitRuntime> OldClass = OldWeapon->GetClass();
	CancelCurrentWeaponActions(OldWeapon, true);
	AWeaponKitRuntime* IncomingWeapon = nullptr;
	// 新手枪成功后才抛出旧枪并销毁地上的来源。
	if (CreateWeaponInSlot(SlotHosts[OldIndex], IncomingClass, IncomingState, IncomingWeapon))
	{
		CommitCurrentWeapon(IncomingWeapon);
		LaunchStagedPickup(StagedOldPickup);
		ConsumePickupSource(Source);
		return true;
	}

	AWeaponKitRuntime* RestoredOldWeapon = nullptr;
	if (CreateWeaponInSlot(SlotHosts[OldIndex], OldClass, OldState, RestoredOldWeapon))
	{
		CommitCurrentWeapon(RestoredOldWeapon);
		StagedOldPickup->Destroy();
		ReleasePickupClaim(Source);
		return false;
	}

	IWeaponKitSlotHost::Execute_ClearHostedWeapon(SlotHosts[OldIndex].GetObject());
	const int32 OtherIndex = FindNextOccupiedSlot(OldIndex);
	CommitCurrentWeapon(GetSlotWeapon(OtherIndex));
	LaunchStagedPickup(StagedOldPickup);
	ReleasePickupClaim(Source);
	UE_LOG(LogWeaponKitInventory, Error, TEXT("Could not create the incoming weapon or restore the replaced weapon; the old weapon remains on the ground."));
	return false;
}

// 置忙标记，取消当前换弹并忘掉槽位记录。不销毁槽组件里的武器。
void UWeaponKitInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bInventoryBusy = true;
	if (AWeaponKitRuntime* Weapon = GetCurrentWeapon())
	{
		CancelCurrentWeaponActions(Weapon, false);
	}
	CurrentWeapon.Reset();
	ActiveSlotIndex = INDEX_NONE;
	SlotHosts.Reset();
	bInitialized = false;
	bExternalRestorePresentationPending = false;
	bCheckpointLoadoutPendingInitialization = false;
	bCheckpointRestoreFailed = false;
	Super::EndPlay(EndPlayReason);
}
