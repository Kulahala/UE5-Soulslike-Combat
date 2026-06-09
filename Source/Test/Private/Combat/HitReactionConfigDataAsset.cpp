#include "Combat/HitReactionConfigDataAsset.h"

void UHitReactionConfigDataAsset::PostLoad()
{
	Super::PostLoad();

	LogConfigWarnings();
}

#if WITH_EDITOR
void UHitReactionConfigDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	LogConfigWarnings();
}
#endif

void UHitReactionConfigDataAsset::LogConfigWarnings() const
{
#if WITH_EDITOR
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (!HitReact.Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: HitReact.Montage is not set."), *GetName());
	}

	if (!Death.Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Death.Montage is not set."), *GetName());
	}
#endif
}
