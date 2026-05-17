#pragma once

#include "CoreMinimal.h"

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

	static const TArray<FDebugDrawEntry>& GetEntries();

	static bool IsDebugEnabled();
	static bool IsEnemyEnabled();
	static bool IsShapesEnabled();
};
