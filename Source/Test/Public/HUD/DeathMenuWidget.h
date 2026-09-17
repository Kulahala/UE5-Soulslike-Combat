// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeathMenuWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRestartRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeathQuitRequested);

/**
 * 死亡菜单Widget
 * 职责：只处理UI按钮点击，通过delegate通知Controller执行重开/退出。
 */
UCLASS()
class TEST_API UDeathMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Death Menu")
	FOnRestartRequested OnRestartDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Death Menu")
	FOnDeathQuitRequested OnQuitDelegate;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Restart;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Quit;

	UFUNCTION()
	void OnRestartClicked();

	UFUNCTION()
	void OnQuitClicked();
};
