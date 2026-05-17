#include "HUD/PlayerHUDWidget.h"
#include "HUD/BaseHealthBarWidget.h"
#include "Components/ProgressBar.h"
#include "AttributeComponent/AttributeComponent.h"
#include "Utils/DebugDrawHelper.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "Engine/Texture2D.h"

void UPlayerHUDWidget::SetHealthPercent(float Percent)
{
	if (PB_Health)
	{
		if (Percent < PB_Health->GetPercent())
		{
			CurrentBufferDelay = BufferDelayTime;
			DamageFlashPeakAlphaScaled = DamageFlashPeakAlpha * PendingDamageFlashScale;
			DamageFlashTimer = 0.f;
			bDamageFlashAttacking = true;
			PendingDamageFlashScale = 1.f;
		}
		PB_Health->SetPercent(Percent);
	}
}

void UPlayerHUDWidget::SetStaminaPercent(float Percent)
{
	if (PB_Stamina)
	{
		PB_Stamina->SetPercent(Percent);
	}
}

void UPlayerHUDWidget::BindToAttributes(UAttributeComponent* Attributes)
{
	if (Attributes)
	{
		Attributes->OnHealthChanged.AddDynamic(this, &UPlayerHUDWidget::SetHealthPercent);
		SetHealthPercent(Attributes->GetHealthPercent());

		Attributes->OnStaminaChanged.AddDynamic(this, &UPlayerHUDWidget::SetStaminaPercent);
		SetStaminaPercent(Attributes->GetStaminaPercent());
	}
}

void UPlayerHUDWidget::SetPendingDamageFlashScale(float Scale)
{
	PendingDamageFlashScale = FMath::Max(0.f, Scale);
}

void UPlayerHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UBaseHealthBarWidget::TickBufferDelayImpl(PB_Buffer, PB_Health,
		CurrentBufferDelay, BufferDelayTime, BufferInterpSpeed, InDeltaTime);

	// 受击染红：渐入 → 指数衰减
	if (bDamageFlashAttacking)
	{
		DamageFlashTimer += InDeltaTime;
		if (DamageFlashTimer >= DamageFlashAttackDuration)
		{
			DamageFlashAlpha = DamageFlashPeakAlphaScaled;
			bDamageFlashAttacking = false;
		}
		else
		{
			DamageFlashAlpha = DamageFlashPeakAlphaScaled * (DamageFlashTimer / DamageFlashAttackDuration);
		}
	}
	else if (DamageFlashAlpha > 0.f)
	{
		DamageFlashAlpha *= FMath::Pow(0.01f, InDeltaTime / DamageFlashDuration);
		if (DamageFlashAlpha < 0.005f) DamageFlashAlpha = 0.f;
	}

	InitVignetteBrush();
}

int32 UPlayerHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	// 受击染红（边缘红晕：按到最近屏幕边缘的距离渐变，外围 FadeWidth 区域有红晕）
	if (DamageFlashAlpha > 0.f)
	{
		if (VignetteBrush.GetResourceObject())
		{
			const FVector2f LocalSize = FVector2f(AllottedGeometry.GetLocalSize());
			const float EdgeAlpha = DamageFlashAlpha * VignetteEdgeAlpha;
			const FLinearColor DrawColor(DamageFlashColor.R, DamageFlashColor.G, DamageFlashColor.B, EdgeAlpha);
			FSlateDrawElement::MakeBox(OutDrawElements, MaxLayer + 1,
				AllottedGeometry.ToPaintGeometry(LocalSize, FSlateLayoutTransform()),
				&VignetteBrush, ESlateDrawEffect::None, DrawColor);
			MaxLayer += 1;
		}
	}

	if (FDebugDrawHelper::IsDebugEnabled())
	{
		const TArray<FDebugDrawEntry>& Entries = FDebugDrawHelper::GetEntries();
		if (Entries.Num() > 0)
		{
			const int32 TextLayer = MaxLayer + 1;
			const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Bold", 22);
			const FVector2f LocalSize = FVector2f(AllottedGeometry.GetLocalSize());
			const float LineHeight = 28.f;
			const float StartY = LocalSize.Y * 0.5f - Entries.Num() * LineHeight * 0.5f;

			for (int32 i = 0; i < Entries.Num(); ++i)
			{
				const FVector2f Position(10.f, StartY + i * LineHeight);
				FSlateDrawElement::MakeText(OutDrawElements, TextLayer,
					AllottedGeometry.ToPaintGeometry(FVector2f(300.f, LineHeight), FSlateLayoutTransform(Position)),
					Entries[i].Text, FontInfo, ESlateDrawEffect::None, Entries[i].Color);
			}
			return TextLayer;
		}
	}

	return MaxLayer;
}

void UPlayerHUDWidget::InitVignetteBrush()
{
	if (bVignetteInitialized && FMath::IsNearlyEqual(CachedFadeWidth, VignetteFadeWidth)) return;
	bVignetteInitialized = true;
	CachedFadeWidth = VignetteFadeWidth;

	if (VignetteTexture)
	{
		VignetteBrush.SetResourceObject(nullptr);
		VignetteTexture->ConditionalBeginDestroy();
		VignetteTexture = nullptr;
	}

	const int32 Size = 256;
	VignetteTexture = UTexture2D::CreateTransient(Size, Size, PF_R8G8B8A8);
	if (!VignetteTexture) return;

	FTexture2DMipMap& Mip = VignetteTexture->GetPlatformData()->Mips[0];
	uint8* Pixels = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));

	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			// 归一化坐标：0=边缘, 1=中心
			const float U = (X + 0.5f) / Size;
			const float V = (Y + 0.5f) / Size;
			const float EdgeDist = FMath::Min(
				FMath::Min(U, 1.f - U),
				FMath::Min(V, 1.f - V)
			);

			const float T = 1.f - FMath::Clamp(EdgeDist / CachedFadeWidth, 0.f, 1.f);
			const float Smooth = T * T * (3.f - 2.f * T);  // smoothstep
			const uint8 Alpha = static_cast<uint8>(Smooth * 255.f);

			const int32 PixelIndex = (Y * Size + X) * 4;
			Pixels[PixelIndex + 0] = 255;
			Pixels[PixelIndex + 1] = 255;
			Pixels[PixelIndex + 2] = 255;
			Pixels[PixelIndex + 3] = Alpha;
		}
	}

	Mip.BulkData.Unlock();
	VignetteTexture->SRGB = true;
	VignetteTexture->NeverStream = true;
	VignetteTexture->Filter = TF_Bilinear;
	VignetteTexture->UpdateResource();

	VignetteBrush.SetResourceObject(VignetteTexture);
	VignetteBrush.ImageSize = FVector2D(Size, Size);
	VignetteBrush.DrawAs = ESlateBrushDrawType::Image;
	VignetteBrush.Tiling = ESlateBrushTileType::NoTile;
}
