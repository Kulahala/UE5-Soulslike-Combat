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
