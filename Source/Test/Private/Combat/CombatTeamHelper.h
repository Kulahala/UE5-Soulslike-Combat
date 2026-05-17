#pragma once

#include "GameFramework/Actor.h"

struct FCombatTeamHelper
{
	static bool ShareTeamTag(const AActor* A, const AActor* B)
	{
		if (!A || !B)
		{
			return false;
		}

		for (const FName& Tag : A->Tags)
		{
			if (B->ActorHasTag(Tag))
			{
				return true;
			}
		}

		return false;
	}
};
