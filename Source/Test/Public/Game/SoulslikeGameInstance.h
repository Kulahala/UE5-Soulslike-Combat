// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Save/TestSaveGame.h"
#include "SoulslikeGameInstance.generated.h"

/** 单槽存档、菜单转场上下文与耐久进度写入的唯一入口。 */
UCLASS()
class TEST_API USoulslikeGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	static const FString SaveSlotName;
	static constexpr int32 SaveUserIndex = 0;

	virtual void Init() override;

	UFUNCTION(BlueprintPure, Category = "Save")
	bool HasValidContinue();
	bool HasExistingSave() const;

	UFUNCTION(BlueprintCallable, Category = "Save")
	bool StartNewGame(FName GameplayMapName);

	UFUNCTION(BlueprintCallable, Category = "Save")
	bool ContinueGame();

	bool SaveNow();
	void UpdateGold(int32 NewGold);
	bool AddOwnedItemInstance(const FTestItemInstanceRecord& ItemRecord);
	bool SetEquippedItemSlot(FName SlotId, FName ItemInstanceId);
	bool GetSavedItemOwnership(TArray<FTestItemInstanceRecord>& OutItemInstances,
	                           TArray<FTestEquipmentSlotRecord>& OutEquippedSlots) const;
	void SetRespawnCheckpoint(FName GameplayMapName, FName CheckpointId);
	void PrepareGameplayTransition(FName GameplayMapName, FName CheckpointId);
	void InvalidateCurrentSave(const FString& Reason);
	void ReturnToMainMenu();

	void MarkShortcutOpened(FName PersistentId);
	void MarkRewardClaimed(FName PersistentId);
	void MarkEncounterCleared(FName PersistentId);
	void MarkBossCompleted(FName PersistentId);

	FORCEINLINE const UTestSaveGame* GetCurrentSaveGame() const { return CurrentSaveGame; }
	FORCEINLINE FName GetPendingCheckpointId() const { return PendingCheckpointId; }
	FORCEINLINE FName GetPendingGameplayMapName() const { return PendingGameplayMapName; }

	UFUNCTION(BlueprintPure, Category = "Save")
	FName GetLastCheckpointId() const;

private:
	bool LoadExistingSave();
	bool EnsureCurrentSaveLoaded();
	bool AddPersistentId(TSet<FName>& TargetSet, FName PersistentId, const TCHAR* Context);
	void OpenGameplayMap();

	UPROPERTY()
	UTestSaveGame* CurrentSaveGame = nullptr;

	FName PendingCheckpointId = NAME_None;
	FName PendingGameplayMapName = NAME_None;
	bool bAttemptedSaveLoad = false;

	UPROPERTY(EditDefaultsOnly, Category = "Maps", meta = (ToolTip = "主菜单地图名。资产路径由地图名解析，不保存到 SaveGame。"))
	FName MainMenuMapName = FName(TEXT("MainMenu"));

	UPROPERTY(EditDefaultsOnly, Category = "Maps", meta = (ToolTip = "新档初始复活火堆的稳定 PersistentId；首次进入地图仍从 PlayerStart 出生。"))
	FName DefaultStartCheckpointId = FName(TEXT("StartBonfire"));
};
