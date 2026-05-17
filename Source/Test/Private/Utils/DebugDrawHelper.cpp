#include "Utils/DebugDrawHelper.h"
#include "DrawDebugHelpers.h"

static TArray<FDebugDrawEntry> GDebugEntries;
static uint64 GLastFrameNumber = UINT64_MAX;

static TAutoConsoleVariable<int32> CVarDebugEnable(
	TEXT("test.Debug.Enable"), 1,
	TEXT("显示调试文字（0=关 1=开）"), ECVF_Default);

static TAutoConsoleVariable<int32> CVarDebugEnemy(
	TEXT("test.Debug.Enemy"), 1,
	TEXT("显示敌人调试信息（0=关 1=开）"), ECVF_Default);

static TAutoConsoleVariable<int32> CVarDebugShapes(
	TEXT("test.Debug.Shapes"), 1,
	TEXT("显示世界调试图形（0=关 1=开）"), ECVF_Default);

bool FDebugDrawHelper::IsDebugEnabled()
{
	return CVarDebugEnable.GetValueOnGameThread() != 0;
}

bool FDebugDrawHelper::IsEnemyEnabled()
{
	return IsDebugEnabled() && CVarDebugEnemy.GetValueOnGameThread() != 0;
}

bool FDebugDrawHelper::IsShapesEnabled()
{
	return IsDebugEnabled() && CVarDebugShapes.GetValueOnGameThread() != 0;
}

void FDebugDrawHelper::Add(const FString& Text, const FColor& Color)
{
	if (!IsDebugEnabled()) return;

	if (GFrameCounter != GLastFrameNumber)
	{
		GLastFrameNumber = GFrameCounter;
		GDebugEntries.Reset();
	}

	GDebugEntries.Add({Text, Color});
}

void FDebugDrawHelper::AddSphere(UWorld* World, const FVector& Center, float Radius,
	const FColor& Color, int32 Segments)
{
	if (!IsShapesEnabled() || !World) return;
	DrawDebugSphere(World, Center, Radius, Segments, Color, false, -1.f, 0, 1.f);
}

const TArray<FDebugDrawEntry>& FDebugDrawHelper::GetEntries()
{
	static const TArray<FDebugDrawEntry> EmptyEntries;
	if (GFrameCounter != GLastFrameNumber)
	{
		return EmptyEntries;
	}
	return GDebugEntries;
}
