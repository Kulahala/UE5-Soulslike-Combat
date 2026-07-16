// Fill out your copyright notice in the Description page of Project Settings.

#include "Save/TestSaveGame.h"

void UTestSaveGame::InitializeNewSave(FName InitialMapName, FName InitialCheckpointId)
{
	SaveVersion = CurrentSaveVersion;
	bIsValid = true;
	MapName = InitialMapName;
	LastCheckpointId = InitialCheckpointId;
	ActivatedCheckpointIds.Reset();
	Gold = 0;
	ItemInstances.Reset();
	LoadedAmmoContainers.Reset();
	EquippedSlots.Reset();
	OpenedShortcutIds.Reset();
	ClaimedRewardIds.Reset();
	ClearedEncounterIds.Reset();
	CompletedBossIds.Reset();
}

bool UTestSaveGame::IsCompatible() const
{
	return SaveVersion == CurrentSaveVersion;
}

bool UTestSaveGame::IsPersistable() const
{
	return bIsValid && IsCompatible() && MapName != NAME_None;
}

bool UTestSaveGame::HasRespawnAnchor() const
{
	return IsPersistable() && LastCheckpointId != NAME_None && ActivatedCheckpointIds.Contains(LastCheckpointId);
}
