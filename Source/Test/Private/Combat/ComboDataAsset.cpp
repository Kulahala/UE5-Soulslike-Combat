#include "Combat/ComboDataAsset.h"

#include "Animation/AnimMontage.h"
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

	if (ComboChain.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: ComboChain is empty; light attacks will have no playable combo segments."), *GetName());
	}

	for (int32 Index = 0; Index < ComboChain.Num(); ++Index)
	{
		const FComboSegment& Segment = ComboChain[Index];
		if (!Segment.Montage)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: ComboChain[%d] has no Montage."), *GetName(), Index);
		}
		else if (Segment.EntrySection == NAME_None)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: ComboChain[%d] has no EntrySection."), *GetName(), Index);
		}
		else if (Segment.Montage->GetSectionIndex(Segment.EntrySection) == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: ComboChain[%d] EntrySection '%s' does not exist in Montage '%s'."),
			       *GetName(), Index, *Segment.EntrySection.ToString(), *GetNameSafe(Segment.Montage.Get()));
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
