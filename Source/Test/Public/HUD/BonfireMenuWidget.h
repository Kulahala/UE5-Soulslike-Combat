#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "Items/ItemDefinitionDataAsset.h"
#include "Items/ItemOwnershipComponent.h"
#include "BonfireMenuWidget.generated.h"

class UButton;
class UComboBoxString;
class UWidgetSwitcher;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBonfireRestRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBonfireLeaveRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBonfireEquipmentRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBonfireLoadoutSelectionRequested,
	EItemEquipmentSlot, EquipmentSlot, FName, InstanceId);

/** 火堆服务菜单的表示层；重载、存档和玩家保护由 Controller/GameMode 负责。 */
UCLASS(Abstract)
class TEST_API UBonfireMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Bonfire")
	FOnBonfireRestRequested OnRestRequested;

	UPROPERTY(BlueprintAssignable, Category = "Bonfire")
	FOnBonfireLeaveRequested OnLeaveRequested;

	UPROPERTY(BlueprintAssignable, Category = "Bonfire")
	FOnBonfireEquipmentRequested OnEquipmentRequested;

	UPROPERTY(BlueprintAssignable, Category = "Bonfire")
	FOnBonfireLoadoutSelectionRequested OnLoadoutSelectionRequested;

	void SetLoadoutOptions(EItemEquipmentSlot EquipmentSlot, const TArray<FItemLoadoutOption>& Options,
	                       FName SelectedInstanceId);
	void ShowServicePage();
	void ShowLoadoutPage();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Rest = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Leave = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Equipment = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_LoadoutBack = nullptr;

	UPROPERTY(meta = (BindWidget))
	UComboBoxString* Combo_MainHand = nullptr;

	UPROPERTY(meta = (BindWidget))
	UComboBoxString* Combo_OffHand = nullptr;

	UPROPERTY(meta = (BindWidget))
	UWidgetSwitcher* Switcher_Content = nullptr;

private:
	UFUNCTION()
	void HandleRestClicked();

	UFUNCTION()
	void HandleLeaveClicked();

	UFUNCTION()
	void HandleEquipmentClicked();

	UFUNCTION()
	void HandleLoadoutBackClicked();

	UFUNCTION()
	void HandleMainHandSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleOffHandSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	void PopulateLoadoutOptions(UComboBoxString* ComboBox, TMap<FString, FName>& OptionIds,
	                            const TArray<FItemLoadoutOption>& Options, FName SelectedInstanceId);
	void HandleLoadoutSelection(EItemEquipmentSlot EquipmentSlot, const FString& SelectedItem,
	                            const TMap<FString, FName>& OptionIds);

	TMap<FString, FName> MainHandOptionIds;
	TMap<FString, FName> OffHandOptionIds;
	bool bApplyingLoadoutOptions = false;
};
