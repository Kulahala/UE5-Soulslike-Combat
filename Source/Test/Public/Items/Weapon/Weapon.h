// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Items/item.h"
#include "Weapon.generated.h"

class UBoxComponent;

UCLASS()
class TEST_API AWeapon : public Aitem
{
	GENERATED_BODY()

public:
	AWeapon();

	void AttachMeshToSocket(USceneComponent* Parent, const FName& SocketName);
	void Equip(USceneComponent* Parent, const FName& SocketName, AActor* NewOwner, APawn* NewInstigator);

	// IPickupInterface
	virtual void OnPickup_Implementation(AActor* Picker) override;

	// 武器碰撞检测
	void StartWeaponTrace();  // 开始检测：重置旧位置
	void ExecuteWeaponTrace(); // 执行检测：每帧调用

	// 防重复受击黑名单
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TArray<AActor*> IgnoreActors;

protected:
	/* 拾取 */
	// 装备武器时播放的音效
	UPROPERTY(EditAnywhere, Category = "Weapon Properties")
	USoundBase* EquipSound;

	/* 战斗效果 */
	// 击中时的摄像机震动
	void CameraShake();
	void HitStop(AActor* HitActor);

	UFUNCTION()
	void RestoreTimeDilation(AActor* Attacker, AActor* Victim);

	// 命中时给攻击者本地玩家播放的相机震动
	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<class UCameraShakeBase> HitCameraShake;

	// 是否开启卡肉感 (Hit Stop)
	UPROPERTY(EditAnywhere, Category = "Combat")
	bool bEnableHitStop = false;

	// 卡肉持续时间（秒）
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (EditCondition = "bEnableHitStop"))
	float HitStopDuration = 0.05f;

	// 卡肉时的时间流速（越接近 0 越像静止）
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (EditCondition = "bEnableHitStop"))
	float HitStopTimeDilation = 0.05f;

private:
	/* 碰撞检测（在蓝图中调整：X轴=武器长度，Y轴=武器宽度，Z轴=武器厚度） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equip", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* BoxTrace;

	// 用于保存上一帧的位置/旋转，防止极高速穿模（扫面检测所需）
	FVector TraceCenterOld;
	FRotator TraceRotationOld;

	// 武器基础伤害，格挡时按 BlockedDamageMultiplier 缩放
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	float Damage = 10.f;

	/* 装备旋转偏移：修正不同武器模型的本地朝向差异，装备后叠加到Socket旋转上 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equip", meta = (AllowPrivateAccess = "true"))
	FRotator EquipRotationOffset = FRotator::ZeroRotator;

public:
	FORCEINLINE void SetEnableHitStop(bool bEnable) { bEnableHitStop = bEnable; }
};
