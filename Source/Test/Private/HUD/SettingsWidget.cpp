// Fill out your copyright notice in the Description page of Project Settings.

#include "HUD/SettingsWidget.h"

#include "Components/Button.h"
#include "Settings/TestGameUserSettings.h"

UTestGameUserSettings* USettingsWidget::GetUserSettings() const
{
	return UTestGameUserSettings::GetTestGameUserSettings();
}

void USettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Btn_Back) Btn_Back->OnClicked.AddDynamic(this, &USettingsWidget::HandleBackClicked);
}

void USettingsWidget::NativeDestruct()
{
	if (Btn_Back) Btn_Back->OnClicked.RemoveDynamic(this, &USettingsWidget::HandleBackClicked);
	Super::NativeDestruct();
}

void USettingsWidget::HandleBackClicked()
{
	OnClosed.Broadcast();
}
