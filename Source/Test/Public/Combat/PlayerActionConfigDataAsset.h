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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions", meta = (ToolTip = "玩家翻滚蒙太奇，需包含方向 Section。"))
	TObjectPtr<UAnimMontage> DodgeMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions", meta = (ToolTip = "玩家防御蒙太奇，需包含 BlockRaise / BlockIdle Section。"))
	TObjectPtr<UAnimMontage> BlockMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions", meta = (ToolTip = "玩家弹反蒙太奇，需包含 Parry Section。"))
	TObjectPtr<UAnimMontage> ParryMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions", meta = (ToolTip = "可选喝药蒙太奇。为空时即时治疗并进入冷却。"))
	TObjectPtr<UAnimMontage> PotionMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Priority", meta = (ToolTip = "玩家动作优先级。数值越大优先级越高。运行时不应修改，DataAsset 是配置真相源。"))
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
	void LogConfigWarnings() const;
};
