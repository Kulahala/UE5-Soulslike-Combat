// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OverwriteConfirmationWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOverwriteConfirmed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOverwriteCancelled);

/** 覆盖唯一存档前的二次确认对话框。 */
UCLASS()
class TEST_API UOverwriteConfirmationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Confirmation")
	FOnOverwriteConfirmed OnConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Confirmation")
	FOnOverwriteCancelled OnCancelled;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Btn_Confirm = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Btn_Cancel = nullptr;

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();
};
