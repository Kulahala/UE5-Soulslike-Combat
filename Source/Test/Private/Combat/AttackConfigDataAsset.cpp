// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/AttackConfigDataAsset.h"
#include "Combat/ComboDataAsset.h"

const FSpecialAttackConfig* UAttackConfigDataAsset::FindSpecialAttack(ESpecialAttackType AttackType) const
{
	// 线性遍历查找（3-5 个条目，性能无影响）
	for (const FSpecialAttackConfig& Config : SpecialAttacks)
	{
		if (Config.Type == AttackType)
		{
			return &Config;
		}
	}
	return nullptr;
}

void UAttackConfigDataAsset::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	// 编辑器模式下验证配置完整性
	if (!LightAttackCombo)
	{
		UE_LOG(LogTemp, Warning, TEXT("AttackConfigDataAsset: LightAttackCombo is not set"));
	}

	// 验证特殊攻击配置
	for (const FSpecialAttackConfig& Config : SpecialAttacks)
	{
		if (!Config.Montage)
		{
			UE_LOG(LogTemp, Warning, TEXT("AttackConfigDataAsset: SpecialAttack %s has no Montage"),
				*UEnum::GetValueAsString(Config.Type));
		}
	}
#endif
}
