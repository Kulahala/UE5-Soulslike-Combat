// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/HealthBarComponent.h"
#include "Components/ProgressBar.h"
#include "HUD/BaseHealthBarWidget.h"


void UHealthBarComponent::SetHealthPercent(float Percent)
{
	if (HealthBarWidget == nullptr)
	{
		HealthBarWidget = Cast<UBaseHealthBarWidget>(GetUserWidgetObject());
	}
	if (HealthBarWidget&& HealthBarWidget->PB_Health)
	{
		HealthBarWidget->SetHealthPercent(Percent);
	}
}
