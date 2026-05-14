#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TreasureData.generated.h"

/**
 * 
 */
UCLASS()
class TEST_API UTreasureData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Treasure Info", meta = (ToolTip = "宝物的静态网格体。"))
	UStaticMesh* TreasureMesh;

	UPROPERTY(EditAnywhere, Category = "Treasure Info", meta = (ToolTip = "宝物的金币价值。"))
	int32 GoldValue = 10;
	
	UPROPERTY(EditAnywhere, Category = "Treasure Info", meta = (ToolTip = "拾取宝物时播放的音效。"))
	USoundBase* PickUpSound;

	UPROPERTY(EditAnywhere, Category = "Treasure Info", meta = (ToolTip = "宝物显示名称。"))
	FString TreasureName = TEXT("宝物");

	UPROPERTY(EditAnywhere, Category = "Treasure Info", meta = (ToolTip = "宝物缩放比例。"))
	float TreasureScale = 1.f;

protected:

private:

};
