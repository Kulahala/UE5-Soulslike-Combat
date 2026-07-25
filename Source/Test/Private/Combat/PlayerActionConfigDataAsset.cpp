#include "Combat/PlayerActionConfigDataAsset.h"

#include "Animation/AnimMontage.h"

void UPlayerActionConfigDataAsset::PostLoad()
{
	Super::PostLoad();

	LogConfigWarnings();
}

#if WITH_EDITOR
void UPlayerActionConfigDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	LogConfigWarnings();
}
#endif

void UPlayerActionConfigDataAsset::LogConfigWarnings() const
{
#if WITH_EDITOR
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (!Dodge.Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Dodge.Montage is not set."), *GetName());
	}

	if (!Block.Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Block.Montage is not set."), *GetName());
	}

	if (!GuardBreak.Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: GuardBreak.Montage is not set; guard breaks will fall back to Exhausted recovery."), *GetName());
	}

	if (!Parry.Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Parry.Montage is not set."), *GetName());
	}

	// Potion.Montage 故意不检查：为空时走即时治疗 + 冷却 fallback。
	if (Dodge.StaminaCost < 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Dodge.StaminaCost is negative."), *GetName());
	}

	if (Block.StaminaRegenMultiplier < 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Block.StaminaRegenMultiplier is negative."), *GetName());
	}

	if (Potion.Cooldown < 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Potion.Cooldown is negative."), *GetName());
	}

	if (Potion.HealPercent < 0.f || Potion.HealPercent > 1.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Potion.HealPercent should be in [0, 1]."), *GetName());
	}
#endif
}
