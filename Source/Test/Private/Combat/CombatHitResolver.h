#pragma once

#include "Combat/CombatHitTypes.h"

/**
 * 近战和投射物共用的已命中结算入口。
 * 投递端负责碰撞、命中次数和自身表现；本类只结算目标的战斗结果。
 */
class FCombatHitResolver
{
public:
	static FCombatHitResult ResolveAndApply(const FCombatHitRequest& Request);
};
