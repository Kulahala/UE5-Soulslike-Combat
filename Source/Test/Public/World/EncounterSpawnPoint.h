#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/EncounterTypes.h"
#include "EncounterSpawnPoint.generated.h"

class UArrowComponent;
class USceneComponent;

/** 为 TODO-02C 波次生成预留的稳定关卡定位点；本阶段不生成敌人。 */
UCLASS()
class TEST_API AEncounterSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AEncounterSpawnPoint();

	FORCEINLINE FName GetSpawnPointId() const { return SpawnPointId; }
	FTransform GetSpawnTransform() const;
	bool TryGetCandidateSpawnTransform(FRandomStream& RandomStream, FTransform& OutTransform) const;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* Root = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UArrowComponent* SpawnArrow = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Encounter", meta = (AllowPrivateAccess = "true", ToolTip = "初始生成批次引用的稳定关卡作者 ID。"))
	FName SpawnPointId = NAME_None;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Encounter|Spawn", meta = (AllowPrivateAccess = "true", ToolTip = "生成候选区域形状。Point 或零范围严格使用 SpawnArrow 的固定 Transform。"))
	EEncounterSpawnAreaShape Shape = EEncounterSpawnAreaShape::Point;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Encounter|Spawn", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ToolTip = "Circle 形状的局部 XY 半径（cm）。大于零时在圆内随机采样。"))
	float CircleRadius = 0.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Encounter|Spawn", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ToolTip = "Box 形状的局部 XY 半范围（cm）。零范围严格使用 SpawnArrow 的固定 Transform。"))
	FVector2D BoxHalfExtents = FVector2D::ZeroVector;
};
