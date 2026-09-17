// Fill out your copyright notice in the Description page of Project Settings.


#include "PCG/ArenaGenerator.h"
#include "PCGComponent.h"
#include "Components/SplineComponent.h"

AArenaGenerator::AArenaGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	// 1. 创建样条线，设置为根组件
	ArenaSpline = CreateDefaultSubobject<USplineComponent>(TEXT("ArenaSpline"));
	SetRootComponent(ArenaSpline);
	// 闭合样条线，让它默认就是一个圈
	ArenaSpline->SetClosedLoop(true);

	// 2. 创建 PCG 组件并附加到样条线上
	PCGGenerator = CreateDefaultSubobject<UPCGComponent>(TEXT("PCGGenerator"));
}

void AArenaGenerator::BeginPlay()
{
	Super::BeginPlay();
	
}

void AArenaGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

