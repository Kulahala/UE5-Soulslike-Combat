#pragma once

#include "CoreMinimal.h"
#include "PlayerAttackMotionWarpingConfig.generated.h"

USTRUCT(BlueprintType)
struct FPlayerAttackMotionWarpingConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping", meta = (ToolTip = "锁定目标时是否为该攻击启用短距离 Motion Warping。蒙太奇 NotifyState 的 Warp Target Name 固定使用 AttackTarget。"))
	bool bUseMotionWarping = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping", meta = (EditCondition = "bUseMotionWarping", ClampMin = "0.0", ToolTip = "修正后停在锁定目标前方的距离（cm）。"))
	float WarpStopDistance = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping", meta = (EditCondition = "bUseMotionWarping", ClampMin = "0.0", ToolTip = "从玩家当前位置到 WarpLocation 的最大允许修正距离（cm）。超出时不启用，避免远距离吸附。"))
	float MaxWarpDistance = 60.f;
};
