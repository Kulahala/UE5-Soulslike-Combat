#include "Items/ItemDefinitionDataAsset.h"

#include "Items/Shield/Shield.h"
#include "Items/Weapon/Weapon.h"
#include "UObject/UnrealType.h"

bool UItemDefinitionDataAsset::IsDefinitionValid(FString& OutFailureReason) const
{
	if (DefinitionId == NAME_None)
	{
		OutFailureReason = TEXT("DefinitionId is empty.");
		return false;
	}

	if (EquipmentSlot == EItemEquipmentSlot::None)
	{
		return true;
	}

	const UClass* RuntimeClass = RuntimeItemActorClass.Get();
	if (!RuntimeClass)
	{
		OutFailureReason = TEXT("RuntimeItemActorClass is required for an equippable definition.");
		return false;
	}

	if (EquipmentSlot == EItemEquipmentSlot::MainHand && !RuntimeClass->IsChildOf(AWeapon::StaticClass()))
	{
		OutFailureReason = TEXT("MainHand definitions must use an AWeapon runtime class.");
		return false;
	}

	if (EquipmentSlot == EItemEquipmentSlot::OffHand && !RuntimeClass->IsChildOf(AShield::StaticClass()))
	{
		OutFailureReason = TEXT("OffHand definitions must use an AShield runtime class.");
		return false;
	}

	return true;
}

void UItemDefinitionDataAsset::PostLoad()
{
	Super::PostLoad();
	LogConfigWarnings();
}

#if WITH_EDITOR
void UItemDefinitionDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	LogConfigWarnings();
}
#endif

void UItemDefinitionDataAsset::LogConfigWarnings() const
{
#if WITH_EDITOR
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	FString FailureReason;
	if (!IsDefinitionValid(FailureReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: invalid item definition: %s"), *GetName(), *FailureReason);
	}
#endif
}
