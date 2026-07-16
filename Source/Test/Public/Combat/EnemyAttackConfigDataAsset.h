// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/CombatProjectile.h"
#include "EnemyAttackConfigDataAsset.generated.h"

class UAnimMontage;
struct FPropertyChangedEvent;

UENUM(BlueprintType)
enum class EEnemyAttackDeliveryType : uint8
{
	Melee UMETA(DisplayName = "Melee"),
	Projectile UMETA(DisplayName = "Projectile")
};

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ToolTip = "攻击命中投递方式。Projectile 仍需要 Montage，并在 Release Notify 时发射原生投射物。"))
	EEnemyAttackDeliveryType DeliveryType = EEnemyAttackDeliveryType::Melee;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0", ToolTip = "伤害倍率（相对武器基础伤害）。"))
	float DamageMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0", ToolTip = "玩家成功格挡该招式时的体力消耗倍率。最终耗体 = 盾牌基础格挡耗体 × 此倍率。"))
	float BlockStaminaDamageMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parry", meta = (ToolTip = "勾选后，该招式命中玩家弹反窗口时不会被弹反；普通格挡不受影响。"))
	bool bCannotBeParried = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cooldown", meta = (ClampMin = "0.0", ToolTip = "攻击结束或被打断后的最短冷却（秒），不包含蒙太奇播放时间。"))
	float MinCooldown = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cooldown", meta = (ClampMin = "0.0", ToolTip = "攻击结束或被打断后的最长冷却（秒），不包含蒙太奇播放时间。"))
	float MaxCooldown = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Range", meta = (ClampMin = "0.0", ToolTip = "可选择该攻击的最小目标距离。"))
	float MinDistance = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Range", meta = (ClampMin = "0.0", ToolTip = "可选择该攻击的最大目标距离。v1 应小于等于使用者的 CombatAttackMaxRadius。"))
	float MaxDistance = 170.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "0.0", ToolTip = "满足距离条件时的加权随机权重。0 表示不参与选择。"))
	float Weight = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (EditCondition = "DeliveryType == EEnemyAttackDeliveryType::Projectile", ToolTip = "Projectile 攻击实际生成的原生 ACombatProjectile 类。缺失时该条目不会进入候选池。"))
	TSubclassOf<ACombatProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (EditCondition = "DeliveryType == EEnemyAttackDeliveryType::Projectile", ToolTip = "Projectile 命中和飞行配置。攻击开始时会与本条目倍率合成为不可变快照。"))
	FProjectileDeliveryConfig ProjectileDeliveryConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (EditCondition = "DeliveryType == EEnemyAttackDeliveryType::Projectile", ToolTip = "发射 Socket。None 时使用敌人 GetActorEyesViewPoint()；正式射手应在 D-B 配置真实 Socket。"))
	FName ProjectileSpawnSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Motion Warping", meta = (ToolTip = "是否为该招式启用 Motion Warping。v1 只建议用于跳劈/跃进类 root motion 攻击。"))
	bool bUseMotionWarping = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Motion Warping", meta = (EditCondition = "bUseMotionWarping", ToolTip = "蒙太奇 Motion Warping NotifyState 使用的 Warp Target 名称。必须与蒙太奇中配置一致。"))
	FName WarpTargetName = FName("AttackTarget");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Motion Warping", meta = (EditCondition = "bUseMotionWarping", ClampMin = "0.0", ToolTip = "落点距离目标保留的前方距离（cm），避免跳到玩家身体中心。"))
	float WarpStopDistance = 80.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Motion Warping", meta = (EditCondition = "bUseMotionWarping", ClampMin = "0.0", ToolTip = "从敌人当前位置到 WarpLocation 的最大允许修正距离（cm）。这是 root motion 保险，不参与攻击选择。"))
	float MaxWarpDistance = 200.f;
};

UCLASS(BlueprintType)
class TEST_API UEnemyAttackConfigDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ToolTip = "敌人可用攻击条目。满足距离条件的条目会按 Weight 加权随机选择。"))
	TArray<FEnemyAttackEntry> Attacks;

	int32 ChooseAttackIndex(float DistanceToTarget) const;
	int32 ChooseAttackIntentIndex(int32 ExcludedAttackIndex = INDEX_NONE) const;
	bool IsEntrySelectable(const FEnemyAttackEntry& Entry) const;

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void NormalizeEntries();
	void LogConfigWarnings() const;
};
