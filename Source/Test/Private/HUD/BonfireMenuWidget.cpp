#include "HUD/BonfireMenuWidget.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/WidgetSwitcher.h"

namespace
{
	const FString EmptyLoadoutOption(TEXT("空"));

	FString BuildLoadoutOptionLabel(const FItemLoadoutOption& Option, const TMap<FString, FName>& ExistingOptionIds)
	{
		const FString DisplayName = Option.DisplayName.IsEmpty() ? Option.InstanceId.ToString() : Option.DisplayName.ToString();
		const FString InstanceSuffix = Option.InstanceId.ToString().Right(6);
		const FString BaseLabel = FString::Printf(TEXT("%s [%s]"), *DisplayName, *InstanceSuffix);

		FString Label = BaseLabel;
		int32 DuplicateIndex = 2;
		while (ExistingOptionIds.Contains(Label))
		{
			Label = FString::Printf(TEXT("%s (%d)"), *BaseLabel, DuplicateIndex++);
		}

		return Label;
	}
}

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

	if (Btn_Equipment)
	{
		Btn_Equipment->OnClicked.RemoveDynamic(this, &UBonfireMenuWidget::HandleEquipmentClicked);
		Btn_Equipment->OnClicked.AddDynamic(this, &UBonfireMenuWidget::HandleEquipmentClicked);
	}

	if (Btn_LoadoutBack)
	{
		Btn_LoadoutBack->OnClicked.RemoveDynamic(this, &UBonfireMenuWidget::HandleLoadoutBackClicked);
		Btn_LoadoutBack->OnClicked.AddDynamic(this, &UBonfireMenuWidget::HandleLoadoutBackClicked);
	}

	if (Combo_MainHand)
	{
		Combo_MainHand->OnSelectionChanged.RemoveDynamic(this, &UBonfireMenuWidget::HandleMainHandSelectionChanged);
		Combo_MainHand->OnSelectionChanged.AddDynamic(this, &UBonfireMenuWidget::HandleMainHandSelectionChanged);
	}

	if (Combo_OffHand)
	{
		Combo_OffHand->OnSelectionChanged.RemoveDynamic(this, &UBonfireMenuWidget::HandleOffHandSelectionChanged);
		Combo_OffHand->OnSelectionChanged.AddDynamic(this, &UBonfireMenuWidget::HandleOffHandSelectionChanged);
	}

	ShowServicePage();
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

	if (Btn_Equipment)
	{
		Btn_Equipment->OnClicked.RemoveDynamic(this, &UBonfireMenuWidget::HandleEquipmentClicked);
	}

	if (Btn_LoadoutBack)
	{
		Btn_LoadoutBack->OnClicked.RemoveDynamic(this, &UBonfireMenuWidget::HandleLoadoutBackClicked);
	}

	if (Combo_MainHand)
	{
		Combo_MainHand->OnSelectionChanged.RemoveDynamic(this, &UBonfireMenuWidget::HandleMainHandSelectionChanged);
	}

	if (Combo_OffHand)
	{
		Combo_OffHand->OnSelectionChanged.RemoveDynamic(this, &UBonfireMenuWidget::HandleOffHandSelectionChanged);
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

void UBonfireMenuWidget::HandleEquipmentClicked()
{
	OnEquipmentRequested.Broadcast();
}

void UBonfireMenuWidget::HandleLoadoutBackClicked()
{
	ShowServicePage();
}

void UBonfireMenuWidget::HandleMainHandSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	static_cast<void>(SelectionType);
	HandleLoadoutSelection(EItemEquipmentSlot::MainHand, SelectedItem, MainHandOptionIds);
}

void UBonfireMenuWidget::HandleOffHandSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	static_cast<void>(SelectionType);
	HandleLoadoutSelection(EItemEquipmentSlot::OffHand, SelectedItem, OffHandOptionIds);
}

void UBonfireMenuWidget::SetLoadoutOptions(EItemEquipmentSlot EquipmentSlot,
	const TArray<FItemLoadoutOption>& Options, FName SelectedInstanceId)
{
	bApplyingLoadoutOptions = true;
	if (EquipmentSlot == EItemEquipmentSlot::MainHand)
	{
		PopulateLoadoutOptions(Combo_MainHand, MainHandOptionIds, Options, SelectedInstanceId);
	}
	else if (EquipmentSlot == EItemEquipmentSlot::OffHand)
	{
		PopulateLoadoutOptions(Combo_OffHand, OffHandOptionIds, Options, SelectedInstanceId);
	}
	bApplyingLoadoutOptions = false;
}

void UBonfireMenuWidget::ShowServicePage()
{
	if (Switcher_Content)
	{
		Switcher_Content->SetActiveWidgetIndex(0);
	}
}

void UBonfireMenuWidget::ShowLoadoutPage()
{
	if (Switcher_Content)
	{
		Switcher_Content->SetActiveWidgetIndex(1);
	}
}

void UBonfireMenuWidget::PopulateLoadoutOptions(UComboBoxString* ComboBox, TMap<FString, FName>& OptionIds,
	const TArray<FItemLoadoutOption>& Options, FName SelectedInstanceId)
{
	if (!ComboBox)
	{
		return;
	}

	ComboBox->ClearOptions();
	OptionIds.Reset();
	ComboBox->AddOption(EmptyLoadoutOption);
	OptionIds.Add(EmptyLoadoutOption, NAME_None);

	FString SelectedLabel = EmptyLoadoutOption;
	for (const FItemLoadoutOption& Option : Options)
	{
		if (Option.InstanceId == NAME_None)
		{
			continue;
		}

		const FString Label = BuildLoadoutOptionLabel(Option, OptionIds);
		ComboBox->AddOption(Label);
		OptionIds.Add(Label, Option.InstanceId);
		if (Option.InstanceId == SelectedInstanceId)
		{
			SelectedLabel = Label;
		}
	}

	ComboBox->SetSelectedOption(SelectedLabel);
}

void UBonfireMenuWidget::HandleLoadoutSelection(EItemEquipmentSlot EquipmentSlot, const FString& SelectedItem,
	const TMap<FString, FName>& OptionIds)
{
	if (bApplyingLoadoutOptions)
	{
		return;
	}

	if (const FName* InstanceId = OptionIds.Find(SelectedItem))
	{
		OnLoadoutSelectionRequested.Broadcast(EquipmentSlot, *InstanceId);
	}
}
