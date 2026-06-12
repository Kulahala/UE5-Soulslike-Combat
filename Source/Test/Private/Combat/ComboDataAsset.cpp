#include "Combat/ComboDataAsset.h"

#include "UObject/UnrealType.h"

void UComboDataAsset::PostLoad()
{
	Super::PostLoad();

	LogConfigWarnings();
}

#if WITH_EDITOR
void UComboDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	LogConfigWarnings();
}
#endif

void UComboDataAsset::LogConfigWarnings() const
{
#if WITH_EDITOR
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (!ComboMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: ComboMontage is not set."), *GetName());
	}

	if (ComboChain.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: ComboChain is empty; light attacks will have no playable combo segments."), *GetName());
	}

	for (int32 Index = 0; Index < ComboChain.Num(); ++Index)
	{
		const FComboSegment& Segment = ComboChain[Index];
		if (Segment.SectionName == NAME_None)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: ComboChain[%d] has no SectionName."), *GetName(), Index);
		}

		if (Segment.DamageMultiplier <= 0.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: ComboChain[%d] DamageMultiplier %.2f is <= 0."), *GetName(), Index, Segment.DamageMultiplier);
		}

		if (Segment.PoiseDamageMultiplier <= 0.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: ComboChain[%d] PoiseDamageMultiplier %.2f is <= 0."), *GetName(), Index, Segment.PoiseDamageMultiplier);
		}

		if (Segment.MotionWarping.bUseMotionWarping && Segment.MotionWarping.MaxWarpDistance <= 0.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: ComboChain[%d] enables Motion Warping but MaxWarpDistance is <= 0."), *GetName(), Index);
		}
	}
#endif
}
