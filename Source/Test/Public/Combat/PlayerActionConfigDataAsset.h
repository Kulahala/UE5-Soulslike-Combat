#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PlayerActionConfigDataAsset.generated.h"

class UAnimMontage;
struct FPropertyChangedEvent;

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

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void LogConfigWarnings() const;
};
