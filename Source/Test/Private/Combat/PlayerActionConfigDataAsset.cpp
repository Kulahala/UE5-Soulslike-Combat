#include "Combat/PlayerActionConfigDataAsset.h"

#include "Animation/AnimMontage.h"
#include "UObject/UnrealType.h"

int32 UPlayerActionConfigDataAsset::GetActionPriority(EPlayerActionType Action) const
{
	switch (Action)
	{
	case EPlayerActionType::None:
		return MIN_int32;
	case EPlayerActionType::Attack:
		return PriorityConfig.Attack;
	case EPlayerActionType::Dodge:
		return PriorityConfig.Dodge;
	case EPlayerActionType::Block:
		return PriorityConfig.Block;
	case EPlayerActionType::Parry:
		return PriorityConfig.Parry;
	case EPlayerActionType::Potion:
		return PriorityConfig.Potion;
	case EPlayerActionType::HitReact:
		return PriorityConfig.HitReact;
	case EPlayerActionType::Death:
		return PriorityConfig.Death;
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

	if (!DodgeMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: DodgeMontage is not set."), *GetName());
	}

	if (!BlockMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: BlockMontage is not set."), *GetName());
	}

	if (!ParryMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: ParryMontage is not set."), *GetName());
	}

	// PotionMontage 故意不检查：为空时走即时治疗 + 冷却 fallback。
	if (PriorityConfig.Attack < 0 || PriorityConfig.Dodge < 0 || PriorityConfig.Block < 0 ||
		PriorityConfig.Parry < 0 || PriorityConfig.Potion < 0 || PriorityConfig.HitReact < 0 ||
		PriorityConfig.Death < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: PriorityConfig contains a negative value. Action priority expects non-negative values, larger numbers are higher priority."), *GetName());
	}
#endif
}
