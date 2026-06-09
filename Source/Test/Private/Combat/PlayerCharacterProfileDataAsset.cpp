#include "Combat/PlayerCharacterProfileDataAsset.h"

#include "Combat/AttackConfigDataAsset.h"
#include "Combat/HitReactionConfigDataAsset.h"
#include "Combat/PlayerActionConfigDataAsset.h"
#include "UObject/UnrealType.h"

void UPlayerCharacterProfileDataAsset::PostLoad()
{
	Super::PostLoad();

	LogConfigWarnings();
}

#if WITH_EDITOR
void UPlayerCharacterProfileDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	LogConfigWarnings();
}
#endif

void UPlayerCharacterProfileDataAsset::LogConfigWarnings() const
{
#if WITH_EDITOR
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (!AttackConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: AttackConfig is not set."), *GetName());
	}

	if (!ActionConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: ActionConfig is not set."), *GetName());
	}

	if (!ReactionConfig)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("%s: ReactionConfig is not set. Player hit reactions and death montages will not play."),
		       *GetName());
	}
#endif
}
