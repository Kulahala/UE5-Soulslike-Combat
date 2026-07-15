// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/SoulslikeGameInstance.h"

#include "Kismet/GameplayStatics.h"
#include "Save/TestSaveGame.h"

const FString USoulslikeGameInstance::SaveSlotName(TEXT("TestSaveSlot"));

void USoulslikeGameInstance::Init()
{
	Super::Init();
	LoadExistingSave();
}

bool USoulslikeGameInstance::HasValidContinue()
{
	return EnsureCurrentSaveLoaded() && CurrentSaveGame && CurrentSaveGame->IsUsable();
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

	CurrentSaveGame = NewObject<UTestSaveGame>(this);
	CurrentSaveGame->InitializeNewSave(GameplayMapName, DefaultStartCheckpointId);
	bAttemptedSaveLoad = true;
	// 新游戏从关卡 PlayerStart 开始；默认火堆只作为新档的首个复活锚点。
	PrepareGameplayTransition(GameplayMapName, NAME_None);

	if (!SaveNow())
	{
		return false;
	}

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
	if (!CurrentSaveGame || !CurrentSaveGame->IsUsable())
	{
		UE_LOG(LogTemp, Warning, TEXT("SaveNow skipped: current save is not usable."));
		return false;
	}

	if (!UGameplayStatics::SaveGameToSlot(CurrentSaveGame, SaveSlotName, SaveUserIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to save slot '%s'."), *SaveSlotName);
		return false;
	}

	return true;
}

void USoulslikeGameInstance::UpdateGold(int32 NewGold)
{
	if (!EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsUsable())
	{
		return;
	}

	CurrentSaveGame->Gold = FMath::Max(0, NewGold);
	SaveNow();
}

bool USoulslikeGameInstance::AddOwnedItemInstance(const FTestItemInstanceRecord& ItemRecord)
{
	if (!EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsUsable())
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

bool USoulslikeGameInstance::AddOwnedItemInstanceAndClaimReward(const FTestItemInstanceRecord& ItemRecord,
	                                                                FName RewardId)
{
	if (!EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsUsable())
	{
		UE_LOG(LogTemp, Warning, TEXT("AddOwnedItemInstanceAndClaimReward failed: no usable current save."));
		return false;
	}

	if (RewardId == NAME_None || ItemRecord.DefinitionId == NAME_None || ItemRecord.InstanceId == NAME_None
		|| ItemRecord.Quantity <= 0 || ItemRecord.UpgradeLevel < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddOwnedItemInstanceAndClaimReward rejected invalid reward or item record."));
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
		UE_LOG(LogTemp, Warning, TEXT("AddOwnedItemInstanceAndClaimReward rejected duplicate InstanceId '%s'."),
			*ItemRecord.InstanceId.ToString());
		return false;
	}

	const TArray<FTestItemInstanceRecord> PreviousItems = CurrentSaveGame->ItemInstances;
	const TSet<FName> PreviousClaimedRewards = CurrentSaveGame->ClaimedRewardIds;
	CurrentSaveGame->ItemInstances.Add(ItemRecord);
	CurrentSaveGame->ClaimedRewardIds.Add(RewardId);

	if (SaveNow())
	{
		return true;
	}

	CurrentSaveGame->ItemInstances = PreviousItems;
	CurrentSaveGame->ClaimedRewardIds = PreviousClaimedRewards;
	return false;
}

bool USoulslikeGameInstance::SetEquippedItemSlot(FName SlotId, FName ItemInstanceId)
{
	if (SlotId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetEquippedItemSlot rejected an empty SlotId."));
		return false;
	}

	if (!EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsUsable())
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

	if (!CurrentSaveGame || !CurrentSaveGame->IsUsable())
	{
		return false;
	}

	OutItemInstances = CurrentSaveGame->ItemInstances;
	OutEquippedSlots = CurrentSaveGame->EquippedSlots;
	return true;
}

void USoulslikeGameInstance::SetRespawnCheckpoint(FName GameplayMapName, FName CheckpointId)
{
	if (GameplayMapName == NAME_None || CheckpointId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetRespawnCheckpoint rejected an empty map or checkpoint ID."));
		return;
	}

	if (!EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsUsable())
	{
		CurrentSaveGame = NewObject<UTestSaveGame>(this);
		CurrentSaveGame->InitializeNewSave(GameplayMapName, CheckpointId);
		bAttemptedSaveLoad = true;
	}
	else
	{
		CurrentSaveGame->MapName = GameplayMapName;
		CurrentSaveGame->LastCheckpointId = CheckpointId;
	}

	PrepareGameplayTransition(GameplayMapName, CheckpointId);
	SaveNow();
}

void USoulslikeGameInstance::PrepareGameplayTransition(FName GameplayMapName, FName CheckpointId)
{
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
	if (PersistentId == NAME_None || !EnsureCurrentSaveLoaded() || !CurrentSaveGame || !CurrentSaveGame->IsUsable())
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

	if (!CurrentSaveGame->IsUsable())
	{
		UE_LOG(LogTemp, Warning, TEXT("Save slot '%s' is invalid, incompatible, or missing a respawn anchor. Continue is disabled."), *SaveSlotName);
		return false;
	}

	return true;
}

bool USoulslikeGameInstance::EnsureCurrentSaveLoaded()
{
	return CurrentSaveGame || (bAttemptedSaveLoad ? false : LoadExistingSave());
}

bool USoulslikeGameInstance::AddPersistentId(TSet<FName>& TargetSet, FName PersistentId, const TCHAR* Context)
{
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

void USoulslikeGameInstance::OpenGameplayMap()
{
	if (PendingGameplayMapName == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("OpenGameplayMap failed: no pending gameplay map."));
		return;
	}

	UGameplayStatics::OpenLevel(this, PendingGameplayMapName);
}
