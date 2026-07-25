#include "Character/Components/ObstructionRecoverySpringArmComponent.h"

void UObstructionRecoverySpringArmComponent::ConfigureObstructionRecovery(float InInterpSpeed, float InClearDelay)
{
	ObstructionRecoveryInterpSpeed = FMath::Max(0.f, InInterpSpeed);
	ObstructionClearDelay = FMath::Max(0.f, InClearDelay);
}

void UObstructionRecoverySpringArmComponent::ResetObstructionRecovery()
{
	bLastPhysicalCollision = false;
	bObstructionRecoveryActive = false;
	ObstructionClearElapsed = 0.f;
	ObstructionRecoveryAlpha = 1.f;
}

FVector UObstructionRecoverySpringArmComponent::BlendLocations(
	const FVector& DesiredArmLocation,
	const FVector& TraceHitLocation,
	bool bHitSomething,
	float DeltaTime)
{
	bLastPhysicalCollision = bHitSomething;
	if (bHitSomething)
	{
		const FVector ArmOrigin = GetComponentLocation() + TargetOffset;
		const float DesiredDistance = FVector::Dist(ArmOrigin, DesiredArmLocation);
		ObstructionRecoveryAlpha = DesiredDistance > KINDA_SMALL_NUMBER
			? FMath::Clamp(FVector::Dist(ArmOrigin, TraceHitLocation) / DesiredDistance, 0.f, 1.f)
			: 1.f;
		ObstructionClearElapsed = 0.f;
		bObstructionRecoveryActive = true;
		return TraceHitLocation;
	}

	if (!bObstructionRecoveryActive)
	{
		return DesiredArmLocation;
	}

	ObstructionClearElapsed += FMath::Max(0.f, DeltaTime);
	if (ObstructionClearElapsed < ObstructionClearDelay)
	{
		return GetRecoveryLocation(DesiredArmLocation);
	}

	ObstructionRecoveryAlpha = FMath::FInterpTo(
		ObstructionRecoveryAlpha, 1.f, DeltaTime, ObstructionRecoveryInterpSpeed);
	if (FMath::IsNearlyEqual(ObstructionRecoveryAlpha, 1.f, 0.001f))
	{
		bObstructionRecoveryActive = false;
		ObstructionClearElapsed = 0.f;
		ObstructionRecoveryAlpha = 1.f;
		return DesiredArmLocation;
	}

	return GetRecoveryLocation(DesiredArmLocation);
}

FVector UObstructionRecoverySpringArmComponent::GetRecoveryLocation(const FVector& DesiredArmLocation) const
{
	const FVector ArmOrigin = GetComponentLocation() + TargetOffset;
	return FMath::Lerp(ArmOrigin, DesiredArmLocation, ObstructionRecoveryAlpha);
}
