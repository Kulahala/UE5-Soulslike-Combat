#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/PlayerAttackMotionWarpingConfig.h"
#include "ComboDataAsset.generated.h"

class UAnimMontage;
struct FPropertyChangedEvent;

USTRUCT(BlueprintType)
struct FComboSegment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ToolTip = "该连招段要播放的 Montage。"))
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ToolTip = "该连招段在 Montage 中的入口 Section，默认从 Entry 开始。"))
	FName EntrySection = FName(TEXT("Entry"));

	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ClampMin = "0.1", ClampMax = "5.0", ToolTip = "该段伤害倍率（相对武器基础伤害）。"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Stamina", meta = (ClampMin = "0.0", ToolTip = "该段攻击消耗的体力。"))
	float StaminaCost = 15.0f;

	UPROPERTY(EditAnywhere, Category = "Poise", meta = (ClampMin = "0.1", ClampMax = "5.0", ToolTip = "该段韧性伤害倍率（相对武器基础韧性伤害）。"))
	float PoiseDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping", meta = (ToolTip = "该连招段的锁定攻击 Motion Warping 配置。"))
	FPlayerAttackMotionWarpingConfig MotionWarping;
};

/**
 * 连招数据资产类
 */
UCLASS(BlueprintType)
class TEST_API UComboDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo", meta = (ToolTip = "轻攻击连招段列表，按输入续接顺序执行。"))
	TArray<FComboSegment> ComboChain;

	// 获取连招总段数
	UFUNCTION(BlueprintCallable, Category = "Combo", meta = (ToolTip = "返回连招段总数。"))
	int32 GetComboCount() const { return ComboChain.Num(); }

	// 获取指定段的配置（带边界检查）
	const FComboSegment* GetSegment(int32 Index) const
	{
		return ComboChain.IsValidIndex(Index) ? &ComboChain[Index] : nullptr;
	}

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void LogConfigWarnings() const;
};
