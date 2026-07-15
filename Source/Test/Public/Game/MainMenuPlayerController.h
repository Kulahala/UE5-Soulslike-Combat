// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuPlayerController.generated.h"

class UMainMenuWidget;
class UOverwriteConfirmationWidget;
class USettingsWidget;

/** 主菜单 UMG 生命周期与 New Game/Continue 的流程入口。 */
UCLASS()
class TEST_API AMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (ToolTip = "WBP_MainMenu，父类必须是 UMainMenuWidget。"))
	TSubclassOf<UMainMenuWidget> MainMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (ToolTip = "覆盖存档确认框，父类必须是 UOverwriteConfirmationWidget。"))
	TSubclassOf<UOverwriteConfirmationWidget> OverwriteConfirmationWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (ToolTip = "WBP_Settings，父类必须是 USettingsWidget。"))
	TSubclassOf<USettingsWidget> SettingsWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Maps", meta = (ToolTip = "唯一可继续的游戏地图。"))
	FName GameplayMapName = FName(TEXT("TestMap"));

	UPROPERTY(EditDefaultsOnly, Category = "Game Flow")
	float MenuFadeDuration = 0.25f;

private:
	UPROPERTY()
	UMainMenuWidget* MainMenuWidget = nullptr;

	UPROPERTY()
	UOverwriteConfirmationWidget* OverwriteConfirmationWidget = nullptr;

	UPROPERTY()
	USettingsWidget* SettingsWidget = nullptr;

	FTimerHandle MenuTransitionTimer;
	bool bCreateNewGameAfterFade = false;

	UFUNCTION()
	void HandleNewGameRequested();

	UFUNCTION()
	void HandleContinueRequested();

	UFUNCTION()
	void HandleSettingsRequested();

	UFUNCTION()
	void HandleQuitRequested();

	UFUNCTION()
	void HandleOverwriteConfirmed();

	UFUNCTION()
	void HandleOverwriteCancelled();

	UFUNCTION()
	void HandleSettingsClosed();

	void BeginGameplayTransition(bool bCreateNewGame);
	void FinishGameplayTransition();
	void RecoverFromFailedGameplayTransition();
	void RestoreMainMenuInput();
	void RemoveInheritedGameplayHUDs() const;
};
