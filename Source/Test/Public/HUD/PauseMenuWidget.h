// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UButton;
class UCheckBox;
class UOverlay;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnResumeRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnQuitRequested);

/**
 * 暂停菜单Widget
 * 职责：UI显示和用户交互，通过delegate通知Controller
 */
UCLASS()
class TEST_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/* Delegates */
	UPROPERTY(BlueprintAssignable, Category = "Pause Menu")
	FOnResumeRequested OnResumeDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Pause Menu")
	FOnQuitRequested OnQuitDelegate;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/* Widget Components */
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Resume;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Btn_Quit;

	UPROPERTY(meta = (BindWidget))
	UOverlay* Overlay_Background;

	UPROPERTY(meta = (BindWidgetOptional))
	UCheckBox* CB_DebugEnabled;

	UPROPERTY(meta = (BindWidgetOptional))
	UCheckBox* CB_DebugPlayer;

	UPROPERTY(meta = (BindWidgetOptional))
	UCheckBox* CB_DebugEnemy;

	UPROPERTY(meta = (BindWidgetOptional))
	UCheckBox* CB_DebugRanges;

	UPROPERTY(meta = (BindWidgetOptional))
	UCheckBox* CB_DebugShapes;

	/* Button Callbacks */
	UFUNCTION()
	void OnResumeClicked();

	UFUNCTION()
	void OnQuitClicked();

	UFUNCTION()
	void OnDebugEnabledChanged(bool bIsChecked);

	UFUNCTION()
	void OnDebugPlayerChanged(bool bIsChecked);

	UFUNCTION()
	void OnDebugEnemyChanged(bool bIsChecked);

	UFUNCTION()
	void OnDebugRangesChanged(bool bIsChecked);

	UFUNCTION()
	void OnDebugShapesChanged(bool bIsChecked);

	/* 音效接口（第一版空实现，后续扩展） */
	virtual void PlayPauseSound();
	virtual void PlayResumeSound();
};
