// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Items/item.h"
#include "Treasure.generated.h"

class UTreasureData;

UCLASS()
class TEST_API ATreasure : public Aitem
{
	GENERATED_BODY()

public:
	ATreasure();

	UFUNCTION(BlueprintCallable, Category = "Treasure Properties")
	void InitializeFromData(UTreasureData* Data);

	void SetGoldValue(int32 NewValue) { GoldValue = NewValue; }
	int32 GetGoldValue() const { return GoldValue; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) override;
	virtual void SphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Treasure Properties", meta = (ToolTip = "宝物的金币价值。"))
	int32 GoldValue = 10;

	UPROPERTY(EditAnywhere, Category = "Sounds", meta = (ToolTip = "拾取宝物时播放的音效。"))
	USoundBase* PickSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Treasure Properties", meta = (ToolTip = "宝物显示名称。"))
	FString TreasureName = TEXT("宝物");

private:

};
