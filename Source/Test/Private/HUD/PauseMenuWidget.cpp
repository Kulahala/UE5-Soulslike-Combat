// Fill out your copyright notice in the Description page of Project Settings.

#include "HUD/PauseMenuWidget.h"
#include "Components/Button.h"
#include "Components/Overlay.h"

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Resume)
	{
		Btn_Resume->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnResumeClicked);
	}

	// 设置可聚焦并抢焦点，确保NativeOnKeyDown能收到键盘输入
	SetIsFocusable(true);
	SetKeyboardFocus();
}

FReply UPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// 监听P键或ESC恢复游戏
	if (InKeyEvent.GetKey() == EKeys::P || InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnResumeDelegate.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPauseMenuWidget::OnResumeClicked()
{
	PlayResumeSound();
	OnResumeDelegate.Broadcast();
}

void UPauseMenuWidget::PlayPauseSound()
{
	// 第一版空实现，后续可播放音效
}

void UPauseMenuWidget::PlayResumeSound()
{
	// 第一版空实现，后续可播放音效
}
