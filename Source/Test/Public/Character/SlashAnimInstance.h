#pragma once

#include "CoreMinimal.h"
#include "Character/BaseCharacterAnimInstance.h"
#include "Character/CharacterTypes.h"
#include "SlashAnimInstance.generated.h"

class AMyCharacter;

UCLASS()
class TEST_API USlashAnimInstance : public UBaseCharacterAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References", meta = (AllowPrivateAccess = "true", ToolTip = "拥有此动画实例的角色引用。"))
	AMyCharacter* MyCharacter = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true", ToolTip = "当前武器装备状态。"))
	EWeaponState WeaponState = EWeaponState::EWS_Unequipped;

	// 是否正在防御
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true", ToolTip = "是否正在防御状态。"))
	bool bIsBlocking = false;

	// 是否处于受击硬直
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true", ToolTip = "是否处于受击硬直状态。"))
	bool bIsStunning = false;

};
