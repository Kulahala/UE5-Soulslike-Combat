#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Character/CharacterTypes.h"
#include "PlayerActionConfigDataAsset.generated.h"

class UAnimMontage;
class USoundBase;
struct FPropertyChangedEvent;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", ToolTip = "防御期间体力自然恢复倍率。0.7 表示恢复速度为正常的 70%。"))
	float StaminaRegenMultiplier = 0.7f;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Potion", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0", ToolTip = "没有喝药蒙太奇时的即时治疗比例。蒙太奇 Notify 路径继续使用 UAnimNotify_PotionHeal 自己的 HealPercent。"))
	float HealPercent = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Potion", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "喝药冷却时间。"))
	float Cooldown = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Potion", meta = (ToolTip = "没有喝药蒙太奇时即时治疗播放的临时音效。使用喝药蒙太奇后请改用 Montage Sound Notify，避免重复播放。"))
	TObjectPtr<USoundBase> FallbackHealSound = nullptr;
};

/**
 * Player-only action montage config.
 *
 * Attack montages stay in UAttackConfigDataAsset. HitReact/Death stay on
 * ABaseCharacter for now because they are shared by player and enemies.
 */
UCLASS(BlueprintType)
class TEST_API UPlayerActionConfigDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions", meta = (ToolTip = "玩家翻滚动作配置。"))
	FPlayerDodgeActionConfig Dodge;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions", meta = (ToolTip = "玩家举盾防御动作配置。"))
	FPlayerBlockActionConfig Block;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions", meta = (ToolTip = "玩家弹反动作配置。"))
	FPlayerParryActionConfig Parry;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions", meta = (ToolTip = "玩家喝药动作配置。"))
	FPlayerPotionActionConfig Potion;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Priority", meta = (ToolTip = "Attack / HitReact / Death 的共享优先级。Attack 的蒙太奇配置仍归 UAttackConfigDataAsset；这里只保存取消判断需要的优先级。数值越大优先级越高。"))
	FPlayerSharedActionPriorityConfig SharedPriority;

	UFUNCTION(BlueprintPure, Category = "Priority", meta = (ToolTip = "返回指定玩家动作类型的优先级。未识别动作返回 0。"))
	int32 GetActionPriority(EPlayerActionType Action) const;

	UFUNCTION(BlueprintPure, Category = "Priority", meta = (ToolTip = "判断新动作优先级是否严格高于当前动作。"))
	bool IsStrictlyHigherPriority(EPlayerActionType NewAction, EPlayerActionType CurrentAction) const;

	UFUNCTION(BlueprintPure, Category = "Priority", meta = (ToolTip = "判断新动作优先级是否不低于当前动作。"))
	bool IsAtLeastSamePriority(EPlayerActionType NewAction, EPlayerActionType CurrentAction) const;

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void LogConfigWarnings() const;
};
