// Fill out your copyright notice in the Description page of Project Settings.

#include "HUD/DeathMenuWidget.h"

#include "Components/Button.h"

void UDeathMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Restart)
	{
		Btn_Restart->OnClicked.RemoveDynamic(this, &UDeathMenuWidget::OnRestartClicked);
		Btn_Restart->OnClicked.AddDynamic(this, &UDeathMenuWidget::OnRestartClicked);
	}

	if (Btn_Quit)
	{
		Btn_Quit->OnClicked.RemoveDynamic(this, &UDeathMenuWidget::OnQuitClicked);
		Btn_Quit->OnClicked.AddDynamic(this, &UDeathMenuWidget::OnQuitClicked);
	}

	SetIsFocusable(true);
	SetKeyboardFocus();
}

void UDeathMenuWidget::NativeDestruct()
{
	if (Btn_Restart)
	{
		Btn_Restart->OnClicked.RemoveDynamic(this, &UDeathMenuWidget::OnRestartClicked);
	}

	if (Btn_Quit)
	{
		Btn_Quit->OnClicked.RemoveDynamic(this, &UDeathMenuWidget::OnQuitClicked);
	}

	Super::NativeDestruct();
}

void UDeathMenuWidget::OnRestartClicked()
{
	OnRestartDelegate.Broadcast();
}

void UDeathMenuWidget::OnQuitClicked()
{
	OnQuitDelegate.Broadcast();
}
