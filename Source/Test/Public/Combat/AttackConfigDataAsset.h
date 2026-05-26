// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AttackConfigDataAsset.generated.h"

class UComboDataAsset;

/* 特殊攻击类型枚举（编译期类型安全） */
UENUM(BlueprintType)
enum class ESpecialAttackType : uint8
{
	SprintAttack   UMETA(DisplayName = "Sprint Attack"),
	JumpAttack     UMETA(DisplayName = "Jump Attack"),
	ChargedAttack  UMETA(DisplayName = "Charged Attack")
};

/* 特殊攻击配置 */
USTRUCT(BlueprintType)
struct FSpecialAttackConfig
{
	GENERATED_BODY()

	// 攻击类型
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ToolTip = "攻击类型"))
	ESpecialAttackType Type = ESpecialAttackType::SprintAttack;

	// 攻击蒙太奇
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ToolTip = "攻击动画蒙太奇"))
	TObjectPtr<UAnimMontage> Montage;

	// 伤害倍率
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.1", ClampMax = "10.0", ToolTip = "伤害倍率（相对武器基础伤害）"))
	float DamageMultiplier = 1.0f;

	// 韧性伤害倍率
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Poise", meta = (ClampMin = "0.1", ClampMax = "10.0", ToolTip = "韧性伤害倍率（相对武器基础韧性伤害）"))
	float PoiseDamageMultiplier = 1.0f;

	// 体力消耗
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "1.0", ToolTip = "体力消耗"))
	float StaminaCost = 15.f;
};

/**
 * 攻击配置数据资产
 * 统一管理所有攻击类型的配置（连招、冲刺攻击、跳跃攻击等）
 */
UCLASS(BlueprintType)
class TEST_API UAttackConfigDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/* 轻攻击连招链 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo", meta = (ToolTip = "轻攻击连招链配置"))
	TObjectPtr<UComboDataAsset> LightAttackCombo;

	/* 特殊攻击配置（使用 TArray 提升蓝图编辑体验） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Special Attacks", meta = (ToolTip = "特殊攻击配置（冲刺攻击、跳跃攻击等）"))
	TArray<FSpecialAttackConfig> SpecialAttacks;

	/* Helper 方法：查找特殊攻击配置（线性遍历，3-5 个条目性能无影响） */
	const FSpecialAttackConfig* FindSpecialAttack(ESpecialAttackType AttackType) const;

	/* 验证配置完整性 */
	virtual void PostInitProperties() override;
};
