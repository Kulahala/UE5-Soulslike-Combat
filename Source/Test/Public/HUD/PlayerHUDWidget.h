#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHUDWidget.generated.h"

class UProgressBar;
class UAttributeComponent;

UCLASS()
class TEST_API UPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void SetHealthPercent(float Percent);
	UFUNCTION()
	void SetStaminaPercent(float Percent);
	void BindToAttributes(UAttributeComponent* Attributes);

	UPROPERTY(meta = (BindWidget))
	UProgressBar* PB_Health;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* PB_Buffer;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* PB_Stamina;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	// 缓冲条追赶真实血条的速度
	UPROPERTY(EditAnywhere, Category = "Health Bar")
	float BufferInterpSpeed = 3.0f;

	// 缓冲条开始下降前的等待时间（延迟时间）
	UPROPERTY(EditAnywhere, Category = "Health Bar")
	float BufferDelayTime = 2.0f;

	float CurrentBufferDelay = 0.0f;
};
