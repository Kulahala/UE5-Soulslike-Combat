#include "Items/ItemOwnershipComponent.h"

#include "Game/SoulslikeGameInstance.h"
#include "Misc/Guid.h"

UItemOwnershipComponent::UItemOwnershipComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UItemOwnershipComponent::BeginPlay()
{
	Super::BeginPlay();
	BuildDefinitionCatalog();
}

bool UItemOwnershipComponent::RestoreFromSave(const UTestSaveGame* SaveGame)
{
	OwnedItemInstances.Reset();
	EquippedSlots.Reset();
	BuildDefinitionCatalog();

	if (!SaveGame || !SaveGame->IsUsable())
	{
		UE_LOG(LogTemp, Warning, TEXT("Item ownership restore skipped: no usable save is available."));
		return false;
	}

	TSet<FName> SeenInstanceIds;
	for (const FTestItemInstanceRecord& ItemRecord : SaveGame->ItemInstances)
	{
		FString FailureReason;
		if (!ValidateItemRecord(ItemRecord, FailureReason))
		{
			UE_LOG(LogTemp, Warning, TEXT("Ignoring saved item instance '%s': %s"),
				*ItemRecord.InstanceId.ToString(), *FailureReason);
			continue;
		}

		if (SeenInstanceIds.Contains(ItemRecord.InstanceId))
		{
			UE_LOG(LogTemp, Warning, TEXT("Ignoring duplicate saved item InstanceId '%s'."), *ItemRecord.InstanceId.ToString());
			continue;
		}

		SeenInstanceIds.Add(ItemRecord.InstanceId);
		OwnedItemInstances.Add(ItemRecord);
	}

	TSet<FName> SeenSlotIds;
	TSet<FName> EquippedInstanceIds;
	for (const FTestEquipmentSlotRecord& SlotRecord : SaveGame->EquippedSlots)
	{
		if (SlotRecord.SlotId == NAME_None || SlotRecord.ItemInstanceId == NAME_None)
		{
			UE_LOG(LogTemp, Warning, TEXT("Ignoring saved equipment slot with an empty slot or instance ID."));
			continue;
		}

		if (SeenSlotIds.Contains(SlotRecord.SlotId))
		{
			UE_LOG(LogTemp, Warning, TEXT("Ignoring duplicate saved equipment slot '%s'."), *SlotRecord.SlotId.ToString());
			continue;
		}

		const FTestItemInstanceRecord* ItemRecord = GetOwnedItemInstance(SlotRecord.ItemInstanceId);
		if (!ItemRecord)
		{
			UE_LOG(LogTemp, Warning, TEXT("Ignoring equipment slot '%s': instance '%s' is not owned."),
				*SlotRecord.SlotId.ToString(), *SlotRecord.ItemInstanceId.ToString());
			continue;
		}

		const UItemDefinitionDataAsset* Definition = GetDefinition(ItemRecord->DefinitionId);
		if (!Definition || GetSlotId(Definition->GetEquipmentSlot()) != SlotRecord.SlotId)
		{
			UE_LOG(LogTemp, Warning, TEXT("Ignoring equipment slot '%s': instance '%s' has an incompatible definition."),
				*SlotRecord.SlotId.ToString(), *SlotRecord.ItemInstanceId.ToString());
			continue;
		}

		if (EquippedInstanceIds.Contains(SlotRecord.ItemInstanceId))
		{
			UE_LOG(LogTemp, Warning, TEXT("Ignoring duplicate equipped item instance '%s'."), *SlotRecord.ItemInstanceId.ToString());
			continue;
		}

		SeenSlotIds.Add(SlotRecord.SlotId);
		EquippedInstanceIds.Add(SlotRecord.ItemInstanceId);
		EquippedSlots.Add(SlotRecord);
	}

	return true;
}

bool UItemOwnershipComponent::TryGrantDefinition(FName DefinitionId, USoulslikeGameInstance* GameInstance,
                                                  FName& OutInstanceId)
{
	OutInstanceId = NAME_None;
	BuildDefinitionCatalog();

	if (!GameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Grant item '%s' failed: no SoulslikeGameInstance."), *DefinitionId.ToString());
		return false;
	}

	if (!GetDefinition(DefinitionId))
	{
		UE_LOG(LogTemp, Warning, TEXT("Grant item failed: DefinitionId '%s' is not in this player's catalog."),
			*DefinitionId.ToString());
		return false;
	}

	FTestItemInstanceRecord NewRecord;
	NewRecord.DefinitionId = DefinitionId;
	NewRecord.InstanceId = GenerateUniqueInstanceId();
	NewRecord.Quantity = 1;
	NewRecord.UpgradeLevel = 0;

	if (!GameInstance->AddOwnedItemInstance(NewRecord))
	{
		return false;
	}

	OwnedItemInstances.Add(NewRecord);
	OutInstanceId = NewRecord.InstanceId;
	return true;
}

bool UItemOwnershipComponent::TryEquipInstance(FName InstanceId, USoulslikeGameInstance* GameInstance)
{
	BuildDefinitionCatalog();

	if (!GameInstance || InstanceId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("Equip item failed: GameInstance or InstanceId is invalid."));
		return false;
	}

	const FTestItemInstanceRecord* ItemRecord = GetOwnedItemInstance(InstanceId);
	if (!ItemRecord)
	{
		UE_LOG(LogTemp, Warning, TEXT("Equip item failed: instance '%s' is not owned."), *InstanceId.ToString());
		return false;
	}

	const UItemDefinitionDataAsset* Definition = GetDefinition(ItemRecord->DefinitionId);
	const EItemEquipmentSlot EquipmentSlot = Definition ? Definition->GetEquipmentSlot() : EItemEquipmentSlot::None;
	const FName SlotId = GetSlotId(EquipmentSlot);
	if (!IsSupportedEquipmentSlot(EquipmentSlot) || SlotId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("Equip item failed: definition '%s' has no supported equipment slot."),
			*ItemRecord->DefinitionId.ToString());
		return false;
	}

	if (!GameInstance->SetEquippedItemSlot(SlotId, InstanceId))
	{
		return false;
	}

	UpdateLocalEquipmentSlot(SlotId, InstanceId);
	return true;
}

bool UItemOwnershipComponent::TryClearEquipmentSlot(EItemEquipmentSlot EquipmentSlot,
                                                     USoulslikeGameInstance* GameInstance)
{
	const FName SlotId = GetSlotId(EquipmentSlot);
	if (!GameInstance || SlotId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("Clear equipment slot failed: GameInstance or equipment slot is invalid."));
		return false;
	}

	if (!GameInstance->SetEquippedItemSlot(SlotId, NAME_None))
	{
		return false;
	}

	UpdateLocalEquipmentSlot(SlotId, NAME_None);
	return true;
}

FString UItemOwnershipComponent::BuildDebugSummary() const
{
	FString Result = FString::Printf(TEXT("Runtime item ownership: %d instance(s), %d equipped slot(s)."),
		OwnedItemInstances.Num(), EquippedSlots.Num());

	for (const FTestItemInstanceRecord& ItemRecord : OwnedItemInstances)
	{
		Result += FString::Printf(TEXT("\n  Item Instance=%s Definition=%s Quantity=%d Upgrade=%d"),
			*ItemRecord.InstanceId.ToString(), *ItemRecord.DefinitionId.ToString(),
			ItemRecord.Quantity, ItemRecord.UpgradeLevel);
	}

	for (const FTestEquipmentSlotRecord& SlotRecord : EquippedSlots)
	{
		Result += FString::Printf(TEXT("\n  Slot=%s Instance=%s"),
			*SlotRecord.SlotId.ToString(), *SlotRecord.ItemInstanceId.ToString());
	}

	return Result;
}

bool UItemOwnershipComponent::BuildDefinitionCatalog()
{
	if (bDefinitionCatalogInitialized)
	{
		return true;
	}

	bDefinitionCatalogInitialized = true;
	DefinitionsById.Reset();

	bool bAllDefinitionsValid = true;
	for (UItemDefinitionDataAsset* Definition : DefinitionCatalog)
	{
		if (!Definition)
		{
			UE_LOG(LogTemp, Warning, TEXT("Item ownership catalog contains an empty definition reference."));
			bAllDefinitionsValid = false;
			continue;
		}

		FString FailureReason;
		if (!Definition->IsDefinitionValid(FailureReason))
		{
			UE_LOG(LogTemp, Warning, TEXT("Item ownership catalog ignores '%s': %s"),
				*GetNameSafe(Definition), *FailureReason);
			bAllDefinitionsValid = false;
			continue;
		}

		const FName DefinitionId = Definition->GetDefinitionId();
		if (DefinitionsById.Contains(DefinitionId))
		{
			UE_LOG(LogTemp, Warning, TEXT("Item ownership catalog ignores duplicate DefinitionId '%s' on '%s'."),
				*DefinitionId.ToString(), *GetNameSafe(Definition));
			bAllDefinitionsValid = false;
			continue;
		}

		DefinitionsById.Add(DefinitionId, Definition);
	}

	return bAllDefinitionsValid;
}

bool UItemOwnershipComponent::ValidateItemRecord(const FTestItemInstanceRecord& ItemRecord,
                                                  FString& OutFailureReason) const
{
	if (ItemRecord.DefinitionId == NAME_None)
	{
		OutFailureReason = TEXT("DefinitionId is empty.");
		return false;
	}

	if (ItemRecord.InstanceId == NAME_None)
	{
		OutFailureReason = TEXT("InstanceId is empty.");
		return false;
	}

	if (ItemRecord.Quantity <= 0)
	{
		OutFailureReason = TEXT("Quantity must be positive.");
		return false;
	}

	if (ItemRecord.UpgradeLevel < 0)
	{
		OutFailureReason = TEXT("UpgradeLevel cannot be negative.");
		return false;
	}

	if (!GetDefinition(ItemRecord.DefinitionId))
	{
		OutFailureReason = FString::Printf(TEXT("DefinitionId '%s' is not in this player's catalog."),
			*ItemRecord.DefinitionId.ToString());
		return false;
	}

	return true;
}

const UItemDefinitionDataAsset* UItemOwnershipComponent::GetDefinition(FName DefinitionId) const
{
	if (UItemDefinitionDataAsset* const* FoundDefinition = DefinitionsById.Find(DefinitionId))
	{
		return *FoundDefinition;
	}

	return nullptr;
}

const FTestItemInstanceRecord* UItemOwnershipComponent::GetOwnedItemInstance(FName InstanceId) const
{
	return OwnedItemInstances.FindByPredicate([InstanceId](const FTestItemInstanceRecord& ItemRecord)
	{
		return ItemRecord.InstanceId == InstanceId;
	});
}

FName UItemOwnershipComponent::GetEquippedInstanceId(EItemEquipmentSlot EquipmentSlot) const
{
	const FName SlotId = GetSlotId(EquipmentSlot);
	if (SlotId == NAME_None)
	{
		return NAME_None;
	}

	if (const FTestEquipmentSlotRecord* SlotRecord = EquippedSlots.FindByPredicate(
		[SlotId](const FTestEquipmentSlotRecord& Candidate)
		{
			return Candidate.SlotId == SlotId;
		}))
	{
		return SlotRecord->ItemInstanceId;
	}

	return NAME_None;
}

bool UItemOwnershipComponent::IsSupportedEquipmentSlot(EItemEquipmentSlot EquipmentSlot)
{
	return EquipmentSlot == EItemEquipmentSlot::MainHand || EquipmentSlot == EItemEquipmentSlot::OffHand;
}

FName UItemOwnershipComponent::GetSlotId(EItemEquipmentSlot EquipmentSlot)
{
	switch (EquipmentSlot)
	{
	case EItemEquipmentSlot::MainHand:
		return FName(TEXT("MainHand"));
	case EItemEquipmentSlot::OffHand:
		return FName(TEXT("OffHand"));
	default:
		return NAME_None;
	}
}

void UItemOwnershipComponent::UpdateLocalEquipmentSlot(FName SlotId, FName ItemInstanceId)
{
	const int32 ExistingIndex = EquippedSlots.IndexOfByPredicate([SlotId](const FTestEquipmentSlotRecord& SlotRecord)
	{
		return SlotRecord.SlotId == SlotId;
	});

	if (ItemInstanceId == NAME_None)
	{
		if (ExistingIndex != INDEX_NONE)
		{
			EquippedSlots.RemoveAt(ExistingIndex);
		}
		return;
	}

	if (ExistingIndex != INDEX_NONE)
	{
		EquippedSlots[ExistingIndex].ItemInstanceId = ItemInstanceId;
		return;
	}

	FTestEquipmentSlotRecord NewSlotRecord;
	NewSlotRecord.SlotId = SlotId;
	NewSlotRecord.ItemInstanceId = ItemInstanceId;
	EquippedSlots.Add(NewSlotRecord);
}

FName UItemOwnershipComponent::GenerateUniqueInstanceId() const
{
	return FName(*FString::Printf(TEXT("Item_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
}
