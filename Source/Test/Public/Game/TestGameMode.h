// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "TestGameMode.generated.h"

class ACharacterController;
class ACheckpointActor;
class AMyCharacter;
class UAttributeComponent;

/** 单人地图流程所有者：出生、资源恢复与地图重载，不持有 UMG 或具体世界进度规则。 */
UCLASS()
class TEST_API ATestGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ATestGameMode();

	virtual void BeginPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	void RequestUseCheckpoint(ACheckpointActor* Checkpoint, AMyCharacter* Player);
	bool RequestRestAtCheckpoint(ACheckpointActor* Checkpoint, AMyCharacter* Player);
	bool CanUseCheckpoint(const ACheckpointActor* Checkpoint, const AMyCharacter* Player) const;
	/** 复用敌人当前目标和外层 AI 状态判断玩家是否正被主动交战。 */
	bool IsPlayerEngagedByEnemy(const AMyCharacter* Player) const;
	void HandlePlayerDeath(AMyCharacter* Player);
	void RequestReturnToMainMenu(ACharacterController* RequestingController);

	FORCEINLINE bool IsTransitionInProgress() const { return bTransitionInProgress; }

private:
	ACheckpointActor* FindCheckpointById(FName PersistentId) const;
	FName GetCurrentGameplayMapName() const;
	void HandlePlayerSpawned(APlayerController* NewPlayer);
	void RestorePlayerFromSave(AMyCharacter* Player);
	void CapturePlayerGold();
	void StartGameplayReload();
	void FinishGameplayReload();

	UFUNCTION()
	void HandlePlayerGoldChanged(int32 NewGold);

	TWeakObjectPtr<AMyCharacter> ManagedPlayer;
	FTimerHandle DeathRespawnTimer;
	FTimerHandle MapTransitionTimer;
	bool bRestoringPlayerState = false;
	bool bTransitionInProgress = false;
	FName TransitionMapName = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "Game Flow", meta = (ToolTip = "死亡 Overlay 可见时长（秒）。"))
	float DeathOverlayDuration = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "Game Flow", meta = (ToolTip = "重载地图前的淡出时长（秒）。"))
	float MapFadeDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Game Flow", meta = (ToolTip = "进入 TestMap 后恢复到玩家镜头的混合时长（秒）。"))
	float PlayerCameraBlendDuration = 0.25f;
};
