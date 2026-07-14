// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TestSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FTestItemInstanceRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Item")
	FName DefinitionId = NAME_None;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Item")
	FName InstanceId = NAME_None;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Item")
	int32 Quantity = 1;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Item")
	int32 UpgradeLevel = 0;
};

USTRUCT(BlueprintType)
struct FTestEquipmentSlotRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Equipment")
	FName SlotId = NAME_None;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Equipment")
	FName ItemInstanceId = NAME_None;
};

/**
 * 单槽持久化数据。只保存稳定 ID 和纯数据，不保存运行时 Actor、Widget 或临时战斗状态。
 */
UCLASS()
class TEST_API UTestSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSaveVersion = 1;

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
	int32 SaveVersion = CurrentSaveVersion;

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
	bool bIsValid = false;

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
	FName MapName = NAME_None;

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
	FName LastCheckpointId = NAME_None;

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
	int32 Gold = 0;

	// TODO-03 开始拥有这些字段的实际写入与装备恢复。
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
	TArray<FTestItemInstanceRecord> ItemInstances;

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
	TArray<FTestEquipmentSlotRecord> EquippedSlots;

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
	TSet<FName> OpenedShortcutIds;

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
	TSet<FName> ClaimedRewardIds;

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
	TSet<FName> ClearedEncounterIds;

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
	TSet<FName> CompletedBossIds;

	void InitializeNewSave(FName InitialMapName, FName InitialCheckpointId);
	bool IsCompatible() const;
	bool IsUsable() const;
};
