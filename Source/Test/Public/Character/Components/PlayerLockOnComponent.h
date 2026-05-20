// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerLockOnComponent.generated.h"

class AEnemy;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TEST_API UPlayerLockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerLockOnComponent();

	AEnemy* FindBestTarget(const FVector& PlayerLoc, const FVector& CameraForward) const;
	void SetLockedTarget(AEnemy* NewTarget);
	void ClearLockedTarget();
	bool IsCurrentTargetValid(const FVector& PlayerLoc) const;

	FORCEINLINE bool IsLockingOn() const { return bIsLockingOn; }
	FORCEINLINE AEnemy* GetLockedTarget() const { return LockedTarget; }
	FORCEINLINE float GetRotationInterpSpeed() const { return LockOnRotationInterpSpeed; }
	FORCEINLINE float GetStrafeSpeedMultiplier() const { return LockOnStrafeSpeedMultiplier; }
	FORCEINLINE float GetBackSpeedMultiplier() const { return LockOnBackSpeedMultiplier; }
	FORCEINLINE const FVector& GetSocketOffset() const { return LockOnSocketOffset; }
	FORCEINLINE float GetSocketOffsetInterpSpeed() const { return LockOnSocketOffsetInterpSpeed; }
	FORCEINLINE float GetFreeRunCameraSideOffset() const { return LockOnFreeRunCameraSideOffset; }
	FORCEINLINE float GetFreeRunCameraBackHeightOffset() const { return LockOnFreeRunCameraBackHeightOffset; }
	FORCEINLINE float GetFreeRunCameraBackArmLengthBonus() const { return LockOnFreeRunCameraBackArmLengthBonus; }
	FORCEINLINE float GetFreeRunCameraInterpSpeed() const { return LockOnFreeRunCameraInterpSpeed; }

private:
	float ScoreTarget(const AEnemy* Enemy, const FVector& PlayerLoc, const FVector& CameraForward) const;

	UPROPERTY()
	AEnemy* LockedTarget = nullptr;

	bool bIsLockingOn = false;

	UPROPERTY(EditDefaultsOnly, Category = "LockOn", meta = (ToolTip = "锁定目标搜索半径（cm）。"))
	float LockOnRadius = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOn", meta = (ToolTip = "前方视角过滤半角（度）。只有在此角度内的敌人才会被选中。"))
	float LockOnViewAngleDegrees = 45.f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOn", meta = (ToolTip = "锁定朝向插值速度。值越大转向越快。"))
	float LockOnRotationInterpSpeed = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOn", meta = (ToolTip = "自动解锁距离（cm）。目标超出此距离自动解锁，带滞后防抖动。"))
	float LockOnBreakRadius = 2000.f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOn|Movement", meta = (ClampMin = "0.0", ToolTip = "锁定时侧移速度倍率。1.0 表示不低于基础移动速度。"))
	float LockOnStrafeSpeedMultiplier = 0.95f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOn|Movement", meta = (ClampMin = "0.0", ToolTip = "锁定时后撤速度倍率。1.0 表示不低于基础移动速度。"))
	float LockOnBackSpeedMultiplier = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOnCamera", meta = (ToolTip = "锁定时 SpringArm 右肩偏移（Y=右, Z=上）。"))
	FVector LockOnSocketOffset = FVector(0.f, 40.f, 60.f);

	UPROPERTY(EditDefaultsOnly, Category = "LockOnCamera", meta = (ToolTip = "SocketOffset 插值速度。"))
	float LockOnSocketOffsetInterpSpeed = 6.f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOnCamera|FreeRun", meta = (ClampMin = "0.0", ToolTip = "锁定冲刺侧移时相机横向最大偏移幅度（cm），正值按输入方向偏移。"))
	float LockOnFreeRunCameraSideOffset = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOnCamera|FreeRun", meta = (ClampMin = "0.0", ToolTip = "锁定冲刺后撤时相机抬高幅度（cm）。"))
	float LockOnFreeRunCameraBackHeightOffset = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOnCamera|FreeRun", meta = (ClampMin = "0.0", ToolTip = "锁定冲刺后撤时弹簧臂额外拉远（cm），默认 0 不改变距离手感。"))
	float LockOnFreeRunCameraBackArmLengthBonus = 80.f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOnCamera|FreeRun", meta = (ToolTip = "锁定冲刺动态偏移插值速度。"))
	float LockOnFreeRunCameraInterpSpeed = 10.f;
};
