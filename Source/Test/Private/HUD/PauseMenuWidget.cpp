// Fill out your copyright notice in the Description page of Project Settings.

#include "HUD/PauseMenuWidget.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Overlay.h"
#include "Utils/DebugDrawHelper.h"

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Resume)
	{
		Btn_Resume->OnClicked.RemoveDynamic(this, &UPauseMenuWidget::OnResumeClicked);
		Btn_Resume->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnResumeClicked);
	}

	if (CB_DebugEnabled)
	{
		CB_DebugEnabled->OnCheckStateChanged.RemoveDynamic(this, &UPauseMenuWidget::OnDebugEnabledChanged);
		CB_DebugEnabled->SetIsChecked(FDebugDrawHelper::GetDebugEnabledRaw());
		CB_DebugEnabled->OnCheckStateChanged.AddDynamic(this, &UPauseMenuWidget::OnDebugEnabledChanged);
	}

	if (CB_DebugEnemy)
	{
		CB_DebugEnemy->OnCheckStateChanged.RemoveDynamic(this, &UPauseMenuWidget::OnDebugEnemyChanged);
		CB_DebugEnemy->SetIsChecked(FDebugDrawHelper::GetEnemyEnabledRaw());
		CB_DebugEnemy->OnCheckStateChanged.AddDynamic(this, &UPauseMenuWidget::OnDebugEnemyChanged);
	}

	if (CB_DebugShapes)
	{
		CB_DebugShapes->OnCheckStateChanged.RemoveDynamic(this, &UPauseMenuWidget::OnDebugShapesChanged);
		CB_DebugShapes->SetIsChecked(FDebugDrawHelper::GetShapesEnabledRaw());
		CB_DebugShapes->OnCheckStateChanged.AddDynamic(this, &UPauseMenuWidget::OnDebugShapesChanged);
	}

	// 设置可聚焦并抢焦点，确保NativeOnKeyDown能收到键盘输入
	SetIsFocusable(true);
	SetKeyboardFocus();
}

void UPauseMenuWidget::NativeDestruct()
{
	if (Btn_Resume)
	{
		Btn_Resume->OnClicked.RemoveDynamic(this, &UPauseMenuWidget::OnResumeClicked);
	}

	if (CB_DebugEnabled)
	{
		CB_DebugEnabled->OnCheckStateChanged.RemoveDynamic(this, &UPauseMenuWidget::OnDebugEnabledChanged);
	}

	if (CB_DebugEnemy)
	{
		CB_DebugEnemy->OnCheckStateChanged.RemoveDynamic(this, &UPauseMenuWidget::OnDebugEnemyChanged);
	}

	if (CB_DebugShapes)
	{
		CB_DebugShapes->OnCheckStateChanged.RemoveDynamic(this, &UPauseMenuWidget::OnDebugShapesChanged);
	}

	Super::NativeDestruct();
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

void UPauseMenuWidget::OnDebugEnabledChanged(bool bIsChecked)
{
	FDebugDrawHelper::SetDebugEnabled(bIsChecked);
}

void UPauseMenuWidget::OnDebugEnemyChanged(bool bIsChecked)
{
	FDebugDrawHelper::SetEnemyEnabled(bIsChecked);
}

void UPauseMenuWidget::OnDebugShapesChanged(bool bIsChecked)
{
	FDebugDrawHelper::SetShapesEnabled(bIsChecked);
}

void UPauseMenuWidget::PlayPauseSound()
{
	// 第一版空实现，后续可播放音效
}

void UPauseMenuWidget::PlayResumeSound()
{
	// 第一版空实现，后续可播放音效
}
