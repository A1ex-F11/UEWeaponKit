// 生成 Niagara 曳光并写入可配置的用户参数。不做伤害检测。
#include "Tracer/WeaponKitShotTracerComponent.h"
#include "Math/ShotTracerGeometry.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"

// 不 Tick。特效寿命由 Niagara 系统自己结束。
UWeaponKitShotTracerComponent::UWeaponKitShotTracerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// 专用服务器、未指定特效、参数名是 None 或几何不合法时直接失败。
bool UWeaponKitShotTracerComponent::PlayShotTracer(FVector MuzzleLocation, FVector ShotEnd)
{
	UWorld* World = GetWorld();
	if (!bTracerEnabled || !IsValid(TracerSystem) || !World || World->GetNetMode() == NM_DedicatedServer ||
		!FMath::IsFinite(StreakWidth) || StreakWidth <= 0.0f ||
		!FMath::IsFinite(TracerColor.R) || !FMath::IsFinite(TracerColor.G) ||
		!FMath::IsFinite(TracerColor.B) || !FMath::IsFinite(TracerColor.A) ||
		DistanceParameter.IsNone() || LifetimeParameter.IsNone() || LengthParameter.IsNone() ||
		WidthParameter.IsNone() || ColorParameter.IsNone())
	{
		return false;
	}

	float Distance = 0.0f;
	float Duration = 0.0f;
	float Length = 0.0f;
	if (!FWeaponKitShotTracerGeometry::BuildFlight(
		MuzzleLocation, ShotEnd, TravelSpeed, MaxTravelDuration, StreakLength, Distance, Duration, Length))
	{
		return false;
	}

	// 先丢掉已经自然结束的曳光，再检查同时存在上限。
	ActiveTracers.RemoveAll([](const TWeakObjectPtr<UNiagaraComponent>& Item) { return !Item.IsValid(); });
	// 达到上限就放弃这一发，避免特效无限堆积。
	if (ActiveTracers.Num() >= FMath::Max(1, MaxActiveTracers))
	{
		return false;
	}

	UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		TracerSystem,
		MuzzleLocation,
		(ShotEnd - MuzzleLocation).Rotation(),
		FVector::OneVector,
		true,
		false,
		ENCPoolMethod::None,
		false);
	if (!Effect)
	{
		return false;
	}

	Effect->SetVariableFloat(DistanceParameter, Distance);
	Effect->SetVariableFloat(LifetimeParameter, Duration);
	Effect->SetVariableFloat(LengthParameter, Length);
	Effect->SetVariableFloat(WidthParameter, StreakWidth);
	Effect->SetVariableLinearColor(ColorParameter, TracerColor);
	ActiveTracers.Add(Effect);
	Effect->Activate(true);
	return true;
}

// 主动销毁仍在跟踪的曳光组件。
void UWeaponKitShotTracerComponent::ClearShotTracers()
{
	for (const TWeakObjectPtr<UNiagaraComponent>& Item : ActiveTracers)
	{
		if (UNiagaraComponent* Effect = Item.Get())
		{
			Effect->DestroyComponent();
		}
	}
	ActiveTracers.Reset();
}

// 组件结束时清曳光，再走父类。
void UWeaponKitShotTracerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearShotTracers();
	Super::EndPlay(EndPlayReason);
}
