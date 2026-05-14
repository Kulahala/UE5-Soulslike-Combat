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

	// 格挡判定半角，总防御弧 = 此值 × 2（默认 ±60°）
	UPROPERTY(EditAnywhere, Category = "Block", meta = (ToolTip = "格挡判定半角，总防御弧 = 此值 × 2。"))
	float BlockHalfAngleDegrees = 60.f;

	// 格挡后伤害倍率（0.05 = 减伤 95%，0 = 完全免伤）
	UPROPERTY(EditAnywhere, Category = "Block", meta = (ToolTip = "格挡后伤害倍率。0.05=减伤95%，0=完全免伤。"))
	float BlockedDamageMultiplier = 0.05f;

	// 每点原始伤害消耗的体力（体力不足时格挡自动失败）
	UPROPERTY(EditAnywhere, Category = "Block", meta = (ToolTip = "每点原始伤害消耗的体力。体力不足时格挡自动失败。"))
	float BlockStaminaCostPerDamage = 0.7f;

	// 格挡中的移速倍率（1.0 = 不减速）
	UPROPERTY(EditAnywhere, Category = "Block", meta = (ToolTip = "格挡中的移速倍率。1.0=不减速。"))
	float BlockMoveSpeedMultiplier = 1.0f;

	// 副手装备插槽名
	UPROPERTY(EditAnywhere, Category = "Equip", meta = (ToolTip = "盾牌挂载的副手骨骼插槽名。"))
	FName OffhandSocketName = FName("LeftHandSocket");

	// 盾牌装备音效
	UPROPERTY(EditAnywhere, Category = "Equip", meta = (ToolTip = "盾牌装备时播放的音效。"))
	USoundBase* EquipSound;

	// 格挡成功音效
	UPROPERTY(EditAnywhere, Category = "Block", meta = (ToolTip = "格挡成功时播放的音效。"))
	USoundBase* BlockSound;

	// 格挡成功粒子
	UPROPERTY(EditAnywhere, Category = "Block", meta = (ToolTip = "格挡成功时播放的粒子特效。"))
	UNiagaraSystem* BlockParticle;
};
