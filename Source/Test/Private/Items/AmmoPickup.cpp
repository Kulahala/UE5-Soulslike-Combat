#include "Items/AmmoPickup.h"

bool AAmmoPickup::TryGrantPickup(AMyCharacter* Picker, USoundBase*& OutPickupSound)
{
	return TryClaimPersistentWorldPickup(Picker, OutPickupSound);
}
