#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PlayerCharacterProfileDataAsset.generated.h"

class UAttackConfigDataAsset;
class UHitReactionConfigDataAsset;
class UPlayerActionConfigDataAsset;
struct FPropertyChangedEvent;

/**
 * Thin player character profile/manifest.
 *
 * This asset is the single editable entry point for a playable character and
 * references smaller system-specific config assets instead of duplicating them.
 */
UCLASS(BlueprintType)
class TEST_API UPlayerCharacterProfileDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ToolTip = "玩家角色的攻击配置资产。"))
	TObjectPtr<UAttackConfigDataAsset> AttackConfig = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actions", meta = (ToolTip = "玩家角色的非攻击动作蒙太奇配置。"))
	TObjectPtr<UPlayerActionConfigDataAsset> ActionConfig = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ToolTip = "玩家角色的受击反应和死亡蒙太奇配置。"))
	TObjectPtr<UHitReactionConfigDataAsset> ReactionConfig = nullptr;

	FORCEINLINE UAttackConfigDataAsset* GetAttackConfig() const { return AttackConfig.Get(); }

	FORCEINLINE UPlayerActionConfigDataAsset* GetActionConfig() const { return ActionConfig.Get(); }

	FORCEINLINE UHitReactionConfigDataAsset* GetReactionConfig() const { return ReactionConfig.Get(); }

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void LogConfigWarnings() const;
};
