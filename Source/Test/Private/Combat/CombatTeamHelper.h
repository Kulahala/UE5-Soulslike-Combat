#pragma once

#include "GameFramework/Actor.h"

struct FCombatTeamHelper
{
	inline static const FName PlayerTeamTag = FName("Player");
	inline static const FName EnemyTeamTag = FName("Enemy");

	static bool ShareTeamTag(const AActor* A, const AActor* B)
	{
		if (!A || !B)
		{
			return false;
		}

		return (A->ActorHasTag(PlayerTeamTag) && B->ActorHasTag(PlayerTeamTag))
			|| (A->ActorHasTag(EnemyTeamTag) && B->ActorHasTag(EnemyTeamTag));
	}
};
