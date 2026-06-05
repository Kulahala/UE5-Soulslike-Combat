#pragma once

#include "CoreMinimal.h"
#include "DrawDebugHelpers.h"

struct FDebugDrawEntry
{
	FString Text;
	FColor Color;
};

class TEST_API FDebugDrawHelper
{
public:
	static void Add(const FString& Text, const FColor& Color = FColor::White);
	static void AddSphere(UWorld* World, const FVector& Center, float Radius,
		const FColor& Color = FColor::Yellow, int32 Segments = 24);

	// 噪音范围可视化（橙色球体，0.5s 持续）
	static void AddNoiseRange(UWorld* World, const FVector& Location, float Range)
	{
		if (IsShapesEnabled() && World)
		{
			DrawDebugSphere(World, Location, Range, 16, FColor::Orange, false, 0.5f);
		}
	}

	static const TArray<FDebugDrawEntry>& GetEntries();

	static bool IsDebugEnabled();
	static bool IsEnemyEnabled();
	static bool IsShapesEnabled();

	static bool GetDebugEnabledRaw();
	static bool GetEnemyEnabledRaw();
	static bool GetShapesEnabledRaw();

	static void SetDebugEnabled(bool bEnabled);
	static void SetEnemyEnabled(bool bEnabled);
	static void SetShapesEnabled(bool bEnabled);
};
