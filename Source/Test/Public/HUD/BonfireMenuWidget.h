#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BonfireMenuWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBonfireRestRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBonfireLeaveRequested);

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

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Rest = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Leave = nullptr;

private:
	UFUNCTION()
	void HandleRestClicked();

	UFUNCTION()
	void HandleLeaveClicked();
};
