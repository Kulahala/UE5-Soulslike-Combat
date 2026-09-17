// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/AttackConfigDataAsset.h"
#include "Combat/ComboDataAsset.h"

#include "Animation/AnimMontage.h"
#include "UObject/UnrealType.h"

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

void UAttackConfigDataAsset::PostLoad()
{
	Super::PostLoad();

	LogConfigWarnings();
}

#if WITH_EDITOR
void UAttackConfigDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	LogConfigWarnings();
}
#endif

void UAttackConfigDataAsset::LogConfigWarnings() const
{
#if WITH_EDITOR
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (!LightAttackCombo)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: LightAttackCombo is not set."), *GetName());
	}
	else
	{
		if (LightAttackCombo->ComboChain.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: LightAttackCombo '%s' has an empty ComboChain."),
			       *GetName(), *LightAttackCombo->GetName());
		}

		for (int32 Index = 0; Index < LightAttackCombo->ComboChain.Num(); ++Index)
		{
			const FComboSegment& Segment = LightAttackCombo->ComboChain[Index];
			if (!Segment.Montage)
			{
				UE_LOG(LogTemp, Warning, TEXT("%s: LightAttackCombo '%s' ComboChain[%d] has no Montage."),
				       *GetName(), *LightAttackCombo->GetName(), Index);
			}
			else if (Segment.EntrySection == NAME_None)
			{
				UE_LOG(LogTemp, Warning, TEXT("%s: LightAttackCombo '%s' ComboChain[%d] has no EntrySection."),
				       *GetName(), *LightAttackCombo->GetName(), Index);
			}
			else if (Segment.Montage->GetSectionIndex(Segment.EntrySection) == INDEX_NONE)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("%s: LightAttackCombo '%s' ComboChain[%d] EntrySection '%s' does not exist in Montage '%s'."),
				       *GetName(), *LightAttackCombo->GetName(), Index,
				       *Segment.EntrySection.ToString(), *GetNameSafe(Segment.Montage.Get()));
			}
		}
	}

	bool bHasSprintAttack = false;
	for (int32 Index = 0; Index < SpecialAttacks.Num(); ++Index)
	{
		const FSpecialAttackConfig& Config = SpecialAttacks[Index];
		bHasSprintAttack |= Config.Type == ESpecialAttackType::SprintAttack;

		if (!Config.Montage)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: SpecialAttacks[%d] %s has no Montage."),
			       *GetName(), Index, *UEnum::GetValueAsString(Config.Type));
		}

		if (Config.MotionWarping.bUseMotionWarping && Config.MotionWarping.MaxWarpDistance <= 0.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: SpecialAttacks[%d] %s enables Motion Warping but MaxWarpDistance is <= 0."),
			       *GetName(), Index, *UEnum::GetValueAsString(Config.Type));
		}
	}

	if (!bHasSprintAttack)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: SpecialAttacks has no SprintAttack entry; sprint attack input will not play an attack montage."), *GetName());
	}

	if (!ChargedAttack.Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: ChargedAttack has no Montage."), *GetName());
	}

	if (ChargedAttack.MinChargeHoldTime > ChargedAttack.MaxChargeHoldTime)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: ChargedAttack MinChargeHoldTime %.2f > MaxChargeHoldTime %.2f."),
		       *GetName(), ChargedAttack.MinChargeHoldTime, ChargedAttack.MaxChargeHoldTime);
	}

	if (ChargedAttack.MaxChargeHoldTime <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: ChargedAttack MaxChargeHoldTime %.2f must be > 0."), *GetName(), ChargedAttack.MaxChargeHoldTime);
	}

	if (ChargedAttack.MotionWarping.bUseMotionWarping && ChargedAttack.MotionWarping.MaxWarpDistance <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: ChargedAttack enables Motion Warping but MaxWarpDistance is <= 0."), *GetName());
	}
#endif
}
