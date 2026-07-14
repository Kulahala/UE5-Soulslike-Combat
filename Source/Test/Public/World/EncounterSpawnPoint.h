#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* Root = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UArrowComponent* SpawnArrow = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Encounter", meta = (AllowPrivateAccess = "true", ToolTip = "未来波次定义引用的稳定关卡作者 ID。"))
	FName SpawnPointId = NAME_None;
};
