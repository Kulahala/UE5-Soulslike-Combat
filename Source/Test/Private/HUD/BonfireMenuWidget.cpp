#include "HUD/BonfireMenuWidget.h"

#include "Components/Button.h"

void UBonfireMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Rest)
	{
		Btn_Rest->OnClicked.RemoveDynamic(this, &UBonfireMenuWidget::HandleRestClicked);
		Btn_Rest->OnClicked.AddDynamic(this, &UBonfireMenuWidget::HandleRestClicked);
	}

	if (Btn_Leave)
	{
		Btn_Leave->OnClicked.RemoveDynamic(this, &UBonfireMenuWidget::HandleLeaveClicked);
		Btn_Leave->OnClicked.AddDynamic(this, &UBonfireMenuWidget::HandleLeaveClicked);
	}

	SetIsFocusable(true);
	SetKeyboardFocus();
}

void UBonfireMenuWidget::NativeDestruct()
{
	if (Btn_Rest)
	{
		Btn_Rest->OnClicked.RemoveDynamic(this, &UBonfireMenuWidget::HandleRestClicked);
	}

	if (Btn_Leave)
	{
		Btn_Leave->OnClicked.RemoveDynamic(this, &UBonfireMenuWidget::HandleLeaveClicked);
	}

	Super::NativeDestruct();
}

FReply UBonfireMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnLeaveRequested.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UBonfireMenuWidget::HandleRestClicked()
{
	OnRestRequested.Broadcast();
}

void UBonfireMenuWidget::HandleLeaveClicked()
{
	OnLeaveRequested.Broadcast();
}
