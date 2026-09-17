// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/MainMenuGameMode.h"

#include "Game/MainMenuPlayerController.h"

AMainMenuGameMode::AMainMenuGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AMainMenuPlayerController::StaticClass();
}
