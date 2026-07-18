#pragma once

#include "CoreMinimal.h"
#include "Items/Weapon/Weapon.h"
#include "BowBase.generated.h"

/**
 * Bow 的共享物理身份：只提供默认左手附着合同。
 * 玩家弹药/瞄准与敌人攻击决策分别留在各自系统。
 */
UCLASS(Abstract)
class TEST_API ABowBase : public AWeapon
{
	GENERATED_BODY()

public:
	ABowBase();
};
