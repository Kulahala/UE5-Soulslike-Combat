#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpringArmComponent.h"
#include "ObstructionRecoverySpringArmComponent.generated.h"

/**
 * 在 SpringArm 已完成的碰撞 Sweep 结果层平滑清障回弹。
 * 不修改 TargetArmLength，也不创建第二条相机碰撞查询。
 */
UCLASS()
class TEST_API UObstructionRecoverySpringArmComponent : public USpringArmComponent
{
	GENERATED_BODY()

public:
	void ConfigureObstructionRecovery(float InInterpSpeed, float InClearDelay);
	void ResetObstructionRecovery();

	bool IsPhysicallyObstructed() const { return bLastPhysicalCollision; }
	bool IsObstructionRecoveryActive() const { return bObstructionRecoveryActive; }
	float GetObstructionClearElapsed() const { return ObstructionClearElapsed; }
	float GetObstructionRecoveryAlpha() const { return ObstructionRecoveryAlpha; }

protected:
	virtual FVector BlendLocations(const FVector& DesiredArmLocation, const FVector& TraceHitLocation,
		bool bHitSomething, float DeltaTime) override;

private:
	FVector GetRecoveryLocation(const FVector& DesiredArmLocation) const;

	float ObstructionRecoveryInterpSpeed = 16.f;
	float ObstructionClearDelay = 0.08f;
	float ObstructionClearElapsed = 0.f;
	float ObstructionRecoveryAlpha = 1.f;
	bool bObstructionRecoveryActive = false;
	bool bLastPhysicalCollision = false;
};
