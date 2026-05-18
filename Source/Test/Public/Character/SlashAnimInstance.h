#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Character/CharacterTypes.h"
#include "SlashAnimInstance.generated.h"

class UCharacterMovementComponent;
class AMyCharacter;

UCLASS()
class TEST_API USlashAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References", meta = (AllowPrivateAccess = "true", ToolTip = "拥有此动画实例的角色引用。"))
	AMyCharacter* MyCharacter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References", meta = (AllowPrivateAccess = "true", ToolTip = "角色移动组件引用。"))
	UCharacterMovementComponent* CharacterMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true", ToolTip = "地速（2D），驱动 BlendSpace。"))
	float GroundSpeed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true", ToolTip = "是否在空中。"))
	bool IsFalling;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true", ToolTip = "垂直速度，用于跳跃/下落动画。"))
	float ZSpeed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true", ToolTip = "移动方向（-180~180），驱动 BlendSpace。"))
	float Direction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true", ToolTip = "当前武器装备状态。"))
	EWeaponState WeaponState = EWeaponState::EWS_Unequipped;

	// 是否正在防御
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true", ToolTip = "是否正在防御状态。"))
	bool bIsBlocking = false;

	// 是否处于受击硬直
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true", ToolTip = "是否处于受击硬直状态。"))
	bool bIsStunning = false;

};
