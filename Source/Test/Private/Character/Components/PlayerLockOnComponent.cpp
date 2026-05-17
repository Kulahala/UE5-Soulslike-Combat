// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Components/PlayerLockOnComponent.h"

#include "AttributeComponent/AttributeComponent.h"
#include "Enemy/Enemy.h"
#include "Kismet/GameplayStatics.h"

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
