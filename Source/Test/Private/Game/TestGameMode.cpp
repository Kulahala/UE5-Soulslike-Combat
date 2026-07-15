// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/TestGameMode.h"

#include "AttributeComponent/AttributeComponent.h"
#include "Character/Controller/CharacterController.h"
#include "Character/MyCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "Enemy/Enemy.h"
#include "EngineUtils.h"
#include "Game/SoulslikeGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Save/TestSaveGame.h"
#include "World/CheckpointActor.h"

ATestGameMode::ATestGameMode()
{
	PlayerControllerClass = ACharacterController::StaticClass();
}

void ATestGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 及早暴露关卡作者遗漏的起始火堆 ID，避免新游戏进入错误的默认 PlayerStart。
	if (USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>())
	{
		const FName PendingCheckpointId = GameInstance->GetPendingCheckpointId();
		if (PendingCheckpointId != NAME_None && !FindCheckpointById(PendingCheckpointId))
		{
			UE_LOG(LogTemp, Warning, TEXT("Pending checkpoint '%s' was not found in map '%s'."),
				*PendingCheckpointId.ToString(), *GetCurrentGameplayMapName().ToString());
			GameInstance->InvalidateCurrentSave(FString::Printf(TEXT("checkpoint '%s' is not present in map '%s'"),
				*PendingCheckpointId.ToString(), *GetCurrentGameplayMapName().ToString()));
		}
	}
}

AActor* ATestGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>())
	{
		if (ACheckpointActor* Checkpoint = FindCheckpointById(GameInstance->GetPendingCheckpointId()))
		{
			return Checkpoint;
		}
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

void ATestGameMode::RestartPlayer(AController* NewPlayer)
{
	if (USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>())
	{
		const FName PendingCheckpointId = GameInstance->GetPendingCheckpointId();
		UE_LOG(LogTemp, Display, TEXT("RestartPlayer in '%s': pending checkpoint='%s'."),
			*GetCurrentGameplayMapName().ToString(), *PendingCheckpointId.ToString());

		if (ACheckpointActor* Checkpoint = FindCheckpointById(PendingCheckpointId))
		{
			RestartPlayerAtTransform(NewPlayer, Checkpoint->GetSpawnTransform());
			if (NewPlayer && NewPlayer->GetPawn())
			{
				HandlePlayerSpawned(Cast<APlayerController>(NewPlayer));
				return;
			}

			UE_LOG(LogTemp, Warning,
				TEXT("Failed to spawn player at checkpoint '%s' (%s); falling back to PlayerStart."),
				*Checkpoint->GetPersistentId().ToString(), *Checkpoint->GetSpawnTransform().GetLocation().ToString());
		}
		else if (PendingCheckpointId != NAME_None)
		{
			UE_LOG(LogTemp, Warning, TEXT("Checkpoint '%s' was not found in map '%s'; falling back to PlayerStart."),
				*PendingCheckpointId.ToString(), *GetCurrentGameplayMapName().ToString());
		}
	}

	Super::RestartPlayer(NewPlayer);
	HandlePlayerSpawned(Cast<APlayerController>(NewPlayer));
}

void ATestGameMode::HandlePlayerSpawned(APlayerController* NewPlayer)
{
	AMyCharacter* Player = NewPlayer ? Cast<AMyCharacter>(NewPlayer->GetPawn()) : nullptr;
	if (!Player)
	{
		UE_LOG(LogTemp, Warning, TEXT("TestGameMode expected AMyCharacter after RestartPlayer completed."));
		return;
	}

	if (ManagedPlayer.IsValid())
	{
		if (UAttributeComponent* PreviousAttributes = ManagedPlayer->GetAttributes())
		{
			PreviousAttributes->OnGoldChanged.RemoveDynamic(this, &ATestGameMode::HandlePlayerGoldChanged);
		}
	}

	ManagedPlayer = Player;
	RestorePlayerFromSave(Player);

	if (UAttributeComponent* Attributes = Player->GetAttributes())
	{
		Attributes->OnGoldChanged.AddDynamic(this, &ATestGameMode::HandlePlayerGoldChanged);
	}

	if (USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>())
	{
		if (GameInstance->GetPendingGameplayMapName() == GetCurrentGameplayMapName())
		{
			NewPlayer->SetViewTargetWithBlend(Player, PlayerCameraBlendDuration);
			if (NewPlayer->PlayerCameraManager)
			{
				NewPlayer->PlayerCameraManager->StartCameraFade(1.f, 0.f, MapFadeDuration, FLinearColor::Black, false, false);
			}
		}
	}
}

void ATestGameMode::RequestUseCheckpoint(ACheckpointActor* Checkpoint, AMyCharacter* Player)
{
	if (!CanUseCheckpoint(Checkpoint, Player))
	{
		return;
	}

	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Checkpoint use failed: USoulslikeGameInstance is not active."));
		return;
	}

	if (!GameInstance->HasActivatedCheckpoint(Checkpoint->GetPersistentId()))
	{
		RequestRestAtCheckpoint(Checkpoint, Player);
		return;
	}

	ACharacterController* Controller = Cast<ACharacterController>(Player->GetController());
	if (!Controller || !Controller->OpenBonfireMenu(Checkpoint))
	{
		UE_LOG(LogTemp, Warning, TEXT("Checkpoint '%s' could not open the bonfire menu."),
			*Checkpoint->GetPersistentId().ToString());
	}
}

bool ATestGameMode::RequestRestAtCheckpoint(ACheckpointActor* Checkpoint, AMyCharacter* Player)
{
	if (bTransitionInProgress || !Checkpoint || !Player || Checkpoint->GetPersistentId() == NAME_None)
	{
		return false;
	}

	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rest failed: USoulslikeGameInstance is not active."));
		return false;
	}

	CapturePlayerGold();
	if (!GameInstance->ActivateCheckpointAndSetRespawn(GetCurrentGameplayMapName(), Checkpoint->GetPersistentId()))
	{
		return false;
	}

	StartGameplayReload();
	return true;
}

void ATestGameMode::HandlePlayerDeath(AMyCharacter* Player)
{
	if (bTransitionInProgress || !Player)
	{
		return;
	}

	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Death respawn failed: USoulslikeGameInstance is not active."));
		return;
	}

	FName RespawnCheckpointId = NAME_None;
	if (GameInstance->HasValidContinue())
	{
		RespawnCheckpointId = GameInstance->GetLastCheckpointId();
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("No activated checkpoint is available; death will restart from PlayerStart."));
	}

	GameInstance->PrepareGameplayTransition(GetCurrentGameplayMapName(), RespawnCheckpointId);

	bTransitionInProgress = true;
	CapturePlayerGold();

	if (ACharacterController* Controller = Cast<ACharacterController>(Player->GetController()))
	{
		Controller->ShowDeathOverlay();
	}

	GetWorldTimerManager().SetTimer(DeathRespawnTimer, this, &ATestGameMode::StartGameplayReload,
		DeathOverlayDuration, false);
}

void ATestGameMode::RequestReturnToMainMenu(ACharacterController* RequestingController)
{
	if (bTransitionInProgress)
	{
		return;
	}

	bTransitionInProgress = true;
	CapturePlayerGold();

	if (RequestingController)
	{
		RequestingController->PrepareForMapTransition();
	}

	if (USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>())
	{
		GameInstance->ReturnToMainMenu();
	}
}

ACheckpointActor* ATestGameMode::FindCheckpointById(FName PersistentId) const
{
	if (PersistentId == NAME_None || !GetWorld())
	{
		return nullptr;
	}

	ACheckpointActor* Match = nullptr;
	for (TActorIterator<ACheckpointActor> It(GetWorld()); It; ++It)
	{
		if (It->GetPersistentId() != PersistentId)
		{
			continue;
		}

		if (Match)
		{
			UE_LOG(LogTemp, Warning, TEXT("Duplicate checkpoint PersistentId '%s' in map '%s'. Using '%s'."),
				*PersistentId.ToString(), *GetCurrentGameplayMapName().ToString(), *Match->GetName());
			continue;
		}

		Match = *It;
	}

	return Match;
}

FName ATestGameMode::GetCurrentGameplayMapName() const
{
	return FName(*UGameplayStatics::GetCurrentLevelName(this, true));
}

void ATestGameMode::RestorePlayerFromSave(AMyCharacter* Player)
{
	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	UAttributeComponent* Attributes = Player ? Player->GetAttributes() : nullptr;
	if (!GameInstance || !Attributes)
	{
		return;
	}

	const UTestSaveGame* SaveGame = GameInstance->GetCurrentSaveGame();
	if (!SaveGame || !SaveGame->IsPersistable())
	{
		UE_LOG(LogTemp, Warning, TEXT("Player restore skipped: no persistable current save."));
		return;
	}

	bRestoringPlayerState = true;
	Attributes->RestoreCheckpointResources();
	Attributes->SetGold(SaveGame->Gold);
	bRestoringPlayerState = false;

	Player->RestoreItemOwnershipFromSave(SaveGame);
}

bool ATestGameMode::CanUseCheckpoint(const ACheckpointActor* Checkpoint, const AMyCharacter* Player) const
{
	return !bTransitionInProgress && Checkpoint && Checkpoint->GetPersistentId() != NAME_None && Player
		&& Player->CanInteractWithWorld() && !IsPlayerEngagedByEnemy(Player);
}

bool ATestGameMode::IsPlayerEngagedByEnemy(const AMyCharacter* Player) const
{
	if (!Player || !GetWorld())
	{
		return false;
	}

	for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
	{
		if (It->IsEngagingActor(Player))
		{
			return true;
		}
	}

	return false;
}

void ATestGameMode::CapturePlayerGold()
{
	if (!ManagedPlayer.IsValid())
	{
		return;
	}

	if (UAttributeComponent* Attributes = ManagedPlayer->GetAttributes())
	{
		if (USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>())
		{
			GameInstance->UpdateGold(Attributes->GetGold());
		}
	}
}

void ATestGameMode::StartGameplayReload()
{
	if (!GetWorld())
	{
		return;
	}

	bTransitionInProgress = true;
	TransitionMapName = GetCurrentGameplayMapName();

	if (ACharacterController* Controller = Cast<ACharacterController>(GetWorld()->GetFirstPlayerController()))
	{
		Controller->PrepareForMapTransition();
	}

	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (PlayerController->PlayerCameraManager)
		{
			PlayerController->PlayerCameraManager->StartCameraFade(0.f, 1.f, MapFadeDuration, FLinearColor::Black, false, true);
		}
	}

	GetWorldTimerManager().SetTimer(MapTransitionTimer, this, &ATestGameMode::FinishGameplayReload, MapFadeDuration, false);
}

void ATestGameMode::FinishGameplayReload()
{
	if (!bTransitionInProgress || !GetWorld() || TransitionMapName == NAME_None)
	{
		return;
	}

	UGameplayStatics::OpenLevel(this, TransitionMapName);
}

void ATestGameMode::HandlePlayerGoldChanged(int32 NewGold)
{
	if (bRestoringPlayerState)
	{
		return;
	}

	if (USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>())
	{
		GameInstance->UpdateGold(NewGold);
	}
}
