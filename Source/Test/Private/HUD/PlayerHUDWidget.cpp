#include "HUD/PlayerHUDWidget.h"
#include "Components/ProgressBar.h"
#include "AttributeComponent/AttributeComponent.h"
#include "Utils/DebugDrawHelper.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"

void UPlayerHUDWidget::SetHealthPercent(float Percent)
{
	if (PB_Health)
	{
		if (Percent < PB_Health->GetPercent())
		{
			CurrentBufferDelay = BufferDelayTime;
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

void UPlayerHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (PB_Buffer && PB_Health)
	{
		const float BufferPercent = PB_Buffer->GetPercent();
		const float TargetPercent = PB_Health->GetPercent();

		if (BufferPercent > TargetPercent)
		{
			if (CurrentBufferDelay > 0.0f)
			{
				CurrentBufferDelay -= InDeltaTime;
			}
			else
			{
				const float NewPercent = FMath::FInterpTo(BufferPercent, TargetPercent, InDeltaTime, BufferInterpSpeed);
				PB_Buffer->SetPercent(NewPercent);
			}
		}
		else if (BufferPercent < TargetPercent)
		{
			PB_Buffer->SetPercent(TargetPercent);
		}
	}
}

int32 UPlayerHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

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
