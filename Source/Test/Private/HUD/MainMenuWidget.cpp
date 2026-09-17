// Fill out your copyright notice in the Description page of Project Settings.

#include "HUD/MainMenuWidget.h"

#include "Components/Button.h"

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_NewGame) Btn_NewGame->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleNewGameClicked);
	if (Btn_Continue) Btn_Continue->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleContinueClicked);
	if (Btn_Settings) Btn_Settings->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSettingsClicked);
	if (Btn_Quit) Btn_Quit->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleQuitClicked);
}

void UMainMenuWidget::NativeDestruct()
{
	if (Btn_NewGame) Btn_NewGame->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleNewGameClicked);
	if (Btn_Continue) Btn_Continue->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleContinueClicked);
	if (Btn_Settings) Btn_Settings->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleSettingsClicked);
	if (Btn_Quit) Btn_Quit->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleQuitClicked);

	Super::NativeDestruct();
}

void UMainMenuWidget::SetContinueEnabled(bool bEnabled)
{
	if (Btn_Continue)
	{
		Btn_Continue->SetIsEnabled(bEnabled);
	}
}

void UMainMenuWidget::HandleNewGameClicked()
{
	OnNewGameRequested.Broadcast();
}

void UMainMenuWidget::HandleContinueClicked()
{
	OnContinueRequested.Broadcast();
}

void UMainMenuWidget::HandleSettingsClicked()
{
	OnSettingsRequested.Broadcast();
}

void UMainMenuWidget::HandleQuitClicked()
{
	OnQuitRequested.Broadcast();
}
