#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HitReactionConfigDataAsset.generated.h"

class UAnimMontage;
class USoundBase;
class UParticleSystem;
struct FPropertyChangedEvent;

USTRUCT(BlueprintType)
struct FHitReactConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact", meta = (ToolTip = "受击反应蒙太奇。为空时不播放受击反应蒙太奇。"))
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact", meta = (ToolTip = "正面受击 Section 名称。"))
	FName FrontSection = FName("FromFront");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact", meta = (ToolTip = "背面受击 Section 名称。"))
	FName BackSection = FName("FromBack");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact", meta = (ToolTip = "左侧受击 Section 名称。"))
	FName LeftSection = FName("FromLeft");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact", meta = (ToolTip = "右侧受击 Section 名称。"))
	FName RightSection = FName("FromRight");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact", meta = (ToolTip = "普通受击音效。格挡成功不使用这里。"))
	TObjectPtr<USoundBase> HitSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact", meta = (ToolTip = "普通受击粒子。格挡成功不使用这里。"))
	TObjectPtr<UParticleSystem> HitParticle = nullptr;
};

USTRUCT(BlueprintType)
struct FDeathConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death", meta = (ToolTip = "死亡蒙太奇。为空时不播放死亡蒙太奇。"))
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death", meta = (ToolTip = "可选死亡 Section 列表。空数组表示直接播放 Montage，不跳转 Section；敌人可填多个 Section 名支持随机选择。"))
	TArray<FName> Sections;
};

USTRUCT(BlueprintType)
struct FEnemyStanceBreakConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StanceBreak", meta = (ToolTip = "敌人专用失衡蒙太奇。普通 HitReact 不会作为失衡回退。"))
	TObjectPtr<UAnimMontage> Montage = nullptr;
};

/**
 * Shared character hit reaction and death montage config.
 *
 * Behavior stays on ABaseCharacter / subclasses. This asset only owns montage
 * references and section names.
 */
UCLASS(BlueprintType)
class TEST_API UHitReactionConfigDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact", meta = (ToolTip = "共享受击反应配置，包含蒙太奇、方向 Section 和普通受击反馈。"))
	FHitReactConfig HitReact;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death", meta = (ToolTip = "共享死亡蒙太奇配置。"))
	FDeathConfig Death;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StanceBreak", meta = (ToolTip = "敌人专用失衡配置。玩家 Guard Break 使用 PlayerActionConfig 的独立字段。"))
	FEnemyStanceBreakConfig StanceBreak;

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void LogConfigWarnings() const;
};
