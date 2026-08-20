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
	/**
	 * 战斗感知状态刷新辅助函数 (Combat Presence)：
	 * 当发生了一次真实的、非友军、非遭遇待命怪的有效命中（哪怕被格挡弹反或者伤害为 0），
	 * 刷新玩家的交战感知时间戳 (LastCombatPresenceTime)，使玩家在接下来的数秒内（默认 4 秒）
	 * 保持“交战战斗姿态”（在此期间冲刺 Shift 会持续消耗体力，并重置体力自然恢复的冷却延迟）。
	 */
	void MarkCombatPresenceForConfirmedPlayerEnemyHit(const FCombatHitRequest& Request,
		const FCombatHitResult& Result)
	{
		// 未成功结算、被待命抑制或为同阵营友军时，不刷新交战状态
		if (!Result.bResolved || Result.bSuppressed || Result.bSameTeam)
		{
			return;
		}

		// 若攻击方是处于遭遇战待命态 (Dormant) 的敌人，不产生交战感知
		if (const AEnemy* AttackingEnemy = Cast<AEnemy>(Request.Attacker);
			AttackingEnemy && AttackingEnemy->IsEncounterDormant())
		{
			return;
		}

		// 玩家攻击敌人，或敌人攻击玩家：刷新玩家的战斗交战时间戳
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

/**
 * 全局统一战斗命中结算管线入口
 * 由近战武器扫掠 (AWeapon) 或投射物碰撞 (ACombatProjectile) 在检测到目标后调用。
 * 纯 C++ 静态函数，负责整场伤害/削韧/受击反馈生命周期的单一事实裁决。
 */
FCombatHitResult FCombatHitResolver::ResolveAndApply(const FCombatHitRequest& Request)
{
	FCombatHitResult Result;

	// 1. 目标合法性校验：受击者为空或已被销毁，直接返回空结果
	if (!IsValid(Request.HitActor))
	{
		return Result;
	}

	Result.bResolved = true;
	Result.FinalDamage = Request.IncomingDamage;

	// 2. 遭遇战待命屏障 (Encounter Dormancy Barrier)：
	// 若受击者为尚未被激活的遭遇战待命怪，完全抑制本次伤害，不扣血也不产生受击硬直
	if (const AEnemy* HitEnemy = Cast<AEnemy>(Request.HitActor); HitEnemy && HitEnemy->IsEncounterDormant())
	{
		Result.bSuppressed = true;
		return Result;
	}

	// 3. 阵营过滤 (基于 Actor Tag 白名单机制：Player / Enemy)
	Result.bSameTeam = FCombatTeamHelper::ShareTeamTag(Request.Attacker, Request.HitActor);
	if (!Result.bSameTeam)
	{
		// 4. 【防御接口决议】：向受击者询问防御状态 (TryBlockHit)
		if (IBlockableInterface* Blockable = Cast<IBlockableInterface>(Request.HitActor))
		{
			const FBlockResult BlockResult = Blockable->TryBlockHit(Request);
			Result.bBlocked = BlockResult.bBlocked;
			Result.bParried = BlockResult.bParried;

			// 若受击者成功格挡或弹反，更新最终伤害、击退比例与受击硬直标记
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

		// 5. 【扣除生命值】：调用引擎原生 ApplyDamage（会触发受击方的 TakeDamage 逻辑）
		UGameplayStatics::ApplyDamage(Request.HitActor, Result.FinalDamage, Request.EventInstigator,
			Request.DamageCauser, UDamageType::StaticClass());

		// 6. 【扣除韧性】：若受击者为敌人且允许削韧，扣除其韧性值 (Poise)
		if (AEnemy* HitEnemy = Cast<AEnemy>(Request.HitActor); HitEnemy && Request.bApplyPoiseDamage)
		{
			HitEnemy->ApplyPoiseDamage(Request.PoiseDamage, Request.Attacker);
		}
	}

	// ==================== 弹反成功后的敌人削韧与失衡处理 ====================
	if (Result.bParried)
	{
		// 1. 弹反成功：直接将攻击方敌人的全部韧性 (CurrentPoise) 打空
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

	// 派发受击表现接口 (GetHit)
	if (Request.HitActor->Implements<UHitInterface>())
	{
		IHitInterface::Execute_GetHit(Request.HitActor, Request.HitResult.ImpactPoint, Request.Attacker);
	}

	if (Result.bParried)
	{
		// 2. 派发完 GetHit 之后，如果攻击方敌人韧性已被打空，立即触发专有的 EES_StanceBreak (架势崩溃大硬直)
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
