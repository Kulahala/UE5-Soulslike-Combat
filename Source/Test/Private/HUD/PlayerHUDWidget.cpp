#include "HUD/PlayerHUDWidget.h"
#include "HUD/BaseHealthBarWidget.h"
#include "Components/ProgressBar.h"
#include "AttributeComponent/AttributeComponent.h"
#include "Utils/DebugDrawHelper.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "Widgets/InvalidateWidgetReason.h"
#include "Engine/Texture2D.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UPlayerHUDWidget::SetHealthPercent(float Percent)
{
	RefreshHealthValueText();

	if (PB_Health)
	{
		// 更新目标值
		TargetHealthPercent = Percent;

		// 掉血：立即更新 + 触发红晕
		if (Percent < CurrentHealthPercent)
		{
			CurrentHealthPercent = Percent;
			PB_Health->SetPercent(Percent);
			CurrentBufferDelay = BufferDelayTime;
			DamageFlashPeakAlphaScaled = DamageFlashPeakAlpha * PendingDamageFlashScale;
			DamageFlashTimer = 0.f;
			bDamageFlashAttacking = true;
			PendingDamageFlashScale = 1.f;
		}
		// 回血：启动插值（在 NativeTick 中处理）
	}
}

void UPlayerHUDWidget::SetStaminaPercent(float Percent)
{
	RefreshStaminaValueText();

	if (PB_Stamina)
	{
		PB_Stamina->SetPercent(Percent);
	}
}

void UPlayerHUDWidget::SetPotionCount(int32 CurrentCount, int32 MaxCount)
{
	CurrentPotionCount = CurrentCount;
	MaxPotionCount = MaxCount;

	if (Text_PotionCount)
	{
		Text_PotionCount->SetText(FText::Format(INVTEXT("{0}/{1}"), CurrentCount, MaxCount));
	}

	RefreshPotionVisuals();
}

void UPlayerHUDWidget::SetGoldCount(int32 CurrentGold)
{
	if (Text_GoldCount)
	{
		Text_GoldCount->SetText(FText::Format(INVTEXT("Gold: {0}"), CurrentGold));
	}
}

void UPlayerHUDWidget::SetPotionCooldown(float RemainingTime, float TotalTime)
{
	PotionCooldownDuration = FMath::Max(0.f, TotalTime);
	PotionCooldownRemaining = FMath::Clamp(RemainingTime, 0.f, PotionCooldownDuration);
	bPotionCooldownActive = PotionCooldownDuration > 0.f && PotionCooldownRemaining > PotionCooldownTextHideThreshold;

	RefreshPotionVisuals();
}

void UPlayerHUDWidget::BindToAttributes(UAttributeComponent* Attributes)
{
	UnbindFromAttributes();
	BoundAttributes = Attributes;

	if (BoundAttributes)
	{
		BoundAttributes->OnHealthChanged.AddDynamic(this, &UPlayerHUDWidget::SetHealthPercent);

		// 初始化血量显示（同步当前值和目标值）
		float InitialHealth = BoundAttributes->GetHealthPercent();
		TargetHealthPercent = InitialHealth;
		CurrentHealthPercent = InitialHealth;
		if (PB_Health)
		{
			PB_Health->SetPercent(InitialHealth);
		}
		RefreshHealthValueText();

		BoundAttributes->OnStaminaChanged.AddDynamic(this, &UPlayerHUDWidget::SetStaminaPercent);
		SetStaminaPercent(BoundAttributes->GetStaminaPercent());

		BoundAttributes->OnPotionCountChanged.AddDynamic(this, &UPlayerHUDWidget::SetPotionCount);
		SetPotionCount(BoundAttributes->GetPotionCount(), BoundAttributes->GetMaxPotionCount());

		BoundAttributes->OnGoldChanged.AddDynamic(this, &UPlayerHUDWidget::SetGoldCount);
		SetGoldCount(BoundAttributes->GetGold());
	}
}

void UPlayerHUDWidget::UnbindFromAttributes()
{
	if (!BoundAttributes)
	{
		return;
	}

	BoundAttributes->OnHealthChanged.RemoveDynamic(this, &UPlayerHUDWidget::SetHealthPercent);
	BoundAttributes->OnStaminaChanged.RemoveDynamic(this, &UPlayerHUDWidget::SetStaminaPercent);
	BoundAttributes->OnPotionCountChanged.RemoveDynamic(this, &UPlayerHUDWidget::SetPotionCount);
	BoundAttributes->OnGoldChanged.RemoveDynamic(this, &UPlayerHUDWidget::SetGoldCount);
	BoundAttributes = nullptr;
}

void UPlayerHUDWidget::RefreshHealthValueText()
{
	if (!Text_HealthValue || !BoundAttributes)
	{
		return;
	}

	Text_HealthValue->SetText(FText::Format(INVTEXT("{0} / {1}"),
		FMath::RoundToInt(BoundAttributes->GetCurrentHealth()),
		FMath::RoundToInt(BoundAttributes->GetMaxHealth())));
}

void UPlayerHUDWidget::RefreshStaminaValueText()
{
	if (!Text_StaminaValue || !BoundAttributes)
	{
		return;
	}

	Text_StaminaValue->SetText(FText::Format(INVTEXT("{0} / {1}"),
		FMath::RoundToInt(BoundAttributes->GetCurrentStamina()),
		FMath::RoundToInt(BoundAttributes->GetMaxStamina())));
}

void UPlayerHUDWidget::NativeDestruct()
{
	UnbindFromAttributes();
	Super::NativeDestruct();
}

void UPlayerHUDWidget::SetPendingDamageFlashScale(float Scale)
{
	PendingDamageFlashScale = FMath::Max(0.f, Scale);
}

void UPlayerHUDWidget::SetAimReticleVisible(bool bVisible)
{
	if (bAimReticleVisible == bVisible)
	{
		return;
	}

	bAimReticleVisible = bVisible;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UPlayerHUDWidget::RefreshPotionVisuals()
{
	const bool bHasPotion = CurrentPotionCount > 0;
	const ESlateVisibility CooldownVisibility = bPotionCooldownActive
		                                            ? ESlateVisibility::HitTestInvisible
		                                            : ESlateVisibility::Hidden;

	if (Image_PotionIcon)
	{
		Image_PotionIcon->SetRenderOpacity(bHasPotion ? 1.f : EmptyPotionIconOpacity);
	}

	if (Image_PotionCooldownOverlay)
	{
		Image_PotionCooldownOverlay->SetVisibility(CooldownVisibility);
		Image_PotionCooldownOverlay->SetRenderOpacity(PotionCooldownOverlayOpacity);
	}

	if (PB_PotionCooldown)
	{
		const float CooldownPercent = PotionCooldownDuration > 0.f
			                              ? PotionCooldownRemaining / PotionCooldownDuration
			                              : 0.f;
		PB_PotionCooldown->SetVisibility(CooldownVisibility);
		PB_PotionCooldown->SetPercent(CooldownPercent);
	}

	if (Text_PotionCooldown)
	{
		Text_PotionCooldown->SetVisibility(CooldownVisibility);
		if (bPotionCooldownActive)
		{
			Text_PotionCooldown->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), PotionCooldownRemaining)));
		}
	}
}

void UPlayerHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 血条恢复插值（先快后慢）
	if (PB_Health && CurrentHealthPercent < TargetHealthPercent)
	{
		CurrentHealthPercent = FMath::FInterpTo(CurrentHealthPercent, TargetHealthPercent, InDeltaTime, HealthRecoverInterpSpeed);
		PB_Health->SetPercent(CurrentHealthPercent);

		// 到达目标值时停止插值
		if (FMath::IsNearlyEqual(CurrentHealthPercent, TargetHealthPercent, 0.001f))
		{
			CurrentHealthPercent = TargetHealthPercent;
			PB_Health->SetPercent(TargetHealthPercent);
		}
	}

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
			MaxLayer = TextLayer;
		}
	}

	if (bAimReticleVisible)
	{
		const FVector2f LocalSize = FVector2f(AllottedGeometry.GetLocalSize());
		const FVector2f Center = LocalSize * 0.5f;
		constexpr float ReticleHalfGap = 3.f;
		constexpr float ReticleOuterRadius = 10.f;

		const FVector2f Segments[][2] = {
			{ FVector2f(Center.X - ReticleOuterRadius, Center.Y), FVector2f(Center.X - ReticleHalfGap, Center.Y) },
			{ FVector2f(Center.X + ReticleHalfGap, Center.Y), FVector2f(Center.X + ReticleOuterRadius, Center.Y) },
			{ FVector2f(Center.X, Center.Y - ReticleOuterRadius), FVector2f(Center.X, Center.Y - ReticleHalfGap) },
			{ FVector2f(Center.X, Center.Y + ReticleHalfGap), FVector2f(Center.X, Center.Y + ReticleOuterRadius) }
		};

		const int32 OutlineLayer = MaxLayer + 1;
		const int32 InnerLayer = OutlineLayer + 1;
		for (const FVector2f (&Segment)[2] : Segments)
		{
			TArray<FVector2f> OutlinePoints = { Segment[0], Segment[1] };
			FSlateDrawElement::MakeLines(OutDrawElements, OutlineLayer, AllottedGeometry.ToPaintGeometry(),
				MoveTemp(OutlinePoints), ESlateDrawEffect::None, FLinearColor(0.02f, 0.03f, 0.04f, 0.7f), true, 3.f);

			TArray<FVector2f> InnerPoints = { Segment[0], Segment[1] };
			FSlateDrawElement::MakeLines(OutDrawElements, InnerLayer, AllottedGeometry.ToPaintGeometry(),
				MoveTemp(InnerPoints), ESlateDrawEffect::None, FLinearColor(0.94f, 0.96f, 0.98f, 0.95f), true, 1.25f);
		}
		MaxLayer = InnerLayer;
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
