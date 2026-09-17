#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Items/Bow/BowBase.h"
#include "BowAnimInstance.generated.h"

/**
 * 共享 Bow 的表现状态桥接。它只读取拥有它的 ABowBase，不拥有输入、弹药或发射时机。
 */
UCLASS()
class TEST_API UBowAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	void RefreshBowReference();

	UPROPERTY(Transient)
	ABowBase* Bow = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true", ToolTip = "拥有 Bow 的只读表现状态，仅供 Bow 自身 AnimBP 选择动画。"))
	EBowPresentationState BowPresentationState = EBowPresentationState::EBPS_Relaxed;
};
