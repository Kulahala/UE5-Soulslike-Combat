#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BaseCharacterAnimInstance.generated.h"

class ABaseCharacter;
class UCharacterMovementComponent;

// ABaseCharacter 根 AnimBP 的共享原生动画数据；不持有 Skeleton 专属资产或战斗状态策略。
UCLASS(BlueprintType)
class TEST_API UBaseCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	FORCEINLINE ABaseCharacter* GetBaseCharacter() const { return BaseCharacter; }

private:
	void RefreshCharacterReferences();
	void ResetMovementAnimationData();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "References", meta = (AllowPrivateAccess = "true", ToolTip = "拥有此动画实例的角色基类引用。"))
	ABaseCharacter* BaseCharacter = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "References", meta = (AllowPrivateAccess = "true", ToolTip = "角色移动组件引用。"))
	UCharacterMovementComponent* CharacterMovement = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true", ToolTip = "地速（2D），驱动 BlendSpace。"))
	float GroundSpeed = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true", ToolTip = "是否在空中。"))
	bool IsFalling = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true", ToolTip = "垂直速度，用于跳跃/下落动画。"))
	float ZSpeed = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true", ToolTip = "移动方向（-180~180），驱动 BlendSpace。"))
	float Direction = 0.f;
};
