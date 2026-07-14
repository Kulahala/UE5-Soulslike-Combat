// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/MainMenuPlayerController.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Game/SoulslikeGameInstance.h"
#include "HUD/MainMenuWidget.h"
#include "HUD/OverwriteConfirmationWidget.h"
#include "HUD/PlayerHUDWidget.h"
#include "HUD/SettingsWidget.h"
#include "Kismet/KismetSystemLibrary.h"

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	RemoveInheritedGameplayHUDs();

	bShowMouseCursor = true;
	RestoreMainMenuInput();

	if (!IsLocalPlayerController() || !MainMenuWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("MainMenuWidgetClass is not configured on %s."), *GetName());
		return;
	}

	MainMenuWidget = CreateWidget<UMainMenuWidget>(this, MainMenuWidgetClass);
	if (!MainMenuWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create main menu widget."));
		return;
	}

	MainMenuWidget->OnNewGameRequested.AddDynamic(this, &AMainMenuPlayerController::HandleNewGameRequested);
	MainMenuWidget->OnContinueRequested.AddDynamic(this, &AMainMenuPlayerController::HandleContinueRequested);
	MainMenuWidget->OnSettingsRequested.AddDynamic(this, &AMainMenuPlayerController::HandleSettingsRequested);
	MainMenuWidget->OnQuitRequested.AddDynamic(this, &AMainMenuPlayerController::HandleQuitRequested);

	if (USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>())
	{
		MainMenuWidget->SetContinueEnabled(GameInstance->HasValidContinue());
	}
	else
	{
		MainMenuWidget->SetContinueEnabled(false);
	}

	MainMenuWidget->AddToViewport();
}

void AMainMenuPlayerController::HandleNewGameRequested()
{
	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	if (!GameInstance)
	{
		return;
	}

	if (!GameInstance->HasExistingSave())
	{
		BeginGameplayTransition(true);
		return;
	}

	if (!OverwriteConfirmationWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("New Game requires a confirmation widget when a valid save exists."));
		return;
	}

	OverwriteConfirmationWidget = CreateWidget<UOverwriteConfirmationWidget>(this, OverwriteConfirmationWidgetClass);
	if (!OverwriteConfirmationWidget)
	{
		return;
	}

	OverwriteConfirmationWidget->OnConfirmed.AddDynamic(this, &AMainMenuPlayerController::HandleOverwriteConfirmed);
	OverwriteConfirmationWidget->OnCancelled.AddDynamic(this, &AMainMenuPlayerController::HandleOverwriteCancelled);
	OverwriteConfirmationWidget->AddToViewport(10);
}

void AMainMenuPlayerController::HandleContinueRequested()
{
	if (USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>())
	{
		if (GameInstance->HasValidContinue())
		{
			BeginGameplayTransition(false);
		}
	}
}

void AMainMenuPlayerController::HandleSettingsRequested()
{
	if (!SettingsWidgetClass || SettingsWidget)
	{
		return;
	}

	SettingsWidget = CreateWidget<USettingsWidget>(this, SettingsWidgetClass);
	if (!SettingsWidget)
	{
		return;
	}

	SettingsWidget->OnClosed.AddDynamic(this, &AMainMenuPlayerController::HandleSettingsClosed);
	SettingsWidget->AddToViewport(5);
	if (MainMenuWidget)
	{
		MainMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void AMainMenuPlayerController::HandleQuitRequested()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void AMainMenuPlayerController::HandleOverwriteConfirmed()
{
	if (OverwriteConfirmationWidget)
	{
		OverwriteConfirmationWidget->RemoveFromParent();
		OverwriteConfirmationWidget = nullptr;
	}
	BeginGameplayTransition(true);
}

void AMainMenuPlayerController::HandleOverwriteCancelled()
{
	if (OverwriteConfirmationWidget)
	{
		OverwriteConfirmationWidget->RemoveFromParent();
		OverwriteConfirmationWidget = nullptr;
	}
}

void AMainMenuPlayerController::HandleSettingsClosed()
{
	if (SettingsWidget)
	{
		SettingsWidget->RemoveFromParent();
		SettingsWidget = nullptr;
	}
	if (MainMenuWidget)
	{
		MainMenuWidget->SetVisibility(ESlateVisibility::Visible);
	}
}

void AMainMenuPlayerController::BeginGameplayTransition(bool bCreateNewGame)
{
	bCreateNewGameAfterFade = bCreateNewGame;
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(0.f, 1.f, MenuFadeDuration, FLinearColor::Black, false, true);
	}

	GetWorldTimerManager().SetTimer(MenuTransitionTimer, this,
		&AMainMenuPlayerController::FinishGameplayTransition, MenuFadeDuration, false);
}

void AMainMenuPlayerController::FinishGameplayTransition()
{
	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	if (!GameInstance)
	{
		return;
	}

	if (bCreateNewGameAfterFade)
	{
		GameInstance->StartNewGame(GameplayMapName);
	}
	else
	{
		GameInstance->ContinueGame();
	}
}

void AMainMenuPlayerController::RestoreMainMenuInput()
{
	FInputModeUIOnly InputMode;
	SetInputMode(InputMode);
}

void AMainMenuPlayerController::RemoveInheritedGameplayHUDs() const
{
	TArray<UUserWidget*> GameplayHUDWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, GameplayHUDWidgets, UPlayerHUDWidget::StaticClass(), false);

	for (UUserWidget* GameplayHUDWidget : GameplayHUDWidgets)
	{
		if (GameplayHUDWidget)
		{
			GameplayHUDWidget->RemoveFromParent();
		}
	}
}
