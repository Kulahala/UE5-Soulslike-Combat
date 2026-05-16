// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BaseHealthBarWidget.generated.h"

class UProgressBar;
/**
 * 
 */
UCLASS()
class TEST_API UBaseHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetHealthPercent(float Percent);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void PlayFadeOutAnim();

	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void CancelFadeOutAnim();

	// 缓冲条追赶逻辑：掉血延迟后追赶，回血瞬间跟上（static，供外部复用）
	static void TickBufferDelayImpl(UProgressBar* Buffer, UProgressBar* Health,
		float& CurrentDelay, float DelayTime, float InterpSpeed, float InDeltaTime);

	UPROPERTY(meta = (BindWidget))
	UProgressBar* PB_Health;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* PB_Buffer;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void TickBufferDelay(float InDeltaTime);
	// 缓冲条追赶真实血条的速度
	UPROPERTY(EditAnywhere, Category = "Health Bar", meta = (ToolTip = "缓冲条追赶真实血条的插值速度。"))
	float BufferInterpSpeed = 3.0f;

	// 缓冲条开始下降前的等待时间（延迟时间）
	UPROPERTY(EditAnywhere, Category = "Health Bar", meta = (ToolTip = "缓冲条开始追赶前的等待时间（秒）。"))
	float BufferDelayTime = 2.0f;

	// 内部计时器，用于记录当前延迟了多久
	float CurrentBufferDelay = 0.0f;
};
