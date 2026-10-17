// 从已登记的拾取物里按距离和视线选目标。背包不负责这项判断。
#include "Runtime/WeaponKitInteractionScanner.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

// 不 Tick。有登记目标时用定时器刷新瞄准结果。
UWeaponKitInteractionScanner::UWeaponKitInteractionScanner()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// 没实现携带者接口时视为存活。拥有者无效时视为死亡。
bool UWeaponKitInteractionScanner::IsCarrierAlive() const
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

// 只接受实现了拾取接口的 Actor。首次登记时启动重复定时器。
bool UWeaponKitInteractionScanner::RegisterPickup(AActor* Pickup)
{
	if (!IsValid(Pickup) || !Pickup->GetClass()->ImplementsInterface(UWeaponKitPickupInterface::StaticClass()))
	{
		return false;
	}
	NearbyPickups.AddUnique(Pickup);
	if (UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		if (!Timers.IsTimerActive(RefreshTimerHandle))
		{
			Timers.SetTimer(
				RefreshTimerHandle,
				this,
				&UWeaponKitInteractionScanner::RefreshAimedPickup,
				FMath::Max(0.01f, RefreshInterval),
				true);
		}
	}
	RefreshAimedPickup();
	return true;
}

// 移除指定项，并顺手丢掉已经失效的弱指针。
void UWeaponKitInteractionScanner::UnregisterPickup(AActor* Pickup)
{
	NearbyPickups.RemoveAll([Pickup](const TWeakObjectPtr<AActor>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Pickup;
	});
	RefreshAimedPickup();
}

// 手动拾取候选，不排除任何 Actor。
AActor* UWeaponKitInteractionScanner::GetManualPickup() const
{
	return FindPickup(false, nullptr);
}

// 自动拾取候选。ExcludedPickup 用来跳过刚丢掉的那一把。
AActor* UWeaponKitInteractionScanner::GetAutomaticPickup(AActor* ExcludedPickup) const
{
	return FindPickup(true, ExcludedPickup);
}

// 在手动候选上再打一条视线，只有射线打中它才算瞄准。
AActor* UWeaponKitInteractionScanner::GetAimedPickup() const
{
	AActor* Candidate = GetManualPickup();
	UWorld* World = GetWorld();
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(Candidate) || !World || !IsValid(OwnerActor))
	{
		return nullptr;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	if (const APawn* Pawn = Cast<APawn>(OwnerActor))
	{
		if (APlayerController* Controller = Cast<APlayerController>(Pawn->GetController()))
		{
			Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		}
		else
		{
			Pawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);
		}
	}
	else
	{
		OwnerActor->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponKitAimedPickup), false, OwnerActor);
	if (const APawn* Pawn = Cast<APawn>(OwnerActor))
	{
		TArray<AActor*> AttachedActors;
		Pawn->GetAttachedActors(AttachedActors, true, true);
		for (AActor* AttachedActor : AttachedActors)
		{
			QueryParams.AddIgnoredActor(AttachedActor);
		}
	}

	const float TraceLength = MaxDistance + FVector::Distance(ViewLocation, OwnerActor->GetActorLocation());
	FHitResult AimHit;
	return World->LineTraceSingleByChannel(
			AimHit,
			ViewLocation,
			ViewLocation + ViewRotation.Vector() * TraceLength,
			TraceChannel,
			QueryParams) &&
		AimHit.GetActor() == Candidate
		? Candidate
		: nullptr;
}

// 先用视线命中；没打中则在距离内、视线不被挡住的登记项里选最近的。
AActor* UWeaponKitInteractionScanner::FindPickup(bool bAutomaticOnly, AActor* ExcludedPickup) const
{
	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	const APawn* Pawn = Cast<APawn>(OwnerActor);
	if (!World || !IsValid(OwnerActor) || !IsValid(Pawn) || !IsCarrierAlive() || MaxDistance <= 0.0f)
	{
		return nullptr;
	}

	const float MaxDistanceSq = FMath::Square(MaxDistance);
	auto IsRegisteredAndInRange = [this, Pawn, MaxDistanceSq, bAutomaticOnly, ExcludedPickup](AActor* Candidate)
	{
		if (!IsValid(Candidate) || Candidate == ExcludedPickup ||
			!Candidate->GetClass()->ImplementsInterface(UWeaponKitPickupInterface::StaticClass()) ||
			!IWeaponKitPickupInterface::Execute_CanBePickedUpBy(Candidate, Pawn, bAutomaticOnly) ||
			FVector::DistSquared(GetOwner()->GetActorLocation(), Candidate->GetActorLocation()) > MaxDistanceSq)
		{
			return false;
		}
		for (const TWeakObjectPtr<AActor>& Entry : NearbyPickups)
		{
			if (Entry.Get() == Candidate)
			{
				return true;
			}
		}
		return false;
	};

	FVector ViewLocation;
	FRotator ViewRotation;
	if (APlayerController* Controller = Cast<APlayerController>(Pawn->GetController()))
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else
	{
		Pawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponKitPickup), false, Pawn);
	if (IsValid(ExcludedPickup))
	{
		QueryParams.AddIgnoredActor(ExcludedPickup);
	}
	TArray<AActor*> AttachedActors;
	Pawn->GetAttachedActors(AttachedActors, true, true);
	for (AActor* AttachedActor : AttachedActors)
	{
		QueryParams.AddIgnoredActor(AttachedActor);
	}

	const float TraceLength = MaxDistance + FVector::Distance(ViewLocation, Pawn->GetActorLocation());
	FHitResult AimHit;
	if (World->LineTraceSingleByChannel(
			AimHit,
			ViewLocation,
			ViewLocation + ViewRotation.Vector() * TraceLength,
			TraceChannel,
			QueryParams) &&
		IsRegisteredAndInRange(AimHit.GetActor()))
	{
		// 视线已经打在合格拾取物上，优先于纯距离选择。
		return AimHit.GetActor();
	}

	AActor* Nearest = nullptr;
	float NearestDistanceSq = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<AActor>& Entry : NearbyPickups)
	{
		AActor* Candidate = Entry.Get();
		if (!IsRegisteredAndInRange(Candidate))
		{
			continue;
		}
		const float DistanceSq = FVector::DistSquared(Pawn->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSq >= NearestDistanceSq)
		{
			continue;
		}
		FVector TargetPoint = FVector::ZeroVector;
		FVector BoundsExtent = FVector::ZeroVector;
		Candidate->GetActorBounds(false, TargetPoint, BoundsExtent);
		FHitResult SightHit;
		if (World->LineTraceSingleByChannel(SightHit, ViewLocation, TargetPoint, TraceChannel, QueryParams) &&
			SightHit.GetActor() != Candidate)
		{
			continue;
		}
		Nearest = Candidate;
		NearestDistanceSq = DistanceSq;
	}
	return Nearest;
}

// 列表里是否还有没被销毁的拾取物。
bool UWeaponKitInteractionScanner::HasRegisteredPickup() const
{
	for (const TWeakObjectPtr<AActor>& Entry : NearbyPickups)
	{
		if (IsValid(Entry.Get()))
		{
			return true;
		}
	}
	return false;
}

// 目标出现、消失或换成另一把时才广播，避免每个间隔都通知表现层。
void UWeaponKitInteractionScanner::RefreshAimedPickup()
{
	const bool bHasCandidate = HasRegisteredPickup();
	AActor* NewAimed = (bHasCandidate && IsCarrierAlive()) ? GetAimedPickup() : nullptr;
	const bool bHasAim = IsValid(NewAimed);
	if (bHadAimedPickup != bHasAim || (bHasAim && LastAimedPickup.Get() != NewAimed))
	{
		bHadAimedPickup = bHasAim;
		LastAimedPickup = NewAimed;
		OnAimedPickupChanged.Broadcast(NewAimed);
	}
	if (!bHasCandidate)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RefreshTimerHandle);
		}
	}
}

// 停止定时器。若界面还显示着瞄准目标，广播空指针清掉。
void UWeaponKitInteractionScanner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	if (bHadAimedPickup)
	{
		OnAimedPickupChanged.Broadcast(nullptr);
	}
	NearbyPickups.Reset();
	LastAimedPickup.Reset();
	bHadAimedPickup = false;
	Super::EndPlay(EndPlayReason);
}
