// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaGenerator.generated.h"

class UPCGComponent;
class USplineComponent;

UCLASS()
class TEST_API AArenaGenerator : public AActor
{
	GENERATED_BODY()
	
public:	
	AArenaGenerator();
	virtual void Tick(float DeltaTime) override;
protected:
	virtual void BeginPlay() override;

	// 用于手绘竞技场边缘的样条线组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Components", meta = (ToolTip = "手绘竞技场边缘的样条线组件。"))
	TObjectPtr<USplineComponent> ArenaSpline;

	// 挂载和执行生成逻辑的 PCG 组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Components", meta = (ToolTip = "挂载和执行 PCG 程序化生成逻辑的组件。"))
	TObjectPtr<UPCGComponent> PCGGenerator;

public:	
	

};
