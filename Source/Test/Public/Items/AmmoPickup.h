#pragma once

#include "CoreMinimal.h"
#include "Items/item.h"
#include "AmmoPickup.generated.h"

/** 只领取 Ammo Container Reserve 的固定世界拾取物。 */
UCLASS()
class TEST_API AAmmoPickup : public Aitem
{
	GENERATED_BODY()

public:
protected:
	virtual bool TryGrantPickup(AMyCharacter* Picker, USoundBase*& OutPickupSound) override;
	virtual bool RequiresPersistentWorldClaim() const override { return true; }
};
