#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHUDWidget.generated.h"

class UProgressBar;
class UAttributeComponent;
class AMyCharacter;
class UTexture2D;

UCLASS()
class TEST_API UPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void SetHealthPercent(float Percent);
	UFUNCTION()
	void SetStaminaPercent(float Percent);
	void BindToAttributes(UAttributeComponent* Attributes, AMyCharacter* InCharacter);

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

	// 受击染红
	UPROPERTY()
	AMyCharacter* OwnerCharacter = nullptr;

	float DamageFlashAlpha = 0.0f;
	float DamageFlashPeakAlphaScaled = 0.0f;
	float DamageFlashTimer = 0.0f;
	bool bDamageFlashAttacking = false;

	UPROPERTY(EditAnywhere, Category = "Damage Flash")
	float DamageFlashAttackDuration = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Damage Flash")
	float DamageFlashDuration = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Damage Flash")
	float DamageFlashPeakAlpha = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Damage Flash")
	FLinearColor DamageFlashColor = FLinearColor(1.f, 0.f, 0.f, 1.f);

	// 红晕边缘最大透明度（1.0=完全红，0.5=半透明红晕）
	UPROPERTY(EditAnywhere, Category = "Damage Flash", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VignetteEdgeAlpha = 1.0f;

	// 红晕带宽度：0=边缘, 1=屏幕中心。0.2=外围20%有红晕，中间80%干净
	UPROPERTY(EditAnywhere, Category = "Damage Flash", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float VignetteFadeWidth = 0.2f;

	// 边缘红晕纹理（程序化生成：按到最近屏幕边缘的距离，smoothstep 渐变）
	UPROPERTY()
	UTexture2D* VignetteTexture = nullptr;

	FSlateBrush VignetteBrush;
	bool bVignetteInitialized = false;
	float CachedFadeWidth = 0.2f;
	void InitVignetteBrush();
};
