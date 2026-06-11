#include "Combat/PlayerActionConfigDataAsset.h"

#include "Animation/AnimMontage.h"

int32 UPlayerActionConfigDataAsset::GetActionPriority(EPlayerActionType Action) const
{
	switch (Action)
	{
	case EPlayerActionType::None:
		return MIN_int32;
	case EPlayerActionType::Attack:
		return SharedPriority.Attack;
	case EPlayerActionType::Dodge:
		return Dodge.Priority;
	case EPlayerActionType::Block:
		return Block.Priority;
	case EPlayerActionType::Parry:
		return Parry.Priority;
	case EPlayerActionType::Potion:
		return Potion.Priority;
	case EPlayerActionType::HitReact:
		return SharedPriority.HitReact;
	case EPlayerActionType::Death:
		return SharedPriority.Death;
	}

	// 故意不写 default：让 -Wswitch 在 EPlayerActionType 加新值时提示这里需要同步。
	return MIN_int32;
}

bool UPlayerActionConfigDataAsset::IsStrictlyHigherPriority(EPlayerActionType NewAction,
                                                            EPlayerActionType CurrentAction) const
{
	return GetActionPriority(NewAction) > GetActionPriority(CurrentAction);
}

bool UPlayerActionConfigDataAsset::IsAtLeastSamePriority(EPlayerActionType NewAction,
                                                         EPlayerActionType CurrentAction) const
{
	return GetActionPriority(NewAction) >= GetActionPriority(CurrentAction);
}

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

	if (!Parry.Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Parry.Montage is not set."), *GetName());
	}

	// Potion.Montage 故意不检查：为空时走即时治疗 + 冷却 fallback。
	if (SharedPriority.Attack < 0 || Dodge.Priority < 0 || Block.Priority < 0 ||
		Parry.Priority < 0 || Potion.Priority < 0 || SharedPriority.HitReact < 0 ||
		SharedPriority.Death < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Action priority contains a negative value. Action priority expects non-negative values, larger numbers are higher priority."), *GetName());
	}

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
