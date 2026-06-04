// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnemyAttackConfigDataAsset.generated.h"

class UAnimMontage;
struct FPropertyChangedEvent;

USTRUCT(BlueprintType)
struct FEnemyAttackEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ToolTip = "攻击条目名称，仅用于编辑器识别和调试。"))
	FName AttackName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ToolTip = "攻击动画蒙太奇。"))
	UAnimMontage* Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ToolTip = "播放起始 Section。为空时从蒙太奇默认入口播放。"))
	FName StartSection = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0", ToolTip = "伤害倍率（相对武器基础伤害）。"))
	float DamageMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0", ToolTip = "玩家成功格挡该招式时的体力消耗倍率。最终耗体 = 盾牌基础格挡耗体 × 此倍率。"))
	float BlockStaminaDamageMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cooldown", meta = (ClampMin = "0.0", ToolTip = "攻击冷却最短间隔（秒），从攻击开始计算，包含动画播放时间。"))
	float MinCooldown = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cooldown", meta = (ClampMin = "0.0", ToolTip = "攻击冷却最长间隔（秒），从攻击开始计算，包含动画播放时间。"))
	float MaxCooldown = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Range", meta = (ClampMin = "0.0", ToolTip = "可选择该攻击的最小目标距离。"))
	float MinDistance = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Range", meta = (ClampMin = "0.0", ToolTip = "可选择该攻击的最大目标距离。v1 应小于等于使用者的 CombatAttackMaxRadius。"))
	float MaxDistance = 170.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "0.0", ToolTip = "满足距离条件时的加权随机权重。0 表示不参与选择。"))
	float Weight = 1.f;
};

UCLASS(BlueprintType)
class TEST_API UEnemyAttackConfigDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ToolTip = "敌人可用攻击条目。满足距离条件的条目会按 Weight 加权随机选择。"))
	TArray<FEnemyAttackEntry> Attacks;

	int32 ChooseAttackIndex(float DistanceToTarget) const;

	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void NormalizeEntries();
	void LogConfigWarnings() const;
};
