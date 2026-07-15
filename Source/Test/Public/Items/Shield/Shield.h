// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Items/item.h"
#include "Shield.generated.h"

class USoundBase;
class UNiagaraSystem;

UCLASS()
class TEST_API AShield : public Aitem
{
	GENERATED_BODY()

public:
	void EquipToOffhand(USceneComponent* Parent, const FName& SocketName, AActor* NewOwner);
	virtual void OnPickup_Implementation(AActor* Picker) override;
	virtual bool RequiresPersistentWorldClaim() const override { return true; }

	/* Getters */
	FORCEINLINE float GetBlockHalfAngleDegrees() const { return BlockHalfAngleDegrees; }
	FORCEINLINE float GetBlockedDamageMultiplier() const { return BlockedDamageMultiplier; }
	FORCEINLINE float GetBlockStaminaCost() const { return BlockStaminaCost; }
	FORCEINLINE float GetBlockMoveSpeedMultiplier() const { return BlockMoveSpeedMultiplier; }
	FORCEINLINE USoundBase* GetBlockSound() const { return BlockSound; }
	FORCEINLINE UNiagaraSystem* GetBlockParticle() const { return BlockParticle; }
	FORCEINLINE float GetParryStaminaCost() const { return ParryStaminaCost; }
	FORCEINLINE float GetParryCooldown() const { return ParryCooldown; }
	FORCEINLINE USoundBase* GetParrySound() const { return ParrySound; }
	FORCEINLINE UNiagaraSystem* GetParryParticle() const { return ParryParticle; }
	FORCEINLINE FName GetOffhandSocketName() const { return OffhandSocketName; }

private:

	// 格挡判定半角，总防御弧 = 此值 × 2（默认 ±60°）
	UPROPERTY(EditAnywhere, Category = "Block", meta = (AllowPrivateAccess = "true", ToolTip = "格挡判定半角，总防御弧 = 此值 × 2。"))
	float BlockHalfAngleDegrees = 45.f;

	// 格挡后伤害倍率（0.05 = 减伤 95%，0 = 完全免伤）
	UPROPERTY(EditAnywhere, Category = "Block", meta = (AllowPrivateAccess = "true", ToolTip = "格挡后伤害倍率。0.05=减伤95%，0=完全免伤。"))
	float BlockedDamageMultiplier = 0.05f;

	// 每次成功格挡的基础体力消耗（体力不足时格挡自动失败）
	UPROPERTY(EditAnywhere, Category = "Block", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ToolTip = "每次成功格挡的基础体力消耗。最终耗体 = 此值 × 攻击方格挡耗体倍率。体力不足时格挡自动失败。"))
	float BlockStaminaCost = 12.f;

	// 格挡中的移速倍率（1.0 = 不减速）
	UPROPERTY(EditAnywhere, Category = "Block", meta = (AllowPrivateAccess = "true", ToolTip = "格挡中的移速倍率。1.0=不减速。"))
	float BlockMoveSpeedMultiplier = 1.0f;

	// 副手装备插槽名
	UPROPERTY(EditAnywhere, Category = "Equip", meta = (AllowPrivateAccess = "true", ToolTip = "盾牌挂载的副手骨骼插槽名。"))
	FName OffhandSocketName = FName("LeftHandSocket");

	// 盾牌装备音效
	UPROPERTY(EditAnywhere, Category = "Equip", meta = (AllowPrivateAccess = "true", ToolTip = "盾牌装备时播放的音效。"))
	USoundBase* EquipSound;

	// 格挡成功音效
	UPROPERTY(EditAnywhere, Category = "Block", meta = (AllowPrivateAccess = "true", ToolTip = "格挡成功时播放的音效。"))
	USoundBase* BlockSound;

	// 格挡成功粒子
	UPROPERTY(EditAnywhere, Category = "Block", meta = (AllowPrivateAccess = "true", ToolTip = "格挡成功时播放的粒子特效。"))
	UNiagaraSystem* BlockParticle;

	/* 弹反 */
	// 弹反体力消耗（按下时即扣除，不论成功/失误）
	UPROPERTY(EditAnywhere, Category = "Parry", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ToolTip = "弹反的体力消耗。不论成功或失误都扣除。"))
	float ParryStaminaCost = 15.f;

	// 弹反后隐形冷却时间（秒），防止连续点按
	UPROPERTY(EditAnywhere, Category = "Parry", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ToolTip = "弹反后的隐形冷却时间（秒）。弹反动画结束到下一次可弹反的最短间隔。"))
	float ParryCooldown = 0.4f;

	// 弹反成功音效
	UPROPERTY(EditAnywhere, Category = "Parry", meta = (AllowPrivateAccess = "true", ToolTip = "弹反成功时播放的音效。"))
	USoundBase* ParrySound;

	// 弹反成功粒子特效
	UPROPERTY(EditAnywhere, Category = "Parry", meta = (AllowPrivateAccess = "true", ToolTip = "弹反成功时播放的粒子特效。"))
	UNiagaraSystem* ParryParticle;
};
