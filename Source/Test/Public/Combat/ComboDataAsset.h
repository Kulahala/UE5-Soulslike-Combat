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

	// 蒙太奇section名称
	UPROPERTY(EditAnywhere, Category = "Animation")
	FName SectionName = NAME_None;

	// 该段伤害倍率（相对基础伤害）
	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float DamageMultiplier = 1.0f;

	// 该段体力消耗
	UPROPERTY(EditAnywhere, Category = "Stamina", meta = (ClampMin = "0.0"))
	float StaminaCost = 15.0f;

	// 该段韧性伤害倍率（相对武器基础韧性伤害）
	UPROPERTY(EditAnywhere, Category = "Poise", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float PoiseDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping")
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
	// 连招使用的蒙太奇
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> ComboMontage = nullptr;

	// 连招链（按顺序）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
	TArray<FComboSegment> ComboChain;

	// 获取连招总段数
	UFUNCTION(BlueprintCallable, Category = "Combo")
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
