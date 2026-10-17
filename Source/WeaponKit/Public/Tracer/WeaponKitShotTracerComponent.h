/**
 * 射击后播放 Niagara 曳光。
 * 伤害射线由游戏完成；射线成功后调用 PlayShotTracer。插件不引用任何 /Game 特效路径，特效资产由组件属性指定。
 */
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponKitShotTracerComponent.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

/**
 * 曳光组件。挂在武器或角色上均可。
 * Niagara 参数名可改，默认仍是 User.TracerDistance 等，旧特效不用改名。
 */
UCLASS(ClassGroup = (WeaponKit), meta = (BlueprintSpawnableComponent))
class WEAPONKIT_API UWeaponKitShotTracerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 关闭 Tick。曳光寿命由 Niagara 自己管理。 */
	UWeaponKitShotTracerComponent();

	/** 为 false 时 PlayShotTracer 直接失败，不生成特效。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracer", meta = (DisplayName = "启用曳光"))
	bool bTracerEnabled = true;

	/** 要生成的 Niagara 系统。未指定时播放失败。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracer", meta = (DisplayName = "曳光特效"))
	TObjectPtr<UNiagaraSystem> TracerSystem;

	/** 视觉飞行速度（厘米/秒）。只影响特效时长，不影响伤害射线。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracer", meta = (DisplayName = "视觉飞行速度", ClampMin = "1.0", Units = "cm/s"))
	float TravelSpeed = 30000.0f;

	/** 视觉飞行时间上限（秒）。超远距离不会让曳光拖得比这个更久。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracer", meta = (DisplayName = "最长视觉飞行时间", ClampMin = "0.016667", ClampMax = "1.0", Units = "s"))
	float MaxTravelDuration = 0.15f;

	/** 亮线长度（厘米）。近距离命中时会被截成不超过枪口到落点的距离。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracer", meta = (DisplayName = "亮线长度", ClampMin = "0.01", Units = "cm"))
	float StreakLength = 80.0f;

	/** 亮线宽度（厘米），写入宽度参数。非正数时播放失败。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracer", meta = (DisplayName = "亮线宽度", ClampMin = "0.01", Units = "cm"))
	float StreakWidth = 1.2f;

	/** 写入颜色参数的线性颜色。通道不是有限数时播放失败。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracer", meta = (DisplayName = "曳光颜色"))
	FLinearColor TracerColor = FLinearColor(1.0f, 0.65f, 0.25f, 1.0f);

	/** 同时存活的曳光上限。达到上限后新的播放失败，默认 64。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracer", meta = (DisplayName = "同时存在上限", ClampMin = "1"))
	int32 MaxActiveTracers = 64;

	/** 距离用户参数名。默认 User.TracerDistance。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracer|Parameters", meta = (DisplayName = "距离参数"))
	FName DistanceParameter = TEXT("User.TracerDistance");

	/** 寿命用户参数名。默认 User.TracerLifetime。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracer|Parameters", meta = (DisplayName = "寿命参数"))
	FName LifetimeParameter = TEXT("User.TracerLifetime");

	/** 长度用户参数名。默认 User.TracerLength。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracer|Parameters", meta = (DisplayName = "长度参数"))
	FName LengthParameter = TEXT("User.TracerLength");

	/** 宽度用户参数名。默认 User.TracerWidth。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracer|Parameters", meta = (DisplayName = "宽度参数"))
	FName WidthParameter = TEXT("User.TracerWidth");

	/** 颜色用户参数名。默认 User.TracerColor。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracer|Parameters", meta = (DisplayName = "颜色参数"))
	FName ColorParameter = TEXT("User.TracerColor");

	/**
	 * 伤害射线成功后调用一次。
	 * MuzzleLocation 是枪口，ShotEnd 是命中点或射线终点。专用服务器、未配置特效或参数名是 None 时返回 false。
	 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Tracer", meta = (ReturnDisplayName = "Spawned"))
	bool PlayShotTracer(FVector MuzzleLocation, FVector ShotEnd);

	/** 销毁仍在跟踪的曳光组件。组件 EndPlay 时也会调用。 */
	UFUNCTION(BlueprintCallable, Category = "WeaponKit|Tracer")
	void ClearShotTracers();

protected:
	/** 结束时清掉曳光，避免组件销毁后特效残留。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 当前存活的曳光。失效的弱指针会在下次播放前剔除。 */
	TArray<TWeakObjectPtr<UNiagaraComponent>> ActiveTracers;
};
