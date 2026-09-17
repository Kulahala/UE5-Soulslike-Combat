// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Components/PlayerLockOnComponent.h"

#include "AttributeComponent/AttributeComponent.h"
#include "Enemy/Enemy.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr float LockTargetSwitchSideEpsilonPixels = 1.f;

	bool IsPointInsideViewport(const FVector2D& ScreenPosition, int32 ViewportWidth, int32 ViewportHeight)
	{
		return ScreenPosition.X >= 0.f
			&& ScreenPosition.Y >= 0.f
			&& ScreenPosition.X <= static_cast<float>(ViewportWidth)
			&& ScreenPosition.Y <= static_cast<float>(ViewportHeight);
	}
}

UPlayerLockOnComponent::UPlayerLockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

AEnemy* UPlayerLockOnComponent::FindBestTarget(const FVector& PlayerLoc, const FVector& CameraForward) const
{
	if (!GetWorld())
	{
		return nullptr;
	}

	TArray<AActor*> AllEnemies;
	UGameplayStatics::GetAllActorsOfClass(GetOwner(), AEnemy::StaticClass(), AllEnemies);

	AEnemy* BestTarget = nullptr;
	float BestScore = MAX_FLT;
	const FVector CameraForward2D = CameraForward.GetSafeNormal2D();

	for (AActor* Actor : AllEnemies)
	{
		AEnemy* Enemy = Cast<AEnemy>(Actor);
		const float Score = ScoreTarget(Enemy, PlayerLoc, CameraForward2D);
		if (Score < BestScore)
		{
			BestScore = Score;
			BestTarget = Enemy;
		}
	}

	return BestTarget;
}

AEnemy* UPlayerLockOnComponent::FindScreenSideTarget(APlayerController* PlayerController, bool bSwitchToRight) const
{
	AActor* Owner = GetOwner();
	if (!GetWorld() || !Owner || !PlayerController || !IsValid(LockedTarget))
	{
		return nullptr;
	}

	FVector2D CurrentTargetScreenPosition;
	if (!PlayerController->ProjectWorldLocationToScreen(LockedTarget->GetActorLocation(), CurrentTargetScreenPosition, true))
	{
		return nullptr;
	}

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth <= 0 || ViewportHeight <= 0
		|| !IsPointInsideViewport(CurrentTargetScreenPosition, ViewportWidth, ViewportHeight))
	{
		return nullptr;
	}

	const FVector PlayerLocation = Owner->GetActorLocation();
	TArray<AActor*> AllEnemies;
	UGameplayStatics::GetAllActorsOfClass(Owner, AEnemy::StaticClass(), AllEnemies);

	AEnemy* BestTarget = nullptr;
	float BestScreenDistanceSquared = MAX_FLT;
	float BestWorldDistanceSquared = MAX_FLT;

	for (AActor* Actor : AllEnemies)
	{
		AEnemy* Enemy = Cast<AEnemy>(Actor);
		if (!IsValidScreenSwitchCandidate(Enemy, PlayerLocation))
		{
			continue;
		}

		FVector2D CandidateScreenPosition;
		if (!PlayerController->ProjectWorldLocationToScreen(Enemy->GetActorLocation(), CandidateScreenPosition, true)
			|| !IsPointInsideViewport(CandidateScreenPosition, ViewportWidth, ViewportHeight))
		{
			continue;
		}

		const float HorizontalDelta = CandidateScreenPosition.X - CurrentTargetScreenPosition.X;
		const bool bIsOnRequestedSide = bSwitchToRight
			? HorizontalDelta > LockTargetSwitchSideEpsilonPixels
			: HorizontalDelta < -LockTargetSwitchSideEpsilonPixels;
		if (!bIsOnRequestedSide)
		{
			continue;
		}

		const float ScreenDistanceSquared = FVector2D::DistSquared(CandidateScreenPosition, CurrentTargetScreenPosition);
		const float WorldDistanceSquared = FVector::DistSquared2D(Enemy->GetActorLocation(), PlayerLocation);
		if (ScreenDistanceSquared < BestScreenDistanceSquared
			|| (FMath::IsNearlyEqual(ScreenDistanceSquared, BestScreenDistanceSquared, 0.01f)
				&& WorldDistanceSquared < BestWorldDistanceSquared))
		{
			BestTarget = Enemy;
			BestScreenDistanceSquared = ScreenDistanceSquared;
			BestWorldDistanceSquared = WorldDistanceSquared;
		}
	}

	return BestTarget;
}

void UPlayerLockOnComponent::SetLockedTarget(AEnemy* NewTarget)
{
	if (LockedTarget == NewTarget && IsValid(LockedTarget))
	{
		bIsLockingOn = true;
		LockedTarget->SetTargetedByPlayer(true);
		return;
	}

	ClearLockedTarget();
	if (!NewTarget)
	{
		return;
	}

	LockedTarget = NewTarget;
	bIsLockingOn = true;
	LockedTarget->SetTargetedByPlayer(true);
}

void UPlayerLockOnComponent::ClearLockedTarget()
{
	if (IsValid(LockedTarget))
	{
		LockedTarget->SetTargetedByPlayer(false);
	}

	LockedTarget = nullptr;
	bIsLockingOn = false;
}

bool UPlayerLockOnComponent::IsCurrentTargetValid(const FVector& PlayerLoc) const
{
	return IsValid(LockedTarget)
		&& LockedTarget->GetAttributes()
		&& LockedTarget->GetAttributes()->IsAlive()
		&& FVector::Dist2D(PlayerLoc, LockedTarget->GetActorLocation()) <= LockOnBreakRadius;
}

float UPlayerLockOnComponent::ScoreTarget(const AEnemy* Enemy, const FVector& PlayerLoc, const FVector& CameraForward) const
{
	if (!Enemy || !Enemy->GetAttributes() || !Enemy->GetAttributes()->IsAlive())
	{
		return MAX_FLT;
	}

	const FVector ToEnemy = Enemy->GetActorLocation() - PlayerLoc;
	const float Distance = ToEnemy.Size2D();
	if (Distance > LockOnRadius)
	{
		return MAX_FLT;
	}

	const FVector ToEnemyDir = ToEnemy.GetSafeNormal2D();
	const float DotAngle = FVector::DotProduct(CameraForward, ToEnemyDir);
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(LockOnViewAngleDegrees));
	if (DotAngle < CosHalfAngle)
	{
		return MAX_FLT;
	}

	return (1.f - DotAngle) * 1000.f + Distance;
}

bool UPlayerLockOnComponent::IsValidScreenSwitchCandidate(const AEnemy* Enemy, const FVector& PlayerLoc) const
{
	return Enemy
		&& Enemy != LockedTarget
		&& Enemy->GetAttributes()
		&& Enemy->GetAttributes()->IsAlive()
		&& FVector::Dist2D(PlayerLoc, Enemy->GetActorLocation()) <= LockOnRadius;
}
