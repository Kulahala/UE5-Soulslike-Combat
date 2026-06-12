// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PickupInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UPickupInterface : public UInterface
{
	GENERATED_BODY()
};

class TEST_API IPickupInterface
{
	GENERATED_BODY()

public:
	// 被角色按拾取键调用，Picker 为拾取者。
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup", meta = (ToolTip = "拾取入口。Picker 为触发拾取的角色或 Actor。"))
	void OnPickup(AActor* Picker);
};
