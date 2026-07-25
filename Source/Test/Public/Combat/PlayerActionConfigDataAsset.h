#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Character/CharacterTypes.h"
#include "PlayerActionConfigDataAsset.generated.h"

class UAnimMontage;
class USoundBase;
struct FPropertyChangedEvent;

USTRUCT(BlueprintType)
struct FPlayerDodgeActionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge", meta = (ToolTip = "玩家翻滚蒙太奇，需包含方向 Section。"))
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "翻滚体力消耗。"))
	float StaminaCost = 15.f;
};

USTRUCT(BlueprintType)
struct FPlayerBlockActionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Block", meta = (ToolTip = "玩家防御蒙太奇，需包含 BlockRaise Section。"))
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Block", meta = (ToolTip = "举盾起手 Section 名称。"))
	FName BlockRaiseSection = FName("BlockRaise");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", ToolTip = "防御期间体力自然恢复倍率。0.7 表示恢复速度为正常的 70%。"))
	float StaminaRegenMultiplier = 0.7f;
};

USTRUCT(BlueprintType)
struct FPlayerGuardBreakActionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GuardBreak", meta = (ToolTip = "玩家格挡体力耗尽或 Exhausted 受击时的专用破防蒙太奇。为空时会回退到 Exhausted 恢复路径。"))
	TObjectPtr<UAnimMontage> Montage = nullptr;
};

USTRUCT(BlueprintType)
struct FPlayerParryActionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parry", meta = (ToolTip = "玩家弹反蒙太奇，需包含 Parry Section。弹反体力、冷却和反馈仍由盾牌配置持有。"))
	TObjectPtr<UAnimMontage> Montage = nullptr;

};

USTRUCT(BlueprintType)
struct FPlayerPotionActionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Potion", meta = (ToolTip = "可选喝药蒙太奇。为空时即时治疗并进入冷却。"))
	TObjectPtr<UAnimMontage> Montage = nullptr;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions", meta = (ToolTip = "玩家破防硬直动作配置。"))
	FPlayerGuardBreakActionConfig GuardBreak;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions", meta = (ToolTip = "玩家弹反动作配置。"))
	FPlayerParryActionConfig Parry;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions", meta = (ToolTip = "玩家喝药动作配置。"))
	FPlayerPotionActionConfig Potion;

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void LogConfigWarnings() const;
};
