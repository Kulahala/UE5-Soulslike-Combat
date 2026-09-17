// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMainMenuNewGameRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMainMenuContinueRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMainMenuSettingsRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMainMenuQuitRequested);

/** 主菜单按钮事件与 Continue 可用状态。 */
UCLASS()
class TEST_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Main Menu")
	FOnMainMenuNewGameRequested OnNewGameRequested;

	UPROPERTY(BlueprintAssignable, Category = "Main Menu")
	FOnMainMenuContinueRequested OnContinueRequested;

	UPROPERTY(BlueprintAssignable, Category = "Main Menu")
	FOnMainMenuSettingsRequested OnSettingsRequested;

	UPROPERTY(BlueprintAssignable, Category = "Main Menu")
	FOnMainMenuQuitRequested OnQuitRequested;

	UFUNCTION(BlueprintCallable, Category = "Main Menu")
	void SetContinueEnabled(bool bEnabled);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Btn_NewGame = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Btn_Continue = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Btn_Settings = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Btn_Quit = nullptr;

	UFUNCTION()
	void HandleNewGameClicked();

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleQuitClicked();
};
