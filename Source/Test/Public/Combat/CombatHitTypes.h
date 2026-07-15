#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class AActor;
class AController;

/**
 * 一次已确认碰撞的战斗结算输入。数值在投递端创建时快照，结算时不读取攻击者可能已经变化的瞬时招式状态。
 */
struct FCombatHitRequest
{
	AActor* Attacker = nullptr;
	AActor* DamageCauser = nullptr;
	AController* EventInstigator = nullptr;
	AActor* HitActor = nullptr;
	FHitResult HitResult;
	float IncomingDamage = 0.f;
	float PoiseDamage = 0.f;
	bool bApplyPoiseDamage = false;
	float BlockStaminaDamageMultiplier = 1.f;
	bool bCanBeParried = true;
};

/**
 * 一次战斗结算的结果。投递端可据此决定自身的表现和生命周期，但不得重复应用伤害或命中反馈。
 */
struct FCombatHitResult
{
	bool bResolved = false;
	bool bSuppressed = false;
	bool bSameTeam = false;
	bool bBlocked = false;
	bool bParried = false;
	float FinalDamage = 0.f;
	bool bPlayNormalHitReact = true;
	float KnockbackScale = 1.f;
	bool bApplyStun = true;
};
