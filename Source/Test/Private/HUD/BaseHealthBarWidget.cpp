// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/BaseHealthBarWidget.h"

#include "Components/ProgressBar.h"

void UBaseHealthBarWidget::SetHealthPercent(float Percent)
{
	if (PB_Health)
	{
		// 如果是扣血，重置延迟计时器
		if (Percent < PB_Health->GetPercent())
		{
			CurrentBufferDelay = BufferDelayTime;
		}
		PB_Health->SetPercent(Percent);
	}
}

void UBaseHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TickBufferDelay(InDeltaTime);
}

void UBaseHealthBarWidget::TickBufferDelay(float InDeltaTime)
{
	TickBufferDelayImpl(PB_Buffer, PB_Health, CurrentBufferDelay, BufferDelayTime, BufferInterpSpeed, InDeltaTime);
}

void UBaseHealthBarWidget::TickBufferDelayImpl(UProgressBar* Buffer, UProgressBar* Health,
	float& CurrentDelay, float DelayTime, float InterpSpeed, float InDeltaTime)
{
	if (!Buffer || !Health) return;

	const float BufferPercent = Buffer->GetPercent();
	const float TargetPercent = Health->GetPercent();

	if (BufferPercent > TargetPercent)
	{
		if (CurrentDelay > 0.0f)
		{
			CurrentDelay -= InDeltaTime;
		}
		else
		{
			const float NewPercent = FMath::FInterpTo(BufferPercent, TargetPercent, InDeltaTime, InterpSpeed);
			Buffer->SetPercent(NewPercent);
		}
	}
	else if (BufferPercent < TargetPercent)
	{
		Buffer->SetPercent(TargetPercent);
	}
}

