#pragma once

#include "CoreMinimal.h"
#include "EncounterTypes.generated.h"

class AEnemy;

/** 遭遇控制器的运行时生命周期。只表示当前地图实例，不直接持久化。 */
UENUM(BlueprintType)
enum class EEncounterState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Active UMETA(DisplayName = "Active"),
	Cleared UMETA(DisplayName = "Cleared")
};

/** Controller 内部封闭边界的几何形状。Spline 仅支持平面、直线、简单闭环。 */
UENUM(BlueprintType)
enum class EEncounterBoundaryShape : uint8
{
	Rectangle UMETA(DisplayName = "Rectangle"),
	Radial UMETA(DisplayName = "Radial"),
	Spline UMETA(DisplayName = "Spline")
};

/** 遭遇初始生成点的候选区域。候选只在激活时解析，不参与持久化。 */
UENUM(BlueprintType)
enum class EEncounterSpawnAreaShape : uint8
{
	Point UMETA(DisplayName = "Point"),
	Circle UMETA(DisplayName = "Circle"),
	Box UMETA(DisplayName = "Box")
};

/** 一类初始生成参与者及其可用关卡定位点。 */
USTRUCT(BlueprintType)
struct FEncounterSpawnMember
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Spawn", meta = (ToolTip = "激活时 deferred-spawn 的敌人类。必须是有效的 AEnemy 子类。"))
	TSubclassOf<AEnemy> EnemyClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Spawn", meta = (ClampMin = "1", ToolTip = "该敌人类在本次初始批次中生成的数量。"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Spawn", meta = (ToolTip = "一个或多个稳定的 EncounterSpawnPoint ID；激活时从这些定位点中选择。"))
	TArray<FName> SpawnPointIds;
};

/** Encounter 激活时只执行一次的初始生成批次；不包含奖励、持久化或波次调度数据。 */
USTRUCT(BlueprintType)
struct FEncounterInitialSpawnBatch
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Spawn", meta = (ToolTip = "初始生成成员列表。非空时与 PreplacedParticipants 互斥。"))
	TArray<FEncounterSpawnMember> Members;
};

/** 遇战空气墙的关卡作者配置。Controller 原点固定为战斗区中心的地面位置。 */
USTRUCT(BlueprintType)
struct FEncounterBoundaryConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Boundary")
	EEncounterBoundaryShape Shape = EEncounterBoundaryShape::Rectangle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Boundary", meta = (ClampMin = "1.0", ToolTip = "Rectangle 专用：从 Controller 原点到四面空气墙内侧碰撞面的 XY 半范围（cm）。"))
	FVector2D InteriorHalfExtents = FVector2D(600.f, 600.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Boundary", meta = (ClampMin = "1.0", ToolTip = "Radial 专用：从 Controller 原点到圆环空气墙内侧碰撞面的半径（cm）。"))
	float InteriorRadius = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Boundary", meta = (ClampMin = "1.0", ToolTip = "每段空气墙的厚度（cm）。墙体只向战斗区外侧延展，不侵入此配置的内侧边界。"))
	float WallThickness = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Boundary", meta = (ClampMin = "1.0", ToolTip = "空气墙从 Controller 原点所在地面向上的总高度（cm）。"))
	float WallHeight = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Boundary", meta = (ClampMin = "8", ClampMax = "32", ToolTip = "Radial 专用：圆环由多少个重叠的薄盒边界组成。"))
	int32 RadialSegmentCount = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Boundary", meta = (ClampMin = "0.0", ToolTip = "玩家胶囊外表面与空气墙内侧碰撞面之间必须额外留出的安全距离（cm）。内圈会自动再扣除玩家胶囊半径。"))
	float SealClearance = 100.f;
};
