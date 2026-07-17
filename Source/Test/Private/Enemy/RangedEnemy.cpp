// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/RangedEnemy.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Utils/DebugDrawHelper.h"

ARangedEnemy::ARangedEnemy()
{
	SetArchetypeMovementDefaults(220.f, 300.f, 330.f);
	SetArchetypeCombatSpacingDefaults(900.f, 1100.f, 1000.f, 1100.f, 50.f, 1.f);
}

bool ARangedEnemy::HandleArchetypeCombatPriority(float DeltaTime, float DistanceToTarget, const FVector& ToTarget)
{
	return HandleRangedEscape(DeltaTime, DistanceToTarget, ToTarget);
}

void ARangedEnemy::TickArchetypeAttack(float)
{
	TickProjectileLostLineOfSightCancellation();
}

bool ARangedEnemy::HandleArchetypeAttackCooldownEnded()
{
	return bRangedEscapeActive;
}

bool ARangedEnemy::HandleArchetypeMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
	if (bHasRangedEscapeMoveRequest && RequestID.IsEquivalent(RangedEscapeMoveRequestId))
	{
		bHasRangedEscapeMoveRequest = false;
		FinishCombatReposition();
		const float GoalTolerance = FMath::Max(GetCombatRepositionAcceptanceRadius() + 10.f, 25.f);
		const bool bReachedRequestedGoal = FVector::Dist2D(GetActorLocation(), RangedEscapeGoalLocation) <= GoalTolerance;
		if (Result != EPathFollowingResult::Success || !bReachedRequestedGoal)
		{
			bRangedEscapeNavigationFailed = true;
			RangedCombatMoveDetailDebug = Result == EPathFollowingResult::Success
				? TEXT("EscapeNavPartial")
				: TEXT("EscapeNavFailed");
		}
		else
		{
			RangedCombatMoveDetailDebug = TEXT("EscapeNavReached");
		}
		SetCombatRepositionDelay(0.15f);
		return true;
	}

	if (bHasRangedEscapeFallbackMoveRequest && RequestID.IsEquivalent(RangedEscapeFallbackMoveRequestId))
	{
		bHasRangedEscapeFallbackMoveRequest = false;
		FinishCombatReposition();
		if (Result == EPathFollowingResult::Success)
		{
			// 后撤成功后按正常重定位节奏再试下一段固定 Escape，避免每帧重发导航。
			bRangedEscapeNavigationFailed = false;
			RangedCombatMoveDetailDebug = TEXT("EscapeFallbackReached");
			SetCombatRepositionDelay(FMath::Max(0.15f, GetCombatRepositionIntervalMin()));
		}
		else
		{
			RangedCombatMoveDetailDebug = TEXT("EscapeFallbackFailed");
			SetCombatRepositionDelay(0.15f);
		}
		return true;
	}

	return bRangedEscapeActive;
}

void ARangedEnemy::ClearArchetypeCombatState()
{
	ClearRangedEscape();
	LostProjectileLineOfSightStartTime = -1.f;
	RangedCombatMoveDetailDebug.Empty();
}

void ARangedEnemy::ValidateArchetypeCombatConfig() const
{
	if (!IsPureProjectileAttackProfile())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: ARangedEnemy requires at least one selectable Projectile entry and no selectable Melee entry; ranged Escape and LOS cancellation are disabled."),
			*GetName());
		return;
	}

	if (!HasValidRangedEscapeConfig())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: ARangedEnemy requires RangedEscapeEnterRadius > 0, RangedEscapeExitRadius > EnterRadius, and non-negative LOS cancellation timings."),
			*GetName());
		return;
	}

	ValidatePureProjectileTacticalConfig(RangedEscapeEnterRadius, RangedEscapeExitRadius);
}

FString ARangedEnemy::GetArchetypeCombatDebugText() const
{
	if (bRangedEscapeActive)
	{
		return RangedCombatMoveDetailDebug.IsEmpty()
			? TEXT("RangedEscape")
			: FString::Printf(TEXT("RangedEscape [%s]"), *RangedCombatMoveDetailDebug);
	}

	if (RangedCombatMoveDetailDebug == TEXT("LOSConfirm")
		|| RangedCombatMoveDetailDebug == TEXT("LOSCancel"))
	{
		return FString::Printf(TEXT("Ranged [%s]"), *RangedCombatMoveDetailDebug);
	}

	return FString();
}

void ARangedEnemy::DrawArchetypeCombatDebug() const
{
	if (!bRangedEscapeActive || !FDebugDrawHelper::IsRangesEnabled())
	{
		return;
	}

	DrawDebugLine(GetWorld(), GetActorLocation(), RangedEscapeGoalLocation, FColor::Magenta, false, -1.f, 0, 2.f);
	DrawDebugSphere(GetWorld(), RangedEscapeGoalLocation, 18.f, 12, FColor::Magenta, false, -1.f, 0, 1.5f);
}

bool ARangedEnemy::HasValidRangedEscapeConfig() const
{
	return RangedEscapeEnterRadius > 0.f
		&& RangedEscapeExitRadius > RangedEscapeEnterRadius
		&& ProjectileLostLineOfSightCancelDelay >= 0.f
		&& ProjectileLostLineOfSightCancelBlendOut >= 0.f;
}

bool ARangedEnemy::HandleRangedEscape(float DeltaTime, float DistanceToTarget, const FVector& ToTarget)
{
	const bool bWasEscaping = bRangedEscapeActive;
	if (!IsPureProjectileAttackProfile() || !HasValidRangedEscapeConfig())
	{
		if (bWasEscaping)
		{
			ClearRangedEscape();
			StopEnemyMovementIfPossible();
		}
		return bWasEscaping;
	}

	if (!bRangedEscapeActive)
	{
		if (DistanceToTarget >= RangedEscapeEnterRadius)
		{
			return false;
		}

		RangedEscapeGoalLocation = BuildRangedEscapeGoal(ToTarget);
		StopEnemyMovementIfPossible();
		ResetCombatReposition();
		bRangedEscapeActive = true;
		RangedCombatMoveDetailDebug = TEXT("EscapeStart");
	}

	if (DistanceToTarget >= RangedEscapeExitRadius)
	{
		ClearRangedEscape();
		StopEnemyMovementIfPossible();
		GetCharacterMovement()->MaxWalkSpeed = GetCombatManeuverSpeed();
		RangedCombatMoveDetailDebug = TEXT("EscapeExit");
		return true;
	}

	ClearPendingAttack();
	if (bHasRangedEscapeFallbackMoveRequest)
	{
		UpdateCombatRetreatSpeedEase();
	}
	else
	{
		ClearCombatRetreatSpeedEase();
	}
	TickRangedEscapeFacing(DeltaTime);

	const UWorld* World = GetWorld();
	const float CurrentTime = World ? World->GetTimeSeconds() : 0.f;
	if (!IsCombatRepositionReady(CurrentTime) || bHasRangedEscapeMoveRequest || bHasRangedEscapeFallbackMoveRequest)
	{
		return true;
	}

	if (bRangedEscapeNavigationFailed)
	{
		TryStartRangedEscapeFallback(CurrentTime, DistanceToTarget, ToTarget);
		return true;
	}

	// 一段 Escape MoveTo 结束后才用目标的最新位置计算下一段，避免追赶时不断取消导航造成抽搐。
	RangedEscapeGoalLocation = BuildRangedEscapeGoal(ToTarget);
	if (!TryStartRangedEscapeNavigation(RangedEscapeGoalLocation))
	{
		bRangedEscapeNavigationFailed = true;
	}

	return true;
}

FVector ARangedEnemy::BuildRangedEscapeGoal(const FVector& ToTarget) const
{
	AActor* CombatTarget = GetCombatTarget();
	if (!CombatTarget)
	{
		return GetActorLocation();
	}

	FVector EscapeDirection = ToTarget;
	if (EscapeDirection.IsNearlyZero())
	{
		EscapeDirection = GetActorForwardVector().GetSafeNormal2D();
	}

	return CombatTarget->GetActorLocation() - EscapeDirection * RangedEscapeExitRadius;
}

void ARangedEnemy::TickRangedEscapeFacing(float DeltaTime)
{
	FVector FacingDirection = GetVelocity().GetSafeNormal2D();
	if (FacingDirection.IsNearlyZero())
	{
		FacingDirection = (RangedEscapeGoalLocation - GetActorLocation()).GetSafeNormal2D();
	}

	if (!FacingDirection.IsNearlyZero())
	{
		TickCombatFacing(DeltaTime, FacingDirection);
	}
}

bool ARangedEnemy::TryStartRangedEscapeNavigation(const FVector& EscapeGoal)
{
	RangedEscapeGoalLocation = EscapeGoal;
	ClearCombatRetreatSpeedEase();
	GetCharacterMovement()->MaxWalkSpeed = GetChaseSpeed();
	RangedCombatMoveDetailDebug = TEXT("EscapeNav");
	if (MoveToCombatLocation(EscapeGoal, &RangedEscapeMoveRequestId))
	{
		bHasRangedEscapeMoveRequest = true;
		bRangedEscapeNavigationFailed = false;
		SetCombatRepositionDelay(0.15f);
		return true;
	}

	RangedCombatMoveDetailDebug = TEXT("EscapeMoveFail");
	SetCombatRepositionDelay(0.15f);
	return false;
}

bool ARangedEnemy::TryStartRangedEscapeFallback(float CurrentTime, float DistanceToTarget, const FVector& ToTarget)
{
	const FEnemyCombatMovePlan FallbackPlan = BuildCombatMovePlanForRange(DistanceToTarget, ToTarget,
		0.f, GetCombatPreferredMinRadius(), GetCombatPreferredMaxRadius(), false);
	if (!FallbackPlan.IsValid())
	{
		RangedCombatMoveDetailDebug = TEXT("EscapeFallbackInvalid");
		SetCombatRepositionDelay(0.15f);
		return false;
	}

	if (ExecuteCombatMovePlan(FallbackPlan, CurrentTime, &RangedEscapeFallbackMoveRequestId))
	{
		bHasRangedEscapeFallbackMoveRequest = true;
		RangedCombatMoveDetailDebug = FString::Printf(TEXT("EscapeFallback%s"), GetCombatMoveDebugName(FallbackPlan.MoveType));
		return true;
	}

	RangedCombatMoveDetailDebug = TEXT("EscapeFallbackFail");
	return false;
}

void ARangedEnemy::ClearRangedEscape()
{
	const bool bWasEscaping = bRangedEscapeActive || bHasRangedEscapeMoveRequest || bHasRangedEscapeFallbackMoveRequest;
	bRangedEscapeActive = false;
	bHasRangedEscapeMoveRequest = false;
	bHasRangedEscapeFallbackMoveRequest = false;
	bRangedEscapeNavigationFailed = false;
	RangedEscapeMoveRequestId = FAIRequestID();
	RangedEscapeFallbackMoveRequestId = FAIRequestID();
	RangedEscapeGoalLocation = FVector::ZeroVector;

	if (bWasEscaping)
	{
		FinishCombatReposition();
		SetCombatRepositionDelay(0.f);
	}
}

void ARangedEnemy::TickProjectileLostLineOfSightCancellation()
{
	if (!HasUnreleasedActiveProjectileAttack() || !IsValidCombatTarget(GetCombatTarget()))
	{
		LostProjectileLineOfSightStartTime = -1.f;
		if (RangedCombatMoveDetailDebug == TEXT("LOSConfirm"))
		{
			RangedCombatMoveDetailDebug.Empty();
		}
		return;
	}

	if (HasClearActiveProjectileLineOfSight())
	{
		LostProjectileLineOfSightStartTime = -1.f;
		if (RangedCombatMoveDetailDebug == TEXT("LOSConfirm"))
		{
			RangedCombatMoveDetailDebug.Empty();
		}
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (LostProjectileLineOfSightStartTime < 0.f)
	{
		LostProjectileLineOfSightStartTime = CurrentTime;
		RangedCombatMoveDetailDebug = TEXT("LOSConfirm");
		if (ProjectileLostLineOfSightCancelDelay > 0.f)
		{
			return;
		}
	}

	if (CurrentTime - LostProjectileLineOfSightStartTime < ProjectileLostLineOfSightCancelDelay)
	{
		return;
	}

	if (CancelUnreleasedActiveProjectileAttack(ProjectileLostLineOfSightCancelBlendOut, TEXT("lost LOS before Release")))
	{
		RangedCombatMoveDetailDebug = TEXT("LOSCancel");
	}
	LostProjectileLineOfSightStartTime = -1.f;
}
