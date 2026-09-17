// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractionPromptWidget.generated.h"

class UTextBlock;

/** 世界交互提示的轻量 UMG 基类。布局和动画留给 WBP。 */
UCLASS()
class TEST_API UInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetPromptText(const FText& NewPrompt);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* Text_Prompt = nullptr;
};
