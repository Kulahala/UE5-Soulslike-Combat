// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingsWidget.generated.h"

class UButton;
class UTestGameUserSettings;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsClosed);

/** 设置面板基础类。具体控件和 SoundMix 引用由 WBP_Settings 维护。 */
UCLASS()
class TEST_API USettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Settings")
	FOnSettingsClosed OnClosed;

	UFUNCTION(BlueprintPure, Category = "Settings")
	UTestGameUserSettings* GetUserSettings() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Btn_Back = nullptr;

	UFUNCTION()
	void HandleBackClicked();
};
