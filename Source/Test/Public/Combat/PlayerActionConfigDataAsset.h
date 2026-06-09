#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Character/CharacterTypes.h"
#include "PlayerActionConfigDataAsset.generated.h"

class UAnimMontage;
struct FPropertyChangedEvent;

USTRUCT(BlueprintType)
struct FPlayerActionPriorityConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Priority", meta = (ClampMin = "0", UIMin = "0", ToolTip = "普通攻击优先级。数值越大优先级越高。"))
	int32 Attack = 50;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Priority", meta = (ClampMin = "0", UIMin = "0", ToolTip = "翻滚优先级。数值越大优先级越高。"))
	int32 Dodge = 80;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Priority", meta = (ClampMin = "0", UIMin = "0", ToolTip = "防御优先级。数值越大优先级越高。"))
	int32 Block = 70;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Priority", meta = (ClampMin = "0", UIMin = "0", ToolTip = "弹反优先级。数值越大优先级越高。"))
	int32 Parry = 90;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Priority", meta = (ClampMin = "0", UIMin = "0", ToolTip = "喝药优先级。数值越大优先级越高。"))
	int32 Potion = 60;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Priority", meta = (ClampMin = "0", UIMin = "0", ToolTip = "受击优先级。数值越大优先级越高。"))
	int32 HitReact = 99;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Priority", meta = (ClampMin = "0", UIMin = "0", ToolTip = "死亡优先级。数值越大优先级越高。"))
	int32 Death = 100;
};

USTRUCT(BlueprintType)
struct FPlayerSharedActionPriorityConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Priority", meta = (ClampMin = "0", UIMin = "0", ToolTip = "普通攻击优先级。Attack 配置本体仍归 UAttackConfigDataAsset；这里只保存取消判断需要的共享优先级。数值越大优先级越高。"))
	int32 Attack = 50;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Priority", meta = (ClampMin = "0", UIMin = "0", ToolTip = "受击优先级。HitReact 不是玩家主动动作；这里只保存共享优先级。数值越大优先级越高。"))
	int32 HitReact = 99;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Priority", meta = (ClampMin = "0", UIMin = "0", ToolTip = "死亡优先级。Death 不是玩家主动动作；这里只保存共享优先级。数值越大优先级越高。"))
	int32 Death = 100;
};

USTRUCT(BlueprintType)
struct FPlayerDodgeActionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge", meta = (ToolTip = "玩家翻滚蒙太奇，需包含方向 Section。"))
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "0", UIMin = "0", ToolTip = "翻滚优先级。数值越大优先级越高。"))
	int32 Priority = 80;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "翻滚体力消耗。"))
	float StaminaCost = 15.f;
};

USTRUCT(BlueprintType)
struct FPlayerBlockActionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Block", meta = (ToolTip = "玩家防御蒙太奇，需包含 BlockRaise Section。"))
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0", UIMin = "0", ToolTip = "防御优先级。数值越大优先级越高。"))
	int32 Priority = 70;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Block", meta = (ToolTip = "举盾起手 Section 名称。"))
	FName BlockRaiseSection = FName("BlockRaise");
};

USTRUCT(BlueprintType)
struct FPlayerParryActionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parry", meta = (ToolTip = "玩家弹反蒙太奇，需包含 Parry Section。弹反体力、冷却和反馈仍由盾牌配置持有。"))
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parry", meta = (ClampMin = "0", UIMin = "0", ToolTip = "弹反优先级。数值越大优先级越高。"))
	int32 Priority = 90;
};

USTRUCT(BlueprintType)
struct FPlayerPotionActionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Potion", meta = (ToolTip = "可选喝药蒙太奇。为空时即时治疗并进入冷却。"))
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Potion", meta = (ClampMin = "0", UIMin = "0", ToolTip = "喝药优先级。数值越大优先级越高。"))
	int32 Priority = 60;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Potion", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0", ToolTip = "没有 PotionMontage 时的即时治疗比例。蒙太奇 Notify 路径继续使用 UAnimNotify_PotionHeal 自己的 HealPercent。"))
	float HealPercent = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Potion", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "喝药冷却时间。"))
	float Cooldown = 2.f;
};

/**
 * Player-only non-attack action montage config.
 *
 * Attack montages stay in UAttackConfigDataAsset. HitReact/Death stay on
 * ABaseCharacter for now because they are shared by player and enemies.
 */
UCLASS(BlueprintType)
class TEST_API UPlayerActionConfigDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions")
	FPlayerDodgeActionConfig Dodge;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions")
	FPlayerBlockActionConfig Block;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions")
	FPlayerParryActionConfig Parry;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions")
	FPlayerPotionActionConfig Potion;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Priority", meta = (ToolTip = "非玩家主动动作的共享优先级。数值越大优先级越高。运行时不应修改，DataAsset 是配置真相源。"))
	FPlayerSharedActionPriorityConfig SharedPriority;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Legacy", meta = (ToolTip = "Legacy migration only. Use Dodge.Montage instead."))
	TObjectPtr<UAnimMontage> DodgeMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Legacy", meta = (ToolTip = "Legacy migration only. Use Block.Montage instead."))
	TObjectPtr<UAnimMontage> BlockMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Legacy", meta = (ToolTip = "Legacy migration only. Use Parry.Montage instead."))
	TObjectPtr<UAnimMontage> ParryMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Legacy", meta = (ToolTip = "Legacy migration only. Use Potion.Montage instead. Empty Potion.Montage remains valid."))
	TObjectPtr<UAnimMontage> PotionMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Legacy", meta = (ToolTip = "Legacy migration only. Use per-action Priority fields and SharedPriority instead."))
	FPlayerActionPriorityConfig PriorityConfig;

	UFUNCTION(BlueprintPure, Category = "Priority")
	int32 GetActionPriority(EPlayerActionType Action) const;

	UFUNCTION(BlueprintPure, Category = "Priority")
	bool IsStrictlyHigherPriority(EPlayerActionType NewAction, EPlayerActionType CurrentAction) const;

	UFUNCTION(BlueprintPure, Category = "Priority")
	bool IsAtLeastSamePriority(EPlayerActionType NewAction, EPlayerActionType CurrentAction) const;

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UPROPERTY(VisibleAnywhere, Category = "Legacy", meta = (ToolTip = "Legacy migration marker. Do not edit manually."))
	bool bLegacyConfigMigrated = false;

	void MigrateLegacyConfig(bool bForce = false);
	void LogConfigWarnings() const;
};
