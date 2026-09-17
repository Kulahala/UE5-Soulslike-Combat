// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeathOverlayWidget.generated.h"

/** 无按钮死亡 Overlay。WBP 只负责视觉淡入淡出，重生时序属于 ATestGameMode。 */
UCLASS(Abstract)
class TEST_API UDeathOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, Category = "Death")
	void PlayDeathOverlayIn();

	UFUNCTION(BlueprintImplementableEvent, Category = "Death")
	void PlayDeathOverlayOut();
};
