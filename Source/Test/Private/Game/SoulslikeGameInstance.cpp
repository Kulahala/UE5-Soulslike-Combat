// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/SoulslikeGameInstance.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"
#include "Save/TestSaveGame.h"

const FString USoulslikeGameInstance::SaveSlotName(TEXT("TestSaveSlot"));

void USoulslikeGameInstance::Init()
{
	Super::Init();
	LoadExistingSave();
}

bool USoulslikeGameInstance::HasValidContinue()
{
	return EnsureCurrentSaveLoaded() && CurrentSaveGame && CurrentSaveGame->HasRespawnAnchor();
}

bool USoulslikeGameInstance::HasExistingSave() const
{
	return UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex);
}

bool USoulslikeGameInstance::StartNewGame(FName GameplayMapName)
{
	if (GameplayMapName == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartNewGame failed: GameplayMapName is not configured."));
		return false;
	}

	ClearItemClaimSaveFailureForDebug();
	ClearGoldClaimSaveFailureForDebug();
	ClearLoadedAmmoConsumeSaveFailureForDebug();
	ClearAmmoRefillSaveFailureForDebug();

	UTestSaveGame* PreviousSaveGame = CurrentSaveGame;
	const FName PreviousPendingCheckpointId = PendingCheckpointId;
	const FName PreviousPendingGameplayMapName = PendingGameplayMapName;
	const bool bPreviousAttemptedSaveLoad = bAttemptedSaveLoad;

	UTestSaveGame* NewSaveGame = NewObject<UTestSaveGame>(this);
	NewSaveGame->InitializeNewSave(GameplayMapName, NAME_None);

	CurrentSaveGame = NewSaveGame;
	bAttemptedSaveLoad = true;

	if (!SaveNow())
	{
		CurrentSaveGame = PreviousSaveGame;
		PendingCheckpointId = PreviousPendingCheckpointId;
		PendingGameplayMapName = PreviousPendingGameplayMapName;
		bAttemptedSaveLoad = bPreviousAttemptedSaveLoad;
		UE_LOG(LogTemp, Warning, TEXT("StartNewGame failed: the previous in-memory save was restored."));
		return false;
	}

	// 新游戏先建立可写进度记录；首次实际激活火堆前不提供 Continue 或重生锚点。
	PrepareGameplayTransition(GameplayMapName, NAME_None);
	OpenGameplayMap();
	return true;
}

bool USoulslikeGameInstance::ContinueGame()
{
	if (!HasValidContinue())
	{
		UE_LOG(LogTemp, Warning, TEXT("Continue is unavailable: save is missing, invalid, incompatible, or has no checkpoint."));
		return false;
	}

	PrepareGameplayTransition(CurrentSaveGame->MapName, CurrentSaveGame->LastCheckpointId);
	OpenGameplayMap();
	return true;
}

bool USoulslikeGameInstance::SaveNow()
{
	if (!CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		UE_LOG(LogTemp, Warning, TEXT("SaveNow skipped: current save is not persistable."));
		return false;
	}

	if (!UGameplayStatics::SaveGameToSlot(CurrentSaveGame, SaveSlotName, SaveUserIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to save slot '%s'."), *SaveSlotName);
		return false;
	}

	return true;
}

bool USoulslikeGameInstance::TryAddGold(int32 Amount, int32& OutNewGold)
{
	OutNewGold = 0;
	if (Amount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("TryAddGold rejected a non-positive amount: %d."), Amount);
		return false;
	}

	if (!EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		UE_LOG(LogTemp, Warning, TEXT("TryAddGold failed: no persistable current save."));
		return false;
	}

	const int64 CandidateGold = static_cast<int64>(CurrentSaveGame->Gold) + static_cast<int64>(Amount);
	if (CandidateGold > TNumericLimits<int32>::Max())
	{
		UE_LOG(LogTemp, Warning, TEXT("TryAddGold rejected an int32 overflow for amount %d."), Amount);
		return false;
	}

	const int32 PreviousGold = CurrentSaveGame->Gold;
	CurrentSaveGame->Gold = static_cast<int32>(CandidateGold);
	if (!ConsumeGoldClaimSaveFailureForDebug(Amount) && SaveNow())
	{
		OutNewGold = CurrentSaveGame->Gold;
		return true;
	}

	CurrentSaveGame->Gold = PreviousGold;
	return false;
}

bool USoulslikeGameInstance::AddOwnedItemInstance(const FTestItemInstanceRecord& ItemRecord)
{
	if (!EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		UE_LOG(LogTemp, Warning, TEXT("AddOwnedItemInstance failed: no usable current save."));
		return false;
	}

	if (ItemRecord.DefinitionId == NAME_None || ItemRecord.InstanceId == NAME_None || ItemRecord.Quantity <= 0 ||
		ItemRecord.UpgradeLevel < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddOwnedItemInstance rejected invalid item record."));
		return false;
	}

	const bool bDuplicateInstanceId = CurrentSaveGame->ItemInstances.ContainsByPredicate(
		[&ItemRecord](const FTestItemInstanceRecord& ExistingRecord)
		{
			return ExistingRecord.InstanceId == ItemRecord.InstanceId;
		});
	if (bDuplicateInstanceId)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddOwnedItemInstance rejected duplicate InstanceId '%s'."),
			*ItemRecord.InstanceId.ToString());
		return false;
	}

	CurrentSaveGame->ItemInstances.Add(ItemRecord);
	if (SaveNow())
	{
		return true;
	}

	CurrentSaveGame->ItemInstances.Pop();
	return false;
}

bool USoulslikeGameInstance::GrantAmmoReserve(FName DefinitionId, int32 Quantity, int32 ReserveStackLimit,
	const TArray<FTestItemInstanceSelection>& ValidReserveInstances, FName& OutAffectedInstanceId)
{
	return GrantAmmoReserveInternal(DefinitionId, Quantity, ReserveStackLimit, NAME_None,
		ValidReserveInstances, OutAffectedInstanceId);
}

bool USoulslikeGameInstance::GrantAmmoReserveAndClaimReward(FName DefinitionId, int32 Quantity,
	int32 ReserveStackLimit, FName RewardId,
	const TArray<FTestItemInstanceSelection>& ValidReserveInstances, FName& OutAffectedInstanceId)
{
	if (RewardId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("Fixed world ammo claim rejected an empty reward ID."));
		OutAffectedInstanceId = NAME_None;
		return false;
	}

	return GrantAmmoReserveInternal(DefinitionId, Quantity, ReserveStackLimit, RewardId,
		ValidReserveInstances, OutAffectedInstanceId);
}

bool USoulslikeGameInstance::GrantAmmoReserveInternal(FName DefinitionId, int32 Quantity, int32 ReserveStackLimit,
	FName RewardId, const TArray<FTestItemInstanceSelection>& ValidReserveInstances, FName& OutAffectedInstanceId)
{
	OutAffectedInstanceId = NAME_None;
	if (!EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		UE_LOG(LogTemp, Warning, TEXT("Grant ammo reserve failed: no usable current save."));
		return false;
	}

	if (RewardId != NAME_None && CurrentSaveGame->ClaimedRewardIds.Contains(RewardId))
	{
		UE_LOG(LogTemp, Warning, TEXT("Fixed world ammo claim rejected already claimed reward '%s'."),
			*RewardId.ToString());
		return false;
	}

	if (DefinitionId == NAME_None || Quantity <= 0 || ReserveStackLimit <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Grant ammo reserve rejected an empty definition, invalid quantity, or invalid stack limit."));
		return false;
	}

	TSet<FName> SeenInstanceIds;
	TSet<int32> SeenSourceIndices;
	for (const FTestItemInstanceSelection& Selection : ValidReserveInstances)
	{
		if (Selection.InstanceId == NAME_None || Selection.SourceItemIndex == INDEX_NONE || Selection.ExpectedQuantity <= 0
			|| SeenInstanceIds.Contains(Selection.InstanceId) || SeenSourceIndices.Contains(Selection.SourceItemIndex))
		{
			UE_LOG(LogTemp, Warning, TEXT("Grant ammo reserve rejected an invalid or duplicate validated reserve selection."));
			return false;
		}

		if (!CurrentSaveGame->ItemInstances.IsValidIndex(Selection.SourceItemIndex))
		{
			UE_LOG(LogTemp, Warning, TEXT("Grant ammo reserve rejected unavailable source index for InstanceId '%s'."),
				*Selection.InstanceId.ToString());
			return false;
		}

		const FTestItemInstanceRecord& ItemRecord = CurrentSaveGame->ItemInstances[Selection.SourceItemIndex];
		if (ItemRecord.InstanceId != Selection.InstanceId || ItemRecord.DefinitionId != DefinitionId
			|| ItemRecord.Quantity != Selection.ExpectedQuantity)
		{
			UE_LOG(LogTemp, Warning, TEXT("Grant ammo reserve rejected stale validated InstanceId '%s'."),
				*Selection.InstanceId.ToString());
			return false;
		}

		SeenInstanceIds.Add(Selection.InstanceId);
		SeenSourceIndices.Add(Selection.SourceItemIndex);
	}

	const TArray<FTestItemInstanceRecord> PreviousItems = CurrentSaveGame->ItemInstances;
	const TSet<FName> PreviousClaimedRewards = CurrentSaveGame->ClaimedRewardIds;
	int32 RemainingQuantity = Quantity;
	for (const FTestItemInstanceSelection& Selection : ValidReserveInstances)
	{
		FTestItemInstanceRecord& ItemRecord = CurrentSaveGame->ItemInstances[Selection.SourceItemIndex];
		if (ItemRecord.Quantity >= ReserveStackLimit)
		{
			continue;
		}

		const int32 AddedQuantity = FMath::Min(ReserveStackLimit - ItemRecord.Quantity, RemainingQuantity);
		ItemRecord.Quantity += AddedQuantity;
		RemainingQuantity -= AddedQuantity;
		if (OutAffectedInstanceId == NAME_None)
		{
			OutAffectedInstanceId = ItemRecord.InstanceId;
		}
		if (RemainingQuantity <= 0)
		{
			break;
		}
	}

	while (RemainingQuantity > 0)
	{
		FTestItemInstanceRecord NewRecord;
		NewRecord.DefinitionId = DefinitionId;
		NewRecord.InstanceId = GenerateUniqueItemInstanceId(CurrentSaveGame->ItemInstances);
		NewRecord.Quantity = FMath::Min(ReserveStackLimit, RemainingQuantity);
		NewRecord.UpgradeLevel = 0;
		CurrentSaveGame->ItemInstances.Add(NewRecord);
		RemainingQuantity -= NewRecord.Quantity;
		if (OutAffectedInstanceId == NAME_None)
		{
			OutAffectedInstanceId = NewRecord.InstanceId;
		}
	}

	if (RewardId != NAME_None)
	{
		CurrentSaveGame->ClaimedRewardIds.Add(RewardId);
	}

	const bool bCanSave = RewardId == NAME_None || !ConsumeItemClaimSaveFailureForDebug(RewardId);
	if (bCanSave && SaveNow())
	{
		return true;
	}

	CurrentSaveGame->ItemInstances = PreviousItems;
	CurrentSaveGame->ClaimedRewardIds = PreviousClaimedRewards;
	OutAffectedInstanceId = NAME_None;
	return false;
}

bool USoulslikeGameInstance::ConsumeLoadedAmmo(const FTestAmmoContainerSelection& Selection, int32 Quantity)
{
	if (!EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		UE_LOG(LogTemp, Warning, TEXT("Consume loaded ammo failed: no usable current save."));
		return false;
	}

	if (Selection.DefinitionId == NAME_None || Selection.SourceContainerIndex == INDEX_NONE
		|| Selection.ExpectedLoadedQuantity <= 0 || Quantity <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Consume loaded ammo rejected an invalid container selection or quantity."));
		return false;
	}

	if (!CurrentSaveGame->LoadedAmmoContainers.IsValidIndex(Selection.SourceContainerIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("Consume loaded ammo rejected an unavailable container index."));
		return false;
	}

	const FTestAmmoContainerRecord& ExistingContainer = CurrentSaveGame->LoadedAmmoContainers[Selection.SourceContainerIndex];
	if (ExistingContainer.DefinitionId != Selection.DefinitionId
		|| ExistingContainer.LoadedQuantity != Selection.ExpectedLoadedQuantity
		|| ExistingContainer.LoadedQuantity < Quantity)
	{
		UE_LOG(LogTemp, Warning, TEXT("Consume loaded ammo rejected a stale container selection for '%s'."),
			*Selection.DefinitionId.ToString());
		return false;
	}

	const TArray<FTestAmmoContainerRecord> PreviousContainers = CurrentSaveGame->LoadedAmmoContainers;
	FTestAmmoContainerRecord& MutableContainer = CurrentSaveGame->LoadedAmmoContainers[Selection.SourceContainerIndex];
	MutableContainer.LoadedQuantity -= Quantity;
	if (MutableContainer.LoadedQuantity <= 0)
	{
		CurrentSaveGame->LoadedAmmoContainers.RemoveAt(Selection.SourceContainerIndex);
	}

	if (!ConsumeLoadedAmmoSaveFailureForDebug(Selection.DefinitionId) && SaveNow())
	{
		return true;
	}

	CurrentSaveGame->LoadedAmmoContainers = PreviousContainers;
	return false;
}

bool USoulslikeGameInstance::AddOwnedItemInstanceAndClaimReward(const FTestItemInstanceRecord& ItemRecord,
	                                                                FName RewardId)
{
	bool bIgnoredAutoEquipped = false;
	return AddOwnedItemInstanceAndClaimRewardInternal(ItemRecord, RewardId, NAME_None, bIgnoredAutoEquipped);
}

bool USoulslikeGameInstance::AddOwnedItemInstanceAndClaimRewardWithOptionalEmptySlot(
	const FTestItemInstanceRecord& ItemRecord, FName RewardId, FName RequestedEmptySlotId, bool& bOutAutoEquipped)
{
	bOutAutoEquipped = false;
	if (RequestedEmptySlotId != NAME_None && !IsSupportedEquipmentSlotId(RequestedEmptySlotId))
	{
		UE_LOG(LogTemp, Warning, TEXT("Fixed world item claim rejected unsupported equipment slot '%s'."),
			*RequestedEmptySlotId.ToString());
		return false;
	}

	return AddOwnedItemInstanceAndClaimRewardInternal(ItemRecord, RewardId, RequestedEmptySlotId, bOutAutoEquipped);
}

bool USoulslikeGameInstance::AddOwnedItemInstanceAndClaimRewardInternal(
	const FTestItemInstanceRecord& ItemRecord, FName RewardId, FName RequestedEmptySlotId, bool& bOutAutoEquipped)
{
	bOutAutoEquipped = false;
	if (!EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		UE_LOG(LogTemp, Warning, TEXT("Fixed world item claim failed: no usable current save."));
		return false;
	}

	if (RequestedEmptySlotId != NAME_None && !IsSupportedEquipmentSlotId(RequestedEmptySlotId))
	{
		UE_LOG(LogTemp, Warning, TEXT("Fixed world item claim rejected unsupported equipment slot '%s'."),
			*RequestedEmptySlotId.ToString());
		return false;
	}

	if (RewardId == NAME_None || ItemRecord.DefinitionId == NAME_None || ItemRecord.InstanceId == NAME_None
		|| ItemRecord.Quantity <= 0 || ItemRecord.UpgradeLevel < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Fixed world item claim rejected invalid reward or item record."));
		return false;
	}

	if (CurrentSaveGame->ClaimedRewardIds.Contains(RewardId))
	{
		UE_LOG(LogTemp, Warning, TEXT("Reward '%s' has already been claimed."), *RewardId.ToString());
		return false;
	}

	const bool bDuplicateInstanceId = CurrentSaveGame->ItemInstances.ContainsByPredicate(
		[&ItemRecord](const FTestItemInstanceRecord& ExistingRecord)
		{
			return ExistingRecord.InstanceId == ItemRecord.InstanceId;
		});
	if (bDuplicateInstanceId)
	{
		UE_LOG(LogTemp, Warning, TEXT("Fixed world item claim rejected duplicate InstanceId '%s'."),
			*ItemRecord.InstanceId.ToString());
		return false;
	}

	const TArray<FTestItemInstanceRecord> PreviousItems = CurrentSaveGame->ItemInstances;
	const TSet<FName> PreviousClaimedRewards = CurrentSaveGame->ClaimedRewardIds;
	const TArray<FTestEquipmentSlotRecord> PreviousEquippedSlots = CurrentSaveGame->EquippedSlots;
	const bool bRequestedSlotIsEmpty = RequestedEmptySlotId != NAME_None
		&& !CurrentSaveGame->EquippedSlots.ContainsByPredicate([RequestedEmptySlotId](const FTestEquipmentSlotRecord& SlotRecord)
		{
			return SlotRecord.SlotId == RequestedEmptySlotId;
		});

	CurrentSaveGame->ItemInstances.Add(ItemRecord);
	CurrentSaveGame->ClaimedRewardIds.Add(RewardId);
	if (bRequestedSlotIsEmpty)
	{
		FTestEquipmentSlotRecord NewSlotRecord;
		NewSlotRecord.SlotId = RequestedEmptySlotId;
		NewSlotRecord.ItemInstanceId = ItemRecord.InstanceId;
		CurrentSaveGame->EquippedSlots.Add(NewSlotRecord);
	}

	if (!ConsumeItemClaimSaveFailureForDebug(RewardId) && SaveNow())
	{
		bOutAutoEquipped = bRequestedSlotIsEmpty;
		return true;
	}

	CurrentSaveGame->ItemInstances = PreviousItems;
	CurrentSaveGame->ClaimedRewardIds = PreviousClaimedRewards;
	CurrentSaveGame->EquippedSlots = PreviousEquippedSlots;
	return false;
}

bool USoulslikeGameInstance::SetEquippedItemSlot(FName SlotId, FName ItemInstanceId)
{
	if (SlotId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetEquippedItemSlot rejected an empty SlotId."));
		return false;
	}

	if (!EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		UE_LOG(LogTemp, Warning, TEXT("SetEquippedItemSlot failed: no usable current save."));
		return false;
	}

	if (ItemInstanceId != NAME_None)
	{
		const bool bOwnedInstanceExists = CurrentSaveGame->ItemInstances.ContainsByPredicate(
			[ItemInstanceId](const FTestItemInstanceRecord& ItemRecord)
			{
				return ItemRecord.InstanceId == ItemInstanceId;
			});
		if (!bOwnedInstanceExists)
		{
			UE_LOG(LogTemp, Warning, TEXT("SetEquippedItemSlot rejected unknown InstanceId '%s'."),
				*ItemInstanceId.ToString());
			return false;
		}
	}

	const int32 ExistingSlotIndex = CurrentSaveGame->EquippedSlots.IndexOfByPredicate(
		[SlotId](const FTestEquipmentSlotRecord& SlotRecord)
		{
			return SlotRecord.SlotId == SlotId;
		});

	if (ItemInstanceId == NAME_None && ExistingSlotIndex == INDEX_NONE)
	{
		return true;
	}

	if (ItemInstanceId != NAME_None)
	{
		const bool bAlreadyEquippedInOtherSlot = CurrentSaveGame->EquippedSlots.ContainsByPredicate(
			[SlotId, ItemInstanceId](const FTestEquipmentSlotRecord& SlotRecord)
			{
				return SlotRecord.SlotId != SlotId && SlotRecord.ItemInstanceId == ItemInstanceId;
			});
		if (bAlreadyEquippedInOtherSlot)
		{
			UE_LOG(LogTemp, Warning, TEXT("SetEquippedItemSlot rejected instance '%s' already assigned to another slot."),
				*ItemInstanceId.ToString());
			return false;
		}
	}

	TArray<FTestEquipmentSlotRecord> PreviousSlots = CurrentSaveGame->EquippedSlots;
	if (ItemInstanceId == NAME_None)
	{
		CurrentSaveGame->EquippedSlots.RemoveAt(ExistingSlotIndex);
	}
	else if (ExistingSlotIndex != INDEX_NONE)
	{
		CurrentSaveGame->EquippedSlots[ExistingSlotIndex].ItemInstanceId = ItemInstanceId;
	}
	else
	{
		FTestEquipmentSlotRecord NewSlotRecord;
		NewSlotRecord.SlotId = SlotId;
		NewSlotRecord.ItemInstanceId = ItemInstanceId;
		CurrentSaveGame->EquippedSlots.Add(NewSlotRecord);
	}

	if (SaveNow())
	{
		return true;
	}

	CurrentSaveGame->EquippedSlots = MoveTemp(PreviousSlots);
	return false;
}

bool USoulslikeGameInstance::GetSavedItemOwnership(TArray<FTestItemInstanceRecord>& OutItemInstances,
	                                                  TArray<FTestEquipmentSlotRecord>& OutEquippedSlots) const
{
	OutItemInstances.Reset();
	OutEquippedSlots.Reset();

	if (!CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		return false;
	}

	OutItemInstances = CurrentSaveGame->ItemInstances;
	OutEquippedSlots = CurrentSaveGame->EquippedSlots;
	return true;
}

bool USoulslikeGameInstance::GetSavedLoadedAmmoContainers(
	TArray<FTestAmmoContainerRecord>& OutLoadedAmmoContainers) const
{
	OutLoadedAmmoContainers.Reset();
	if (!CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		return false;
	}

	OutLoadedAmmoContainers = CurrentSaveGame->LoadedAmmoContainers;
	return true;
}

bool USoulslikeGameInstance::GetSavedClaimedRewardIds(TSet<FName>& OutClaimedRewardIds) const
{
	OutClaimedRewardIds.Reset();
	if (!CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		return false;
	}

	OutClaimedRewardIds = CurrentSaveGame->ClaimedRewardIds;
	return true;
}

bool USoulslikeGameInstance::ArmNextItemClaimSaveFailureForDebug()
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("Item claim save failure injection is unavailable in Shipping builds."));
	return false;
#else
	if (!EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		UE_LOG(LogTemp, Warning, TEXT("Item claim save failure injection failed: no usable current save."));
		return false;
	}

	bFailNextItemClaimSaveForDebug = true;
	return true;
#endif
}

bool USoulslikeGameInstance::ArmNextGoldClaimSaveFailureForDebug()
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("Gold claim save failure injection is unavailable in Shipping builds."));
	return false;
#else
	if (!EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		UE_LOG(LogTemp, Warning, TEXT("Gold claim save failure injection failed: no usable current save."));
		return false;
	}

	bFailNextGoldClaimSaveForDebug = true;
	return true;
#endif
}

bool USoulslikeGameInstance::ArmNextLoadedAmmoConsumeSaveFailureForDebug()
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("Loaded-ammo save failure injection is unavailable in Shipping builds."));
	return false;
#else
	if (!EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		UE_LOG(LogTemp, Warning, TEXT("Loaded-ammo save failure injection failed: no usable current save."));
		return false;
	}

	bFailNextLoadedAmmoConsumeSaveForDebug = true;
	return true;
#endif
}

bool USoulslikeGameInstance::ArmNextAmmoRefillSaveFailureForDebug()
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("Ammo-refill save failure injection is unavailable in Shipping builds."));
	return false;
#else
	if (!EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		UE_LOG(LogTemp, Warning, TEXT("Ammo-refill save failure injection failed: no usable current save."));
		return false;
	}

	bFailNextAmmoRefillSaveForDebug = true;
	return true;
#endif
}

bool USoulslikeGameInstance::ActivateCheckpointAndSetRespawn(FName GameplayMapName, FName CheckpointId)
{
	const TArray<FTestAmmoRefillRequest> NoRefillRequests;
	return ActivateCheckpointAndRefillAmmo(GameplayMapName, CheckpointId, NoRefillRequests);
}

bool USoulslikeGameInstance::ActivateCheckpointAndRefillAmmo(FName GameplayMapName, FName CheckpointId,
	const TArray<FTestAmmoRefillRequest>& RefillRequests)
{
	if (GameplayMapName == NAME_None || CheckpointId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("Activate checkpoint and refill ammo rejected an empty map or checkpoint ID."));
		return false;
	}

	if (!EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		UE_LOG(LogTemp, Warning, TEXT("Activate checkpoint and refill ammo failed: no persistable current save."));
		return false;
	}

	const FName PreviousMapName = CurrentSaveGame->MapName;
	const FName PreviousCheckpointId = CurrentSaveGame->LastCheckpointId;
	const TSet<FName> PreviousActivatedCheckpointIds = CurrentSaveGame->ActivatedCheckpointIds;
	const TArray<FTestItemInstanceRecord> PreviousItems = CurrentSaveGame->ItemInstances;
	const TArray<FTestAmmoContainerRecord> PreviousLoadedAmmoContainers = CurrentSaveGame->LoadedAmmoContainers;
	int32 TransferredAmmoQuantity = 0;
	FString RefillFailureReason;
	if (!ApplyAmmoRefillRequests(CurrentSaveGame->ItemInstances, CurrentSaveGame->LoadedAmmoContainers,
		RefillRequests, TransferredAmmoQuantity, RefillFailureReason))
	{
		CurrentSaveGame->ItemInstances = PreviousItems;
		CurrentSaveGame->LoadedAmmoContainers = PreviousLoadedAmmoContainers;
		UE_LOG(LogTemp, Warning, TEXT("Activate checkpoint and refill ammo rejected: %s"), *RefillFailureReason);
		return false;
	}

	CurrentSaveGame->MapName = GameplayMapName;
	CurrentSaveGame->LastCheckpointId = CheckpointId;
	CurrentSaveGame->ActivatedCheckpointIds.Add(CheckpointId);

	const bool bInjectedFailure = TransferredAmmoQuantity > 0 && ConsumeAmmoRefillSaveFailureForDebug();
	if (!bInjectedFailure && SaveNow())
	{
		PrepareGameplayTransition(GameplayMapName, CheckpointId);
		return true;
	}

	CurrentSaveGame->MapName = PreviousMapName;
	CurrentSaveGame->LastCheckpointId = PreviousCheckpointId;
	CurrentSaveGame->ActivatedCheckpointIds = PreviousActivatedCheckpointIds;
	CurrentSaveGame->ItemInstances = PreviousItems;
	CurrentSaveGame->LoadedAmmoContainers = PreviousLoadedAmmoContainers;
	return false;
}

bool USoulslikeGameInstance::VerifyAmmoRefillFixture(const FTestAmmoRefillRequest& RefillRequest)

{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("Ammo refill fixture verification is unavailable in Shipping builds."));
	return false;
#else
	if (!EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		UE_LOG(LogTemp, Warning, TEXT("Ammo refill fixture verification failed: no usable current save."));
		return false;
	}

	if (RefillRequest.DefinitionId == NAME_None || RefillRequest.ValidReserveInstances.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Ammo refill fixture verification requires a transferable validated reserve request."));
		return false;
	}

	TArray<FTestItemInstanceRecord> FixtureItems = CurrentSaveGame->ItemInstances;
	FTestItemInstanceRecord InvalidFixtureRecord;
	InvalidFixtureRecord.DefinitionId = RefillRequest.DefinitionId;
	InvalidFixtureRecord.InstanceId = NAME_None;
	InvalidFixtureRecord.Quantity = 777;
	InvalidFixtureRecord.UpgradeLevel = 0;
	FixtureItems.Insert(InvalidFixtureRecord, 0);
	TArray<FTestAmmoContainerRecord> FixtureContainers = CurrentSaveGame->LoadedAmmoContainers;

	FTestAmmoRefillRequest FixtureRequest = RefillRequest;
	TSet<FName> ValidInstanceIdSet;
	for (FTestItemInstanceSelection& Selection : FixtureRequest.ValidReserveInstances)
	{
		ValidInstanceIdSet.Add(Selection.InstanceId);
		++Selection.SourceItemIndex;
	}
	auto SumValidatedQuantity = [&ValidInstanceIdSet](const TArray<FTestItemInstanceRecord>& ItemRecords)
	{
		int64 TotalQuantity = 0;
		for (const FTestItemInstanceRecord& ItemRecord : ItemRecords)
		{
			if (ValidInstanceIdSet.Contains(ItemRecord.InstanceId))
			{
				TotalQuantity += ItemRecord.Quantity;
			}
		}
		return TotalQuantity;
	};

	const int64 BeforeValidatedQuantity = SumValidatedQuantity(FixtureItems);
	int32 TransferredAmmoQuantity = 0;
	FString FailureReason;
	TArray<FTestAmmoRefillRequest> FixtureRequests;
	FixtureRequests.Add(FixtureRequest);
	if (!ApplyAmmoRefillRequests(FixtureItems, FixtureContainers, FixtureRequests, TransferredAmmoQuantity, FailureReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("Ammo refill fixture verification failed to apply the copied transaction: %s"),
			*FailureReason);
		return false;
	}

	const int64 AfterValidatedQuantity = SumValidatedQuantity(FixtureItems);
	if (!FixtureItems.IsValidIndex(0) || FixtureItems[0].InstanceId != NAME_None || FixtureItems[0].Quantity != 777
		|| TransferredAmmoQuantity <= 0 || BeforeValidatedQuantity - AfterValidatedQuantity != TransferredAmmoQuantity)
	{
		UE_LOG(LogTemp, Warning, TEXT("Ammo refill fixture verification failed: invalid raw record or validated quantity changed unexpectedly."));
		return false;
	}

	UE_LOG(LogTemp, Display, TEXT("Ammo refill fixture verified: invalid raw record was untouched; %d validated '%s' ammo transferred in memory only."),
		TransferredAmmoQuantity, *RefillRequest.DefinitionId.ToString());
	return true;
#endif
}

bool USoulslikeGameInstance::HasActivatedCheckpoint(FName CheckpointId)
{
	return CheckpointId != NAME_None && EnsureCurrentSaveLoaded() && CurrentSaveGame
		&& CurrentSaveGame->IsPersistable() && CurrentSaveGame->ActivatedCheckpointIds.Contains(CheckpointId);
}

void USoulslikeGameInstance::PrepareGameplayTransition(FName GameplayMapName, FName CheckpointId)
{
	ClearItemClaimSaveFailureForDebug();
	ClearGoldClaimSaveFailureForDebug();
	ClearLoadedAmmoConsumeSaveFailureForDebug();
	ClearAmmoRefillSaveFailureForDebug();
	PendingGameplayMapName = GameplayMapName;
	PendingCheckpointId = CheckpointId;

	UE_LOG(LogTemp, Display, TEXT("Prepared gameplay transition: map='%s', checkpoint='%s'."),
		*PendingGameplayMapName.ToString(), *PendingCheckpointId.ToString());
}

void USoulslikeGameInstance::InvalidateCurrentSave(const FString& Reason)
{
	if (!CurrentSaveGame)
	{
		return;
	}

	CurrentSaveGame->bIsValid = false;
	if (!UGameplayStatics::SaveGameToSlot(CurrentSaveGame, SaveSlotName, SaveUserIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to persist invalid save state for slot '%s'."), *SaveSlotName);
	}

	UE_LOG(LogTemp, Warning, TEXT("Save slot '%s' was invalidated: %s"), *SaveSlotName, *Reason);
}

void USoulslikeGameInstance::ReturnToMainMenu()
{
	ClearItemClaimSaveFailureForDebug();
	ClearGoldClaimSaveFailureForDebug();
	ClearLoadedAmmoConsumeSaveFailureForDebug();
	ClearAmmoRefillSaveFailureForDebug();
	PendingGameplayMapName = NAME_None;
	PendingCheckpointId = NAME_None;
	UGameplayStatics::OpenLevel(this, MainMenuMapName);
}

void USoulslikeGameInstance::MarkShortcutOpened(FName PersistentId)
{
	if (EnsureCurrentSaveLoaded() && CurrentSaveGame)
	{
		AddPersistentId(CurrentSaveGame->OpenedShortcutIds, PersistentId, TEXT("shortcut"));
	}
}

void USoulslikeGameInstance::MarkRewardClaimed(FName PersistentId)
{
	if (EnsureCurrentSaveLoaded() && CurrentSaveGame)
	{
		AddPersistentId(CurrentSaveGame->ClaimedRewardIds, PersistentId, TEXT("reward"));
	}
}

bool USoulslikeGameInstance::HasClaimedReward(FName PersistentId)
{
	if (PersistentId == NAME_None || !EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		return false;
	}

	return CurrentSaveGame->ClaimedRewardIds.Contains(PersistentId);
}

void USoulslikeGameInstance::MarkEncounterCleared(FName PersistentId)
{
	if (EnsureCurrentSaveLoaded() && CurrentSaveGame)
	{
		AddPersistentId(CurrentSaveGame->ClearedEncounterIds, PersistentId, TEXT("encounter"));
	}
}

void USoulslikeGameInstance::MarkBossCompleted(FName PersistentId)
{
	if (EnsureCurrentSaveLoaded() && CurrentSaveGame)
	{
		AddPersistentId(CurrentSaveGame->CompletedBossIds, PersistentId, TEXT("Boss"));
	}
}

FName USoulslikeGameInstance::GetLastCheckpointId() const
{
	return CurrentSaveGame ? CurrentSaveGame->LastCheckpointId : NAME_None;
}

bool USoulslikeGameInstance::LoadExistingSave()
{
	bAttemptedSaveLoad = true;
	CurrentSaveGame = nullptr;

	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex))
	{
		return false;
	}

	USaveGame* LoadedSave = UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex);
	CurrentSaveGame = Cast<UTestSaveGame>(LoadedSave);
	if (!CurrentSaveGame)
	{
		UE_LOG(LogTemp, Warning, TEXT("Save slot '%s' could not be loaded as UTestSaveGame. Keeping the file and disabling Continue."), *SaveSlotName);
		return false;
	}

	if (!CurrentSaveGame->IsPersistable())
	{
		UE_LOG(LogTemp, Warning, TEXT("Save slot '%s' is invalid or incompatible. Continue is disabled."), *SaveSlotName);
		return false;
	}

	if (!CurrentSaveGame->HasRespawnAnchor())
	{
		UE_LOG(LogTemp, Display, TEXT("Save slot '%s' has persistent progress but no activated checkpoint. Continue is disabled."), *SaveSlotName);
	}

	return true;
}

bool USoulslikeGameInstance::EnsureCurrentSaveLoaded()
{
	return CurrentSaveGame || (bAttemptedSaveLoad ? false : LoadExistingSave());
}

bool USoulslikeGameInstance::AddPersistentId(TSet<FName>& TargetSet, FName PersistentId, const TCHAR* Context)
{
	if (!CurrentSaveGame || !CurrentSaveGame->IsPersistable())
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot persist %s: no persistable current save."), Context);
		return false;
	}

	if (PersistentId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot persist an empty %s ID."), Context);
		return false;
	}

	if (TargetSet.Contains(PersistentId))
	{
		return false;
	}

	TargetSet.Add(PersistentId);
	SaveNow();
	return true;
}

bool USoulslikeGameInstance::ApplyAmmoRefillRequests(TArray<FTestItemInstanceRecord>& ItemInstances,
	TArray<FTestAmmoContainerRecord>& LoadedAmmoContainers, const TArray<FTestAmmoRefillRequest>& RefillRequests,
	int32& OutTransferredQuantity, FString& OutFailureReason) const
{
	OutTransferredQuantity = 0;
	OutFailureReason.Reset();
	TSet<FName> SeenDefinitionIds;
	for (const FTestAmmoRefillRequest& RefillRequest : RefillRequests)
	{
		if (RefillRequest.DefinitionId == NAME_None || RefillRequest.LoadedCapacity <= 0
			|| RefillRequest.ReserveStackLimit <= 0 || RefillRequest.ValidReserveInstances.IsEmpty())
		{
			OutFailureReason = TEXT("An ammo refill request is incomplete.");
			return false;
		}

		if (SeenDefinitionIds.Contains(RefillRequest.DefinitionId))
		{
			OutFailureReason = FString::Printf(TEXT("Duplicate ammo refill definition '%s'."),
				*RefillRequest.DefinitionId.ToString());
			return false;
		}
		SeenDefinitionIds.Add(RefillRequest.DefinitionId);

		if (RefillRequest.SourceContainerIndex != INDEX_NONE)
		{
			if (!LoadedAmmoContainers.IsValidIndex(RefillRequest.SourceContainerIndex))
			{
				OutFailureReason = FString::Printf(TEXT("Loaded-ammo source index is unavailable for '%s'."),
					*RefillRequest.DefinitionId.ToString());
				return false;
			}

			const FTestAmmoContainerRecord& ContainerRecord = LoadedAmmoContainers[RefillRequest.SourceContainerIndex];
			if (ContainerRecord.DefinitionId != RefillRequest.DefinitionId
				|| ContainerRecord.LoadedQuantity != RefillRequest.ExpectedLoadedQuantity
				|| ContainerRecord.LoadedQuantity <= 0 || ContainerRecord.LoadedQuantity >= RefillRequest.LoadedCapacity)
			{
				OutFailureReason = FString::Printf(TEXT("Loaded-ammo source is stale or invalid for '%s'."),
					*RefillRequest.DefinitionId.ToString());
				return false;
			}
		}
		else if (RefillRequest.ExpectedLoadedQuantity != 0)
		{
			OutFailureReason = FString::Printf(TEXT("Loaded-ammo request for '%s' has no source but a nonzero expected quantity."),
				*RefillRequest.DefinitionId.ToString());
			return false;
		}

		TSet<FName> SeenReserveInstanceIds;
		TSet<int32> SeenReserveSourceIndices;
		for (const FTestItemInstanceSelection& Selection : RefillRequest.ValidReserveInstances)
		{
			if (Selection.InstanceId == NAME_None || Selection.SourceItemIndex == INDEX_NONE
				|| Selection.ExpectedQuantity <= 0 || SeenReserveInstanceIds.Contains(Selection.InstanceId)
				|| SeenReserveSourceIndices.Contains(Selection.SourceItemIndex))
			{
				OutFailureReason = TEXT("An ammo refill request contains an invalid or duplicate reserve selection.");
				return false;
			}

			if (!ItemInstances.IsValidIndex(Selection.SourceItemIndex))
			{
				OutFailureReason = FString::Printf(TEXT("Validated reserve source index is unavailable for InstanceId '%s'."),
					*Selection.InstanceId.ToString());
				return false;
			}

			const FTestItemInstanceRecord& ItemRecord = ItemInstances[Selection.SourceItemIndex];
			if (ItemRecord.InstanceId != Selection.InstanceId || ItemRecord.DefinitionId != RefillRequest.DefinitionId
				|| ItemRecord.Quantity != Selection.ExpectedQuantity)
			{
				OutFailureReason = FString::Printf(TEXT("Validated reserve InstanceId '%s' is stale."),
					*Selection.InstanceId.ToString());
				return false;
			}

			SeenReserveInstanceIds.Add(Selection.InstanceId);
			SeenReserveSourceIndices.Add(Selection.SourceItemIndex);
		}
	}

	TSet<int32> EmptySelectedItemIndices;
	for (const FTestAmmoRefillRequest& RefillRequest : RefillRequests)
	{
		const int32 CurrentLoadedQuantity = RefillRequest.SourceContainerIndex != INDEX_NONE
			? LoadedAmmoContainers[RefillRequest.SourceContainerIndex].LoadedQuantity
			: 0;
		int32 RemainingToLoad = RefillRequest.LoadedCapacity - CurrentLoadedQuantity;
		if (RemainingToLoad <= 0)
		{
			continue;
		}

		const int32 TargetLoadedQuantity = RemainingToLoad;
		for (const FTestItemInstanceSelection& Selection : RefillRequest.ValidReserveInstances)
		{
			if (RemainingToLoad <= 0)
			{
				break;
			}

			FTestItemInstanceRecord& ItemRecord = ItemInstances[Selection.SourceItemIndex];
			const int32 ConsumedQuantity = FMath::Min(ItemRecord.Quantity, RemainingToLoad);
			ItemRecord.Quantity -= ConsumedQuantity;
			RemainingToLoad -= ConsumedQuantity;
			if (ItemRecord.Quantity <= 0)
			{
				EmptySelectedItemIndices.Add(Selection.SourceItemIndex);
			}
		}

		const int32 TransferredQuantity = TargetLoadedQuantity - RemainingToLoad;
		if (TransferredQuantity <= 0)
		{
			continue;
		}

		if (RefillRequest.SourceContainerIndex != INDEX_NONE)
		{
			LoadedAmmoContainers[RefillRequest.SourceContainerIndex].LoadedQuantity += TransferredQuantity;
		}
		else
		{
			FTestAmmoContainerRecord NewContainer;
			NewContainer.DefinitionId = RefillRequest.DefinitionId;
			NewContainer.LoadedQuantity = TransferredQuantity;
			LoadedAmmoContainers.Add(NewContainer);
		}

		OutTransferredQuantity += TransferredQuantity;
	}

	TArray<int32> SortedEmptyItemIndices;
	for (const int32 ItemIndex : EmptySelectedItemIndices)
	{
		SortedEmptyItemIndices.Add(ItemIndex);
	}
	SortedEmptyItemIndices.Sort([](const int32 Left, const int32 Right)
	{
		return Left > Right;
	});
	for (const int32 ItemIndex : SortedEmptyItemIndices)
	{
		if (ItemInstances.IsValidIndex(ItemIndex) && ItemInstances[ItemIndex].Quantity <= 0)
		{
			ItemInstances.RemoveAt(ItemIndex);
		}
	}

	return true;
}

int32 USoulslikeGameInstance::FindItemInstanceIndex(const TArray<FTestItemInstanceRecord>& ItemInstances,
	FName InstanceId)
{
	return ItemInstances.IndexOfByPredicate([InstanceId](const FTestItemInstanceRecord& ItemRecord)
	{
		return ItemRecord.InstanceId == InstanceId;
	});
}

FName USoulslikeGameInstance::GenerateUniqueItemInstanceId(const TArray<FTestItemInstanceRecord>& ItemInstances)
{
	for (;;)
	{
		const FName CandidateInstanceId = FName(*FString::Printf(TEXT("Item_%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		if (FindItemInstanceIndex(ItemInstances, CandidateInstanceId) == INDEX_NONE)
		{
			return CandidateInstanceId;
		}
	}
}

bool USoulslikeGameInstance::IsSupportedEquipmentSlotId(FName SlotId)
{
	return SlotId == FName(TEXT("MainHand")) || SlotId == FName(TEXT("OffHand"));
}

bool USoulslikeGameInstance::ConsumeItemClaimSaveFailureForDebug(FName RewardId)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (!bFailNextItemClaimSaveForDebug)
	{
		return false;
	}

	bFailNextItemClaimSaveForDebug = false;
	UE_LOG(LogTemp, Warning, TEXT("Injected item claim save failure for reward '%s'."), *RewardId.ToString());
	return true;
#endif
}

bool USoulslikeGameInstance::ConsumeGoldClaimSaveFailureForDebug(int32 Amount)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (!bFailNextGoldClaimSaveForDebug)
	{
		return false;
	}

	bFailNextGoldClaimSaveForDebug = false;
	UE_LOG(LogTemp, Warning, TEXT("Injected gold claim save failure for amount %d."), Amount);
	return true;
#endif
}

bool USoulslikeGameInstance::ConsumeLoadedAmmoSaveFailureForDebug(FName DefinitionId)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (!bFailNextLoadedAmmoConsumeSaveForDebug)
	{
		return false;
	}

	bFailNextLoadedAmmoConsumeSaveForDebug = false;
	UE_LOG(LogTemp, Warning, TEXT("Injected loaded-ammo save failure for DefinitionId '%s'."), *DefinitionId.ToString());
	return true;
#endif
}

bool USoulslikeGameInstance::ConsumeAmmoRefillSaveFailureForDebug()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (!bFailNextAmmoRefillSaveForDebug)
	{
		return false;
	}

	bFailNextAmmoRefillSaveForDebug = false;
	UE_LOG(LogTemp, Warning, TEXT("Injected ammo-refill save failure."));
	return true;
#endif
}

void USoulslikeGameInstance::ClearItemClaimSaveFailureForDebug()
{
	bFailNextItemClaimSaveForDebug = false;
}

void USoulslikeGameInstance::ClearGoldClaimSaveFailureForDebug()
{
	bFailNextGoldClaimSaveForDebug = false;
}

void USoulslikeGameInstance::ClearLoadedAmmoConsumeSaveFailureForDebug()
{
	bFailNextLoadedAmmoConsumeSaveForDebug = false;
}

void USoulslikeGameInstance::ClearAmmoRefillSaveFailureForDebug()
{
	bFailNextAmmoRefillSaveForDebug = false;
}

void USoulslikeGameInstance::OpenGameplayMap()
{
	if (PendingGameplayMapName == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("OpenGameplayMap failed: no pending gameplay map."));
		return;
	}

	UGameplayStatics::OpenLevel(this, PendingGameplayMapName);
}
