// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/Enemy.h"
#include "RangedEnemy.generated.h"

/**
 * 纯远程敌人的战术基类。
 * AEnemy 保留共同 Combat/Projectile 核心；本类只拥有距离环、Escape 与未 Release 的 LOS 取消策略。
 */
UCLASS(Abstract)
class TEST_API ARangedEnemy : public AEnemy
{
	GENERATED_BODY()

public:
	ARangedEnemy();

protected:
	virtual bool HandleArchetypeCombatPriority(float DeltaTime, float DistanceToTarget, const FVector& ToTarget) override;
	virtual void TickArchetypeAttack(float DeltaTime) override;
	virtual bool HandleArchetypeAttackCooldownEnded() override;
	virtual bool HandleArchetypeMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result) override;
	virtual void ClearArchetypeCombatState() override;
	virtual void ValidateArchetypeCombatConfig() const override;
	virtual FString GetArchetypeCombatDebugText() const override;
	virtual void DrawArchetypeCombatDebug() const override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Spacing|Ranged Escape", meta = (ClampMin = "0.0", ToolTip = "纯远程攻击配置进入高速逃离的距离阈值（cm）。距离必须严格小于该值，且不应大于 CombatTooCloseRadius。"))
	float RangedEscapeEnterRadius = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Spacing|Ranged Escape", meta = (ClampMin = "0.0", ToolTip = "纯远程攻击配置退出高速逃离的距离阈值（cm）。距离达到或超过该值才退出；应落在安全距离环内。"))
	float RangedEscapeExitRadius = 1000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Ranged Cancellation", meta = (ClampMin = "0.0", ToolTip = "Draw/AimHold 期间连续失去 LOS 多久后取消未 Release 的远程攻击（秒）。"))
	float ProjectileLostLineOfSightCancelDelay = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Ranged Cancellation", meta = (ClampMin = "0.0", ToolTip = "因 LOS 丢失取消攻击时，当前 Montage 的 Blend Out 时长（秒）。"))
	float ProjectileLostLineOfSightCancelBlendOut = 0.12f;

	bool HasValidRangedEscapeConfig() const;
	bool HandleRangedEscape(float DeltaTime, float DistanceToTarget, const FVector& ToTarget);
	FVector BuildRangedEscapeGoal(const FVector& ToTarget) const;
	void TickRangedEscapeFacing(float DeltaTime);
	bool TryStartRangedEscapeNavigation(const FVector& EscapeGoal);
	bool TryStartRangedEscapeFallback(float CurrentTime, float DistanceToTarget, const FVector& ToTarget);
	void ClearRangedEscape();
	void TickProjectileLostLineOfSightCancellation();

	FAIRequestID RangedEscapeMoveRequestId;
	FAIRequestID RangedEscapeFallbackMoveRequestId;
	FVector RangedEscapeGoalLocation = FVector::ZeroVector;
	bool bRangedEscapeActive = false;
	bool bHasRangedEscapeMoveRequest = false;
	bool bHasRangedEscapeFallbackMoveRequest = false;
	bool bRangedEscapeNavigationFailed = false;
	float LostProjectileLineOfSightStartTime = -1.f;
	FString RangedCombatMoveDetailDebug;
};
