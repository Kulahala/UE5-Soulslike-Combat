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
	LoadedAmmoContainers.Reset();
	OwnedItemSourceIndices.Reset();
	LoadedAmmoSourceIndices.Reset();
	BuildDefinitionCatalog();

	if (!SaveGame || !SaveGame->IsPersistable())
	{
		UE_LOG(LogTemp, Warning, TEXT("Item ownership restore skipped: no persistable save is available."));
		return false;
	}

	TSet<FName> SeenInstanceIds;
	for (int32 SourceIndex = 0; SourceIndex < SaveGame->ItemInstances.Num(); ++SourceIndex)
	{
		const FTestItemInstanceRecord& ItemRecord = SaveGame->ItemInstances[SourceIndex];
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
		OwnedItemSourceIndices.Add(ItemRecord.InstanceId, SourceIndex);
	}

	TSet<FName> SeenAmmoDefinitionIds;
	for (int32 SourceIndex = 0; SourceIndex < SaveGame->LoadedAmmoContainers.Num(); ++SourceIndex)
	{
		const FTestAmmoContainerRecord& ContainerRecord = SaveGame->LoadedAmmoContainers[SourceIndex];
		FString FailureReason;
		if (!ValidateLoadedAmmoContainer(ContainerRecord, FailureReason))
		{
			UE_LOG(LogTemp, Warning, TEXT("Ignoring saved loaded-ammo container '%s': %s"),
				*ContainerRecord.DefinitionId.ToString(), *FailureReason);
			continue;
		}

		if (SeenAmmoDefinitionIds.Contains(ContainerRecord.DefinitionId))
		{
			UE_LOG(LogTemp, Warning, TEXT("Ignoring duplicate saved loaded-ammo container '%s'."),
				*ContainerRecord.DefinitionId.ToString());
			continue;
		}

		SeenAmmoDefinitionIds.Add(ContainerRecord.DefinitionId);
		LoadedAmmoContainers.Add(ContainerRecord);
		LoadedAmmoSourceIndices.Add(ContainerRecord.DefinitionId, SourceIndex);
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
	return TryGrantDefinitionQuantity(DefinitionId, 1, GameInstance, OutInstanceId);
}

bool UItemOwnershipComponent::TryGrantDefinitionQuantity(FName DefinitionId, int32 Quantity,
	USoulslikeGameInstance* GameInstance, FName& OutInstanceId)
{
	OutInstanceId = NAME_None;
	BuildDefinitionCatalog();

	if (!GameInstance || Quantity <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Grant item '%s' failed: GameInstance is unavailable or quantity is invalid."),
			*DefinitionId.ToString());
		return false;
	}

	const UItemDefinitionDataAsset* Definition = GetDefinition(DefinitionId);
	if (!Definition)
	{
		UE_LOG(LogTemp, Warning, TEXT("Grant item failed: DefinitionId '%s' is not in this player's catalog."),
			*DefinitionId.ToString());
		return false;
	}

	if (Definition->UsesAmmoContainer())
	{
		TArray<FTestItemInstanceSelection> ValidReserveInstances;
		GetValidReserveInstances(DefinitionId, ValidReserveInstances);
		if (!GameInstance->GrantAmmoReserve(DefinitionId, Quantity, Definition->GetReserveAmmoStackLimit(),
			ValidReserveInstances, OutInstanceId))
		{
			return false;
		}

		if (!RestoreFromSave(GameInstance->GetCurrentSaveGame()))
		{
			UE_LOG(LogTemp, Error, TEXT("Grant ammo reserve succeeded but the local ownership cache could not be restored."));
			return false;
		}

		BroadcastOwnedQuantity(DefinitionId);
		BroadcastLoadedAmmoQuantity(DefinitionId);
		return true;
	}

	FTestItemInstanceRecord NewRecord;
	NewRecord.DefinitionId = DefinitionId;
	NewRecord.InstanceId = GenerateUniqueInstanceId();
	NewRecord.Quantity = Quantity;
	NewRecord.UpgradeLevel = 0;

	if (!GameInstance->AddOwnedItemInstance(NewRecord))
	{
		return false;
	}

	OwnedItemInstances.Add(NewRecord);
	OutInstanceId = NewRecord.InstanceId;
	BroadcastOwnedQuantity(DefinitionId);
	return true;
}

bool UItemOwnershipComponent::TryConsumeLoadedAmmo(FName DefinitionId, int32 Quantity,
	USoulslikeGameInstance* GameInstance)
{
	BuildDefinitionCatalog();
	const UItemDefinitionDataAsset* Definition = GetDefinition(DefinitionId);
	if (!GameInstance || !Definition || !Definition->UsesAmmoContainer() || Quantity <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Consume loaded ammo '%s' failed: GameInstance, ammo definition, or quantity is invalid."),
			*DefinitionId.ToString());
		return false;
	}

	const int32 LoadedQuantity = GetLoadedAmmoQuantity(DefinitionId);
	if (LoadedQuantity < Quantity)
	{
		UE_LOG(LogTemp, Warning, TEXT("Consume loaded ammo '%s' rejected: not enough loaded ammo."),
			*DefinitionId.ToString());
		return false;
	}

	FTestAmmoContainerSelection Selection;
	Selection.DefinitionId = DefinitionId;
	Selection.ExpectedLoadedQuantity = LoadedQuantity;
	if (const int32* SourceIndex = LoadedAmmoSourceIndices.Find(DefinitionId))
	{
		Selection.SourceContainerIndex = *SourceIndex;
	}

	if (Selection.SourceContainerIndex == INDEX_NONE
		|| !GameInstance->ConsumeLoadedAmmo(Selection, Quantity))
	{
		return false;
	}

	if (!RestoreFromSave(GameInstance->GetCurrentSaveGame()))
	{
		UE_LOG(LogTemp, Error, TEXT("Consume loaded ammo succeeded but the local ownership cache could not be restored."));
		return false;
	}

	BroadcastLoadedAmmoQuantity(DefinitionId);
	return true;
}

bool UItemOwnershipComponent::TryRestockAmmoAtCheckpoint(FName GameplayMapName, FName CheckpointId,
	USoulslikeGameInstance* GameInstance)
{
	BuildDefinitionCatalog();
	if (!GameInstance || GameplayMapName == NAME_None || CheckpointId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("Restock ammo failed: GameInstance, map, or checkpoint is invalid."));
		return false;
	}

	TArray<FName> AmmoDefinitionIds;
	GetAmmoDefinitionIds(AmmoDefinitionIds);
	TMap<FName, int32> PreviousReserveQuantities;
	TMap<FName, int32> PreviousLoadedQuantities;
	TArray<FTestAmmoRefillRequest> RefillRequests;
	for (const FName DefinitionId : AmmoDefinitionIds)
	{
		PreviousReserveQuantities.Add(DefinitionId, GetReserveAmmoQuantity(DefinitionId));
		PreviousLoadedQuantities.Add(DefinitionId, GetLoadedAmmoQuantity(DefinitionId));

		FTestAmmoRefillRequest RefillRequest;
		if (BuildAmmoRefillRequest(DefinitionId, RefillRequest))
		{
			RefillRequests.Add(MoveTemp(RefillRequest));
		}
	}

	if (!GameInstance->ActivateCheckpointAndRefillAmmo(GameplayMapName, CheckpointId, RefillRequests))
	{
		return false;
	}

	if (!RestoreFromSave(GameInstance->GetCurrentSaveGame()))
	{
		UE_LOG(LogTemp, Error, TEXT("Checkpoint ammo refill succeeded but the local ownership cache could not be restored."));
		return false;
	}

	for (const FName DefinitionId : AmmoDefinitionIds)
	{
		if (PreviousReserveQuantities.FindRef(DefinitionId) != GetReserveAmmoQuantity(DefinitionId))
		{
			BroadcastOwnedQuantity(DefinitionId);
		}
		if (PreviousLoadedQuantities.FindRef(DefinitionId) != GetLoadedAmmoQuantity(DefinitionId))
		{
			BroadcastLoadedAmmoQuantity(DefinitionId);
		}
	}

	return true;
}

bool UItemOwnershipComponent::VerifyAmmoRefillFixture(FName DefinitionId, USoulslikeGameInstance* GameInstance) const
{
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Verify ammo refill fixture failed: GameInstance is unavailable."));
		return false;
	}

	FTestAmmoRefillRequest RefillRequest;
	if (!BuildAmmoRefillRequest(DefinitionId, RefillRequest))
	{
		UE_LOG(LogTemp, Warning, TEXT("Verify ammo refill fixture failed: '%s' has no transferable validated reserve ammo."),
			*DefinitionId.ToString());
		return false;
	}

	return GameInstance->VerifyAmmoRefillFixture(RefillRequest);
}

bool UItemOwnershipComponent::TryClaimWorldItem(FName PersistentId, FName DefinitionId,
	                                                USoulslikeGameInstance* GameInstance, bool bRequestAutoEquip,
	                                                FName& OutInstanceId, bool& bOutAutoEquipped,
	                                                FName PendingRewardId)
{
	OutInstanceId = NAME_None;
	bOutAutoEquipped = false;
	BuildDefinitionCatalog();

	if (!GameInstance || PersistentId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("Claim world item failed: no GameInstance or PersistentId."));
		return false;
	}

	const UItemDefinitionDataAsset* Definition = GetDefinition(DefinitionId);
	if (!Definition)
	{
		UE_LOG(LogTemp, Warning, TEXT("Claim world item failed: DefinitionId '%s' is not in this player's catalog."),
			*DefinitionId.ToString());
		return false;
	}

	if (Definition->UsesAmmoContainer())
	{
		UE_LOG(LogTemp, Warning, TEXT("Claim world item rejected ammo DefinitionId '%s'; use the fixed world ammo claim path."),
			*DefinitionId.ToString());
		return false;
	}

	const EItemEquipmentSlot EquipmentSlot = Definition->GetEquipmentSlot();
	const FName RequestedSlotId = GetSlotId(EquipmentSlot);
	if (bRequestAutoEquip && (!IsSupportedEquipmentSlot(EquipmentSlot) || RequestedSlotId == NAME_None
		|| GetEquippedInstanceId(EquipmentSlot) != NAME_None))
	{
		UE_LOG(LogTemp, Warning, TEXT("Claim world item rejected automatic equip for DefinitionId '%s': the compatible slot is unavailable."),
			*DefinitionId.ToString());
		return false;
	}

	FTestItemInstanceRecord NewRecord;
	NewRecord.DefinitionId = DefinitionId;
	NewRecord.InstanceId = GenerateUniqueInstanceId();
	NewRecord.Quantity = 1;
	NewRecord.UpgradeLevel = 0;

	if (!GameInstance->AddOwnedItemInstanceAndClaimRewardWithOptionalEmptySlot(NewRecord, PersistentId,
		bRequestAutoEquip ? RequestedSlotId : NAME_None, bOutAutoEquipped, PendingRewardId))
	{
		return false;
	}

	OwnedItemInstances.Add(NewRecord);
	if (bOutAutoEquipped)
	{
		UpdateLocalEquipmentSlot(RequestedSlotId, NewRecord.InstanceId);
	}
	OutInstanceId = NewRecord.InstanceId;
	return true;
}

bool UItemOwnershipComponent::TryClaimWorldAmmoPickup(FName PersistentId, FName DefinitionId, int32 Quantity,
	USoulslikeGameInstance* GameInstance, FName& OutAffectedInstanceId, FName PendingRewardId)
{
	OutAffectedInstanceId = NAME_None;
	BuildDefinitionCatalog();

	if (!GameInstance || PersistentId == NAME_None || Quantity <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Claim world ammo failed: GameInstance, PersistentId, or quantity is invalid."));
		return false;
	}

	const UItemDefinitionDataAsset* Definition = GetDefinition(DefinitionId);
	if (!Definition || !Definition->UsesAmmoContainer())
	{
		UE_LOG(LogTemp, Warning, TEXT("Claim world ammo rejected DefinitionId '%s': it is not an Ammo Container definition."),
			*DefinitionId.ToString());
		return false;
	}

	TArray<FTestItemInstanceSelection> ValidReserveInstances;
	GetValidReserveInstances(DefinitionId, ValidReserveInstances);
	if (!GameInstance->GrantAmmoReserveAndClaimReward(DefinitionId, Quantity, Definition->GetReserveAmmoStackLimit(),
		PersistentId, ValidReserveInstances, OutAffectedInstanceId, PendingRewardId))
	{
		return false;
	}

	if (!RestoreFromSave(GameInstance->GetCurrentSaveGame()))
	{
		// 耐久事务已提交；不能把世界 Actor 留在场上制造可重复领取的假象。
		UE_LOG(LogTemp, Error, TEXT("Fixed world ammo claim for '%s' committed, but the local ownership cache could not be restored."),
			*DefinitionId.ToString());
		return true;
	}

	BroadcastOwnedQuantity(DefinitionId);
	BroadcastLoadedAmmoQuantity(DefinitionId);
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

void UItemOwnershipComponent::GetLoadoutOptions(EItemEquipmentSlot EquipmentSlot,
                                                 TArray<FItemLoadoutOption>& OutOptions)
{
	OutOptions.Reset();
	BuildDefinitionCatalog();

	if (!IsSupportedEquipmentSlot(EquipmentSlot))
	{
		return;
	}

	for (const FTestItemInstanceRecord& ItemRecord : OwnedItemInstances)
	{
		if (ItemRecord.InstanceId == NAME_None || ItemRecord.Quantity <= 0)
		{
			continue;
		}

		const UItemDefinitionDataAsset* Definition = GetDefinition(ItemRecord.DefinitionId);
		if (!Definition || Definition->GetEquipmentSlot() != EquipmentSlot)
		{
			continue;
		}

		FItemLoadoutOption& Option = OutOptions.AddDefaulted_GetRef();
		Option.InstanceId = ItemRecord.InstanceId;
		Option.DisplayName = Definition->GetDisplayName();
	}
}

FString UItemOwnershipComponent::BuildDebugSummary() const
{
	FString Result = FString::Printf(TEXT("Runtime item ownership: %d instance(s), %d equipped slot(s), %d loaded ammo container(s)."),
		OwnedItemInstances.Num(), EquippedSlots.Num(), LoadedAmmoContainers.Num());

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

	TArray<FName> AmmoDefinitionIds;
	GetAmmoDefinitionIds(AmmoDefinitionIds);
	for (const FName DefinitionId : AmmoDefinitionIds)
	{
		Result += FString::Printf(TEXT("\n  Ammo Definition=%s Loaded=%d/%d Reserve=%d Total=%d"),
			*DefinitionId.ToString(), GetLoadedAmmoQuantity(DefinitionId), GetLoadedAmmoCapacity(DefinitionId),
			GetReserveAmmoQuantity(DefinitionId), GetTotalAmmoQuantity(DefinitionId));
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

bool UItemOwnershipComponent::ValidateLoadedAmmoContainer(const FTestAmmoContainerRecord& ContainerRecord,
	FString& OutFailureReason) const
{
	if (ContainerRecord.DefinitionId == NAME_None)
	{
		OutFailureReason = TEXT("DefinitionId is empty.");
		return false;
	}

	if (ContainerRecord.LoadedQuantity <= 0)
	{
		OutFailureReason = TEXT("LoadedQuantity must be positive; empty containers are not persisted.");
		return false;
	}

	const UItemDefinitionDataAsset* Definition = GetDefinition(ContainerRecord.DefinitionId);
	if (!Definition || !Definition->UsesAmmoContainer())
	{
		OutFailureReason = TEXT("Definition is missing or does not use an ammo container.");
		return false;
	}

	if (ContainerRecord.LoadedQuantity > Definition->GetLoadedAmmoCapacity())
	{
		OutFailureReason = TEXT("LoadedQuantity exceeds the definition capacity.");
		return false;
	}

	return true;
}

bool UItemOwnershipComponent::BuildAmmoRefillRequest(FName DefinitionId, FTestAmmoRefillRequest& OutRequest) const
{
	OutRequest = FTestAmmoRefillRequest{};
	const UItemDefinitionDataAsset* Definition = GetDefinition(DefinitionId);
	if (!Definition || !Definition->UsesAmmoContainer())
	{
		return false;
	}

	const int32 LoadedCapacity = Definition->GetLoadedAmmoCapacity();
	const int32 LoadedQuantity = GetLoadedAmmoQuantity(DefinitionId);
	if (LoadedCapacity <= LoadedQuantity || GetReserveAmmoQuantity(DefinitionId) <= 0)
	{
		return false;
	}

	OutRequest.DefinitionId = DefinitionId;
	OutRequest.LoadedCapacity = LoadedCapacity;
	OutRequest.ReserveStackLimit = Definition->GetReserveAmmoStackLimit();
	OutRequest.ExpectedLoadedQuantity = LoadedQuantity;
	if (const int32* SourceIndex = LoadedAmmoSourceIndices.Find(DefinitionId))
	{
		OutRequest.SourceContainerIndex = *SourceIndex;
	}

	GetValidReserveInstances(DefinitionId, OutRequest.ValidReserveInstances);
	return OutRequest.ValidReserveInstances.Num() > 0;
}

void UItemOwnershipComponent::GetAmmoDefinitionIds(TArray<FName>& OutDefinitionIds) const
{
	OutDefinitionIds.Reset();
	for (const TPair<FName, UItemDefinitionDataAsset*>& Pair : DefinitionsById)
	{
		if (Pair.Value && Pair.Value->UsesAmmoContainer())
		{
			OutDefinitionIds.Add(Pair.Key);
		}
	}

	OutDefinitionIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});
}

void UItemOwnershipComponent::GetValidReserveInstances(FName DefinitionId,
	TArray<FTestItemInstanceSelection>& OutSelections) const
{
	OutSelections.Reset();
	for (const FTestItemInstanceRecord& ItemRecord : OwnedItemInstances)
	{
		if (ItemRecord.DefinitionId == DefinitionId && ItemRecord.InstanceId != NAME_None && ItemRecord.Quantity > 0)
		{
			const int32* SourceIndex = OwnedItemSourceIndices.Find(ItemRecord.InstanceId);
			if (!SourceIndex)
			{
				continue;
			}

			FTestItemInstanceSelection& Selection = OutSelections.AddDefaulted_GetRef();
			Selection.InstanceId = ItemRecord.InstanceId;
			Selection.SourceItemIndex = *SourceIndex;
			Selection.ExpectedQuantity = ItemRecord.Quantity;
		}
	}
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

int32 UItemOwnershipComponent::GetOwnedQuantity(FName DefinitionId) const
{
	int64 TotalQuantity = 0;
	for (const FTestItemInstanceRecord& ItemRecord : OwnedItemInstances)
	{
		if (ItemRecord.DefinitionId == DefinitionId)
		{
			TotalQuantity += ItemRecord.Quantity;
		}
	}

	return static_cast<int32>(FMath::Clamp<int64>(TotalQuantity, 0, MAX_int32));
}

int32 UItemOwnershipComponent::GetReserveAmmoQuantity(FName DefinitionId) const
{
	return GetOwnedQuantity(DefinitionId);
}

int32 UItemOwnershipComponent::GetLoadedAmmoQuantity(FName DefinitionId) const
{
	if (const FTestAmmoContainerRecord* ContainerRecord = LoadedAmmoContainers.FindByPredicate(
		[DefinitionId](const FTestAmmoContainerRecord& Candidate)
		{
			return Candidate.DefinitionId == DefinitionId;
		}))
	{
		return ContainerRecord->LoadedQuantity;
	}

	return 0;
}

int32 UItemOwnershipComponent::GetTotalAmmoQuantity(FName DefinitionId) const
{
	return static_cast<int32>(FMath::Clamp<int64>(
		static_cast<int64>(GetReserveAmmoQuantity(DefinitionId)) + GetLoadedAmmoQuantity(DefinitionId), 0, MAX_int32));
}

int32 UItemOwnershipComponent::GetLoadedAmmoCapacity(FName DefinitionId) const
{
	const UItemDefinitionDataAsset* Definition = GetDefinition(DefinitionId);
	return Definition && Definition->UsesAmmoContainer() ? Definition->GetLoadedAmmoCapacity() : 0;
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

void UItemOwnershipComponent::BroadcastOwnedQuantity(FName DefinitionId)
{
	OnOwnedItemQuantityChanged.Broadcast(DefinitionId, GetOwnedQuantity(DefinitionId));
}

void UItemOwnershipComponent::BroadcastLoadedAmmoQuantity(FName DefinitionId)
{
	OnLoadedAmmoQuantityChanged.Broadcast(DefinitionId, GetLoadedAmmoQuantity(DefinitionId));
}

FName UItemOwnershipComponent::GenerateUniqueInstanceId() const
{
	return FName(*FString::Printf(TEXT("Item_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
}
