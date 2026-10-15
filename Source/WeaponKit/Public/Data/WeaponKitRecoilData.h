/**
 * 后坐序列、随机冲量、镜头反馈和恢复参数。
 * 作为枪械配置的内联子对象。武器用它采样每一发的俯仰 / 偏航，镜头组件用它决定写到控制器还是只偏移摄像机。
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/WeaponKitRecoilData.generated.h"

/**
 * 一把枪的后坐与镜头设置。
 * 固定序列和随机上抬二选一：bUseFixedPattern 为 true 时忽略随机范围。
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, meta = (DisplayName = "后坐与镜头设置"))
class WEAPONKIT_API UWeaponKitRecoilData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 把固定序列、平滑和随机范围设成构造期默认值。具体数值见实现。 */
	UWeaponKitRecoilData();

	/** 瞄准时把当前后坐序列乘这个倍率后写入视线。1 保持原强度，0 关闭瞄准后坐。不影响腰射和射速。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "右键瞄准后坐", meta = (DisplayName = "右键后坐强度倍率", ClampMin = "0.0", ToolTip = "复用当前后坐序列，缩放右键时的瞄准视线冲量；1保持序列强度，0关闭。腰射和射速不变。"))
	float AimingRecoilScale = 1.0f;

	/** 为 true 时按 FixedPattern 逐发采样；为 false 时每发在随机上抬和左右范围内取值。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "固定后坐轨迹", meta = (DisplayName = "使用固定后坐序列", ToolTip = "开启后按数组逐发采样；关闭后使用随机上抬和左右范围。"))
	bool bUseFixedPattern = false;

	/**
	 * 为 true 且使用固定序列时，腰射的射击方向由武器单独累计，镜头只做画面反馈。
	 * 瞄准开火不走这条分离路径，冲量会写进控制旋转。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "射击方向与镜头", meta = (DisplayName = "射击后坐与镜头分离", ToolTip = "开启后镜头仅提供画面反馈，射击方向偏移由武器单独计算。"))
	bool bUseIndependentShotRecoil = false;

	/** 只缩放分离模式下的镜头反馈。可以大于 1；0 表示镜头不动。不改变射击方向。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "射击方向与镜头", meta = (DisplayName = "镜头反馈倍率", ClampMin = "0.0", UIMax = "5.0", ToolTip = "仅缩放镜头反馈；允许大于1，0关闭反馈，不改变射击方向。"))
	float CameraFeedbackScale = 0.25f;

	/** 分离模式下镜头偏移回到零的速度。越大回落越快，最小按 0.1 使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "射击方向与镜头", meta = (DisplayName = "镜头回落速度", ToolTip = "镜头冲击回到正常视角的速度；越大回落越快。", ClampMin = "0.1"))
	float CameraReturnSpeed = 12.0f;

	/** 分离模式下，停火超过恢复延迟后，弹道后坐每秒收回的角度。停火达到序列重置时间则整段清零。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "射击方向与镜头", meta = (DisplayName = "射击后坐恢复速度", ClampMin = "0.0", ToolTip = "停火超过恢复延迟后，每秒恢复的角度；停火达到序列重置时间则完整重置。"))
	float ShotRecoilRecoverySpeed = 8.0f;

	/** 固定后坐序列。X 为非负俯仰增量（度），Y 为带符号的偏航增量（度）。空数组不会产生冲量。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "固定后坐轨迹", meta = (DisplayName = "固定后坐序列", ToolTip = "X 为非负俯仰增量，Y 为带符号的偏航增量。"))
	TArray<FVector2D> FixedPattern;

	/** 停火达到这么多秒后，序列从第一项重新开始，并清掉累计弹道后坐。0 表示每枪都从首项开始，适合手枪。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "固定后坐轨迹", meta = (DisplayName = "序列停火重置时间", ToolTip = "停火达到此秒数后从首项重新开始；0表示每枪清除前发射击偏移，适合手枪。", ClampMin = "0.0"))
	float PatternResetDelay = 0.25f;

	/** 只缩放固定序列的上抬。1 保持原值，0 关闭上抬。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "固定后坐轨迹", meta = (DisplayName = "上抬倍率", ClampMin = "0.0", ToolTip = "仅缩放固定轨迹的上抬量；1保持原值，0关闭上抬。"))
	float FixedPatternPitchScale = 1.0f;

	/** 只缩放固定序列的左右偏移。1 保持原值，0 关闭左右。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "固定后坐轨迹", meta = (DisplayName = "左右倍率", ClampMin = "0.0", ToolTip = "仅缩放固定轨迹的左右量；1保持原值，0关闭左右偏移。"))
	float FixedPatternYawScale = 1.0f;

	/** 序列走完后的循环起点。-1 表示停在最后一项；其他值会被钳到有效下标。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "固定后坐轨迹", meta = (DisplayName = "尾段循环起始序号", ToolTip = "-1 表示到达末项后保持末项；其他值会限制在有效序列范围内。"))
	int32 LoopStartIndex = -1;

	/** 每发剩余镜头冲量平滑施加的秒数。0 表示立即施加整发。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "镜头冲量平滑", meta = (DisplayName = "平滑施加时长", ToolTip = "每发剩余镜头冲量平滑施加的秒数；0表示立即施加。", ClampMin = "0.0"))
	float SmoothKickDuration = 0.05f;

	/** 单发冲量里立刻施加的比例，其余部分按平滑时长播完。取值会被钳到 0 到 1。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "镜头冲量平滑", meta = (DisplayName = "立即施加比例", ToolTip = "单发冲量中立即施加的比例，剩余部分按平滑时长施加。", ClampMin = "0.0", ClampMax = "1.0"))
	float ImmediateKickFraction = 0.25f;

	/** 最后一发之后等待这么多秒，才开始自动收回控制器后坐。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "后坐恢复", meta = (DisplayName = "恢复延迟", ToolTip = "最后一发后等待多少秒开始恢复后坐。", ClampMin = "0.0"))
	float RecoveryDelay = 0.2f;

	/** 控制器后坐模式的自动恢复速度（度/秒）。分离模式改用射击后坐恢复速度和镜头回落速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "后坐恢复", meta = (DisplayName = "恢复速度", ToolTip = "控制器后坐模式的自动恢复速度；分离模式使用射击后坐恢复速度和镜头回落速度。", ClampMin = "0.0"))
	float RecoverySpeed = 1.0f;

	/** 停火后自动收回的上抬比例。1 表示全部收回。不改变分离模式的弹道累计。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "后坐恢复", meta = (DisplayName = "俯仰恢复比例", ToolTip = "停火后自动回收上抬的比例；不改变分离模式弹道。", ClampMin = "0.0", ClampMax = "1.0"))
	float RecoveryPitchRatio = 0.35f;

	/** 关闭固定序列时，每发随机上抬的最小角度。若大于最大值，运行时会交换两者。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "随机后坐", meta = (DisplayName = "随机上抬最小值", ToolTip = "关闭固定序列时，每发随机上抬的最小角度。"))
	float VerticalKickMin = 0.35f;

	/** 关闭固定序列时，每发随机上抬的最大角度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "随机后坐", meta = (DisplayName = "随机上抬最大值", ToolTip = "关闭固定序列时，每发随机上抬的最大角度。"))
	float VerticalKickMax = 0.50f;

	/** 关闭固定序列时，每发左右摆动的最大角度。实际采样范围是 ±该值。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "随机后坐", meta = (DisplayName = "随机左右最大值", ToolTip = "关闭固定序列时，每发左右摆动的最大角度。"))
	float HorizontalKickMax = 0.08f;
};
