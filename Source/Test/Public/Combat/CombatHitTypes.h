#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class AActor;
class AController;

/**
 * 一次已确认碰撞的战斗结算输入数据 (Hit Request)。
 * 数值在投递端（武器挥砍或子弹飞行）创建时快照，结算时不读取攻击者可能已经变化的瞬时招式状态。
 */
struct FCombatHitRequest
{
	/** 发起攻击的实体（如玩家 AMyCharacter 或敌人 AEnemy） */
	AActor* Attacker = nullptr;

	/** 直接造成伤害的道具/投射物实体（如武器 AWeapon、箭矢 ACombatProjectile） */
	AActor* DamageCauser = nullptr;

	/** 攻击者的控制器（用于 UE 原生伤害系统的 Instigator 归属） */
	AController* EventInstigator = nullptr;

	/** 被命中的目标 Actor */
	AActor* HitActor = nullptr;

	/** 引擎原生的射线/扫掠命中检测结果（包含命中点 ImpactPoint、表面法线 ImpactNormal 等） */
	FHitResult HitResult;

	/** 原始输入伤害值（基于武器基础伤害 * 动作倍率） */
	float IncomingDamage = 0.f;

	/** 原始输入削韧值（用于削减敌人韧性 Poise，触发失衡破防） */
	float PoiseDamage = 0.f;

	/** 本次攻击是否执行削韧计算（某些轻攻击/环境伤害可不削韧） */
	bool bApplyPoiseDamage = false;

	/** 破挡体力倍率（当受击者举盾格挡时，盾牌消耗的体力需乘以该倍率，用于重击破挡） */
	float BlockStaminaDamageMultiplier = 1.f;

	/** 该招式是否允许被弹反（普通攻击为 true，大招/投技/地面AOE 为 false） */
	bool bCanBeParried = true;
};

/**
 * 一次战斗结算的最终输出结果 (Hit Result)。
 * 全局裁判 FCombatHitResolver 结算完毕后输出给武器/投射物或外部系统，用于决定表现与生命周期。
 */
struct FCombatHitResult
{
	/** 结算是否成功完成（若 HitActor 为空则为 false） */
	bool bResolved = false;

	/** 本次命中是否被完全抑制（例如击中了处于遭遇战待命状态 Dormant 的敌人，不扣血不反馈） */
	bool bSuppressed = false;

	/** 是否为同阵营命中（同阵营友军不扣血、不削韧，但可派发受击动画） */
	bool bSameTeam = false;

	/** 受击方是否成功格挡（举盾或弹反成功时为 true） */
	bool bBlocked = false;

	/** 受击方是否成功弹反（精准弹反成功时为 true） */
	bool bParried = false;

	/** 经过格挡/减伤计算后的最终实际扣除伤害（弹反成功为 0，格挡成功为减免后伤害） */
	float FinalDamage = 0.f;

	/** 是否播放普通受击硬直动画（格挡/弹反成功通常为 false，被打破防或未格挡为 true） */
	bool bPlayNormalHitReact = true;

	/** 受击击退距离缩放倍率（格挡成功时按减伤比缩短击退距离） */
	float KnockbackScale = 1.f;

	/** 是否对受击者施加受击僵直（用于状态机切入 EAS_Stunning） */
	bool bApplyStun = true;
};
