#include "Combat/PlayerActionConfigDataAsset.h"

#include "Animation/AnimMontage.h"
#include "UObject/UnrealType.h"

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
#endif
}
