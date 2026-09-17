// Fill out your copyright notice in the Description page of Project Settings.

#include "HUD/InteractionPromptWidget.h"

#include "Components/TextBlock.h"

void UInteractionPromptWidget::SetPromptText(const FText& NewPrompt)
{
	if (Text_Prompt)
	{
		Text_Prompt->SetText(NewPrompt);
	}
}
