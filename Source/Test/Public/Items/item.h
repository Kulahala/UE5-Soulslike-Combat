// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractableInterface.h"
#include "Interfaces/PickupInterface.h"
#include "item.generated.h"

class AMyCharacter;
class USphereComponent;

UENUM()
enum class EItemState : int8 { EIS_Spawning, EIS_Dropped, EIS_Equipped };

UCLASS()
class TEST_API Aitem : public AActor, public IPickupInterface, public IInteractableInterface
{
	GENERATED_BODY()

public:
	Aitem();

	UFUNCTION(BlueprintCallable, Category = "Spawning", meta = (ToolTip = "从当前位置沿抛物线移动到目标位置，结束后进入可拾取状态。"))
	void StartSpawning(const FVector& Target);

	virtual void OnPickup_Implementation(AActor* Picker) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual int32 GetInteractionPriority_Implementation() const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	/* 生命周期 */
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/* 拾取碰撞 */
	UFUNCTION()
	virtual void SphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	virtual void SphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void DisablePickupCollision();
	virtual bool RequiresPersistentWorldClaim() const { return false; }
	bool TryClaimPersistentWorldPickup(AMyCharacter* Picker);

	/* 抛物线参数 */
	// 抛物线持续时间
	UPROPERTY(EditAnywhere, Category = "Spawning", meta = (ToolTip = "抛物线生成动画的持续时间（秒）。"))
	float SpawnDuration = 0.5f;

	// 抛物线最高点相对偏移
	UPROPERTY(EditAnywhere, Category = "Spawning", meta = (ToolTip = "抛物线最高点相对落点的高度偏移（cm）。"))
	float SpawnHeight = 100.f;

	/* 浮动/自转参数 */
	// 浮动的高度范围
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ToolTip = "闲置时上下浮动的高度范围（cm）。"))
	float Amplitude = 10.f;

	// 浮动的速度频率
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ToolTip = "闲置时上下浮动的频率。值越大浮动越快。"))
	float TimeConstant = 4.0f;

	// 每秒绕Z轴旋转的度数
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ToolTip = "闲置时每秒绕Z轴旋转的度数。"))
	float RotationRate = 45.f;

	/* 状态 */
	UPROPERTY(BlueprintReadOnly, Category = "State", meta = (ToolTip = "当前物品状态：生成抛物线中、掉落待拾取或已装备。"))
	EItemState ItemState = EItemState::EIS_Dropped;

private:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Persistent Pickup", meta = (AllowPrivateAccess = "true", ToolTip = "固定世界拾取物的稳定关卡作者 ID；不可使用 Actor 名称或运行时 GUID。"))
	FName PersistentId = NAME_None;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Persistent Pickup", meta = (AllowPrivateAccess = "true", ToolTip = "该世界拾取物授予的稳定 Item Definition ID。"))
	FName ItemDefinitionId = NAME_None;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Persistent Pickup", meta = (AllowPrivateAccess = "true", ClampMin = "1", ToolTip = "固定世界拾取授予的数量。普通装备必须为 1；只有 Ammo Container 定义允许大于 1。"))
	int32 PickupQuantity = 1;

	/* 组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "根场景组件。"))
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "物品静态网格体。"))
	UStaticMeshComponent* Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "拾取碰撞球体。"))
	USphereComponent* Sphere;

	// 用于播放微弱亮光等特效
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "物品特效组件（微弱亮光等）。"))
	class UNiagaraComponent* Effect;

	/* 抛物线内部 */
	FVector TargetLocation; // 抛物线落点
	float SpawnRunningTime = 0.f; // 抛物线计时器

	/* 浮动内部 */
	float RunningTime = 0.f;
	FVector StartLocation; // 记录初始位置，作为浮动的基准点
	bool bPersistentWorldPickupAvailable = true;

	void InitializePersistentWorldPickup();
	bool HasDuplicatePersistentWorldPickupId() const;

public:
	FORCEINLINE UStaticMeshComponent* GetMesh() const { return Mesh; }
	FORCEINLINE class UNiagaraComponent* GetEffect() const { return Effect; }
	FORCEINLINE USphereComponent* GetSphere() const { return Sphere; }
};
