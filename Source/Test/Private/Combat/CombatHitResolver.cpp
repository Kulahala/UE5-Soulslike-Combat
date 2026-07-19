#include "Combat/CombatHitResolver.h"

#include "Character/BaseCharacter.h"
#include "Character/MyCharacter.h"
#include "Combat/CombatTeamHelper.h"
#include "Enemy/Enemy.h"
#include "Interfaces/BlockableInterface.h"
#include "Interfaces/HitInterface.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	void MarkCombatPresenceForConfirmedPlayerEnemyHit(const FCombatHitRequest& Request,
		const FCombatHitResult& Result)
	{
		if (!Result.bResolved || Result.bSuppressed || Result.bSameTeam)
		{
			return;
		}

		if (const AEnemy* AttackingEnemy = Cast<AEnemy>(Request.Attacker);
			AttackingEnemy && AttackingEnemy->IsEncounterDormant())
		{
			return;
		}

		if (AMyCharacter* AttackingPlayer = Cast<AMyCharacter>(Request.Attacker);
			AttackingPlayer && Cast<AEnemy>(Request.HitActor))
		{
			AttackingPlayer->MarkCombatPresenceFromConfirmedHostileHit();
		}
		else if (AMyCharacter* HitPlayer = Cast<AMyCharacter>(Request.HitActor);
			HitPlayer && Cast<AEnemy>(Request.Attacker))
		{
			HitPlayer->MarkCombatPresenceFromConfirmedHostileHit();
		}
	}
}

FCombatHitResult FCombatHitResolver::ResolveAndApply(const FCombatHitRequest& Request)
{
	FCombatHitResult Result;
	if (!IsValid(Request.HitActor))
	{
		return Result;
	}

	Result.bResolved = true;
	Result.FinalDamage = Request.IncomingDamage;

	if (const AEnemy* HitEnemy = Cast<AEnemy>(Request.HitActor); HitEnemy && HitEnemy->IsEncounterDormant())
	{
		// 遭遇待命敌人与近战路径一致：不扣血，也不产生任何命中反馈。
		Result.bSuppressed = true;
		return Result;
	}

	Result.bSameTeam = FCombatTeamHelper::ShareTeamTag(Request.Attacker, Request.HitActor);
	if (!Result.bSameTeam)
	{
		if (IBlockableInterface* Blockable = Cast<IBlockableInterface>(Request.HitActor))
		{
			const FBlockResult BlockResult = Blockable->TryBlockHit(Request);
			Result.bBlocked = BlockResult.bBlocked;
			Result.bParried = BlockResult.bParried;

			if (BlockResult.bBlocked)
			{
				Result.FinalDamage = BlockResult.DamageAfterBlock;
				Result.bPlayNormalHitReact = BlockResult.bPlayNormalHitReact;
				Result.KnockbackScale = Request.IncomingDamage > 0.f
					? BlockResult.DamageAfterBlock / Request.IncomingDamage
					: 0.f;
				Result.bApplyStun = BlockResult.bPlayNormalHitReact;
			}
		}

		UGameplayStatics::ApplyDamage(Request.HitActor, Result.FinalDamage, Request.EventInstigator,
			Request.DamageCauser, UDamageType::StaticClass());

		if (AEnemy* HitEnemy = Cast<AEnemy>(Request.HitActor); HitEnemy && Request.bApplyPoiseDamage)
		{
			HitEnemy->ApplyPoiseDamage(Request.PoiseDamage, Request.Attacker);
		}
	}

	if (Result.bParried)
	{
		if (AEnemy* AttackerEnemy = Cast<AEnemy>(Request.Attacker))
		{
			AttackerEnemy->ApplyPoiseDamage(AttackerEnemy->GetCurrentPoise(), Request.HitActor);
		}
	}

	if (ABaseCharacter* HitCharacter = Cast<ABaseCharacter>(Request.HitActor))
	{
		HitCharacter->CachePendingHitContext(Request.Attacker, Result.KnockbackScale,
			!Result.bPlayNormalHitReact, Result.bApplyStun);
	}

	if (Request.HitActor->Implements<UHitInterface>())
	{
		IHitInterface::Execute_GetHit(Request.HitActor, Request.HitResult.ImpactPoint, Request.Attacker);
	}

	if (Result.bParried)
	{
		if (AEnemy* AttackerEnemy = Cast<AEnemy>(Request.Attacker);
			AttackerEnemy && AttackerEnemy->ShouldTriggerStanceBreak())
		{
			AttackerEnemy->ApplyStanceBreak();
		}
	}
	else if (AEnemy* HitEnemy = Cast<AEnemy>(Request.HitActor);
		HitEnemy && HitEnemy->ShouldTriggerStanceBreak())
	{
		HitEnemy->ApplyStanceBreak();
	}

	MarkCombatPresenceForConfirmedPlayerEnemyHit(Request, Result);

	return Result;
}
