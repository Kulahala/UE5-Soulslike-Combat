// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HitInterface.generated.h"

UINTERFACE(MinimalAPI)
class UHitInterface : public UInterface
{
	GENERATED_BODY()
};

class TEST_API IHitInterface
{
	GENERATED_BODY()

public:
	// 命中反馈入口。ImpactPoint 为命中点，HitInstigator 为发起命中的角色或武器拥有者。
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat", meta = (ToolTip = "受击反馈入口。ImpactPoint 为命中点，HitInstigator 为攻击发起者。"))
	void GetHit(const FVector& ImpactPoint, AActor* HitInstigator);

	// 受击特效（音效+粒子），无受击动画，同类命中也触发
	virtual void PlayHitEffects(const FVector& ImpactPoint) {}

	// 纯数学工具：通过点积+叉积计算受击角度，返回 [-180, 180]
	static double GetHitDirection(const FVector& Forward, const FVector& ToHit)
	{
		const double CosTheta = FVector::DotProduct(Forward, ToHit);
		double Theta = FMath::RadiansToDegrees(FMath::Acos(CosTheta));
		const FVector CrossProduct = FVector::CrossProduct(Forward, ToHit);
		if (CrossProduct.Z < 0)
		{
			Theta *= -1.f;
		}
		return Theta;
	}
};
