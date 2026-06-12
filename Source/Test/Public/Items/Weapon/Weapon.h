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
	void ClearIgnoreActors() { IgnoreActors.Empty(); }  // 清空黑名单（受控接口）

	// 命中解析结果（内部用）
	struct FWeaponHitResult
	{
		float FinalDamage = 0.f;
		bool bPlayNormalHitReact = true;
		float KnockbackScale = 1.f;
		bool bApplyStun = true;
		bool bSameTeam = false;
		bool bParried = false;
	};

protected:
	/* 拾取 */
	// 装备武器时播放的音效
	UPROPERTY(EditAnywhere, Category = "Weapon Properties", meta = (ToolTip = "装备武器时播放的音效。"))
	USoundBase* EquipSound;

	/* 战斗效果 */
	// 击中时的摄像机震动
	void CameraShake();
	void HitStop(AActor* HitActor);

	UFUNCTION()
	void RestoreTimeDilation(AActor* Attacker, AActor* Victim);

	// 命中时给攻击者本地玩家播放的相机震动
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ToolTip = "命中时给攻击者本地玩家播放的相机震动类。"))
	TSubclassOf<class UCameraShakeBase> HitCameraShake;

	// 是否开启卡肉感 (Hit Stop)
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ToolTip = "是否开启命中卡肉感（Hit Stop）。"))
	bool bEnableHitStop = false;

	// 卡肉持续时间（秒）
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (EditCondition = "bEnableHitStop", ToolTip = "卡肉持续时间（秒）。"))
	float HitStopDuration = 0.05f;

	// 卡肉时的时间流速（越接近 0 越像静止）
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (EditCondition = "bEnableHitStop", ToolTip = "卡肉时的时间流速。0=完全静止，1=无效果。"))
	float HitStopTimeDilation = 0.05f;

	// 武器基础韧性伤害
	UPROPERTY(EditAnywhere, Category = "Combat|Poise", meta = (ToolTip = "武器基础韧性伤害。最终韧性伤害 = 此值 × 连招倍率。"))
	float BasePoiseDamage = 1.f;

private:
	/* 碰撞检测（在蓝图中调整：X轴=武器长度，Y轴=武器宽度，Z轴=武器厚度） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equip", meta = (AllowPrivateAccess = "true", ToolTip = "武器碰撞检测盒体，X=长度 Y=宽度 Z=厚度。"))
	UBoxComponent* BoxTrace;

	// 防重复受击黑名单
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true", ToolTip = "本次攻击已命中的 Actor 黑名单，防止重复受击。"))
	TArray<AActor*> IgnoreActors;

	// 用于保存上一帧的位置/旋转，防止极高速穿模（扫面检测所需）
	FVector TraceCenterOld;
	FRotator TraceRotationOld;

	void BuildIgnoreList(TArray<AActor*>& OutActors);
	/**
	 * 解析一次武器命中。
	 * HitActor 是本次扫掠命中的 Actor，HitPoint 保留 ImpactPoint/法线等信息。
	 * 返回值只描述伤害与反馈决策，不直接播放反馈或修改受击者状态。
	 */
	FWeaponHitResult ResolveHit(AActor* HitActor, const FHitResult& HitPoint);
	/**
	 * 派发一次命中反馈。
	 * Result 必须来自 ResolveHit()；该函数负责写入 PendingHitContext、触发 GetHit、处理破防、卡肉和黑名单。
	 */
	void DispatchHitFeedback(AActor* HitActor, const FHitResult& HitPoint, const FWeaponHitResult& Result);

	// 武器基础伤害，格挡时按 BlockedDamageMultiplier 缩放
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (AllowPrivateAccess = "true", ToolTip = "武器基础伤害，格挡时按盾牌 BlockedDamageMultiplier 缩放。"))
	float Damage = 10.f;

	/* 装备旋转偏移：修正不同武器模型的本地朝向差异，装备后叠加到Socket旋转上 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equip", meta = (AllowPrivateAccess = "true", ToolTip = "装备旋转偏移，修正不同武器模型的本地朝向差异。"))
	FRotator EquipRotationOffset = FRotator::ZeroRotator;

public:
	FORCEINLINE void SetEnableHitStop(bool bEnable) { bEnableHitStop = bEnable; }
	FORCEINLINE float GetBasePoiseDamage() const { return BasePoiseDamage; }
};
