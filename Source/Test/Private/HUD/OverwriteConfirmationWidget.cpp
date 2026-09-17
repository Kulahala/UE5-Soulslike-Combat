// Fill out your copyright notice in the Description page of Project Settings.

#include "HUD/OverwriteConfirmationWidget.h"

#include "Components/Button.h"

void UOverwriteConfirmationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Btn_Confirm) Btn_Confirm->OnClicked.AddDynamic(this, &UOverwriteConfirmationWidget::HandleConfirmClicked);
	if (Btn_Cancel) Btn_Cancel->OnClicked.AddDynamic(this, &UOverwriteConfirmationWidget::HandleCancelClicked);
}

void UOverwriteConfirmationWidget::NativeDestruct()
{
	if (Btn_Confirm) Btn_Confirm->OnClicked.RemoveDynamic(this, &UOverwriteConfirmationWidget::HandleConfirmClicked);
	if (Btn_Cancel) Btn_Cancel->OnClicked.RemoveDynamic(this, &UOverwriteConfirmationWidget::HandleCancelClicked);
	Super::NativeDestruct();
}

void UOverwriteConfirmationWidget::HandleConfirmClicked()
{
	OnConfirmed.Broadcast();
}

void UOverwriteConfirmationWidget::HandleCancelClicked()
{
	OnCancelled.Broadcast();
}
