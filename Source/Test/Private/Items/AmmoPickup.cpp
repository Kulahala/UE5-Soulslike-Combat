#include "Items/AmmoPickup.h"

#include "Character/MyCharacter.h"

void AAmmoPickup::OnPickup_Implementation(AActor* Picker)
{
	if (AMyCharacter* Character = Cast<AMyCharacter>(Picker))
	{
		TryClaimPersistentWorldPickup(Character);
	}
}
