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

	MigrateLegacyConfig();
	LogConfigWarnings();
}

#if WITH_EDITOR
void UPlayerActionConfigDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const FName MemberPropertyName = PropertyChangedEvent.GetMemberPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UPlayerActionConfigDataAsset, DodgeMontage) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UPlayerActionConfigDataAsset, BlockMontage) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UPlayerActionConfigDataAsset, ParryMontage) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UPlayerActionConfigDataAsset, PotionMontage) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UPlayerActionConfigDataAsset, PriorityConfig) ||
		MemberPropertyName == GET_MEMBER_NAME_CHECKED(UPlayerActionConfigDataAsset, PriorityConfig))
	{
		MigrateLegacyConfig(true);
	}

	LogConfigWarnings();
}
#endif

void UPlayerActionConfigDataAsset::MigrateLegacyConfig(bool bForce)
{
	if (bLegacyConfigMigrated && !bForce)
	{
		return;
	}

	if ((bForce || !Dodge.Montage) && DodgeMontage)
	{
		Dodge.Montage = DodgeMontage;
	}

	if ((bForce || !Block.Montage) && BlockMontage)
	{
		Block.Montage = BlockMontage;
	}

	if ((bForce || !Parry.Montage) && ParryMontage)
	{
		Parry.Montage = ParryMontage;
	}

	if ((bForce || !Potion.Montage) && PotionMontage)
	{
		Potion.Montage = PotionMontage;
	}

	SharedPriority.Attack = PriorityConfig.Attack;
	Dodge.Priority = PriorityConfig.Dodge;
	Block.Priority = PriorityConfig.Block;
	Parry.Priority = PriorityConfig.Parry;
	Potion.Priority = PriorityConfig.Potion;
	SharedPriority.HitReact = PriorityConfig.HitReact;
	SharedPriority.Death = PriorityConfig.Death;

	bLegacyConfigMigrated = true;
}

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
