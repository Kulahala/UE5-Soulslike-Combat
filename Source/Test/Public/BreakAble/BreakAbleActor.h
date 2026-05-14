#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/HitInterface.h"
#include "BreakAbleActor.generated.h"

class ATreasure;
class UGeometryCollectionComponent;
class UNiagaraSystem;

UCLASS()
class TEST_API ABreakAbleActor : public AActor, public IHitInterface
{
	GENERATED_BODY()

public:
	ABreakAbleActor();

	virtual void GetHit_Implementation(const FVector& ImpactPoint, AActor* HitInstigator) override;

protected:
	virtual void BeginPlay() override;

	//SM替换GC
	void BreakReplaced(const FVector& ImpactPoint);
	
	//破碎后放置宝物
	void SpawnSingleTreasure(UWorld* World);

	// --- 破碎后的表现配置 ---
	UPROPERTY(EditAnywhere, Category = "Destruction", meta = (ToolTip = "破碎时播放的粒子特效。"))
	UNiagaraSystem* ImpactParticle;

	UPROPERTY(EditAnywhere, Category = "Destruction", meta = (ToolTip = "破碎时播放的音效。"))
	USoundBase* ImpactSound;

	UPROPERTY(EditAnywhere, Category = "Destruction", meta = (ToolTip = "破碎时碎片飞散的力度。"))
	float ExplosionForce = 500.f;

	// 多少秒后回收整个 Actor
	UPROPERTY(EditAnywhere, Category = "Destruction", meta = (ToolTip = "破碎后多少秒销毁整个 Actor。"))
	float DestructionLifeSpan = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Destruction", meta = (ToolTip = "破碎爆炸的影响半径。"))
	float ExplosionRadius = 300.f;

	UPROPERTY(EditAnywhere, Category = "Destruction|Drops", meta = (ToolTip = "掉落宝物的基类。"))
	TSubclassOf<class ATreasure> BaseTreasureClass;

	// 可能会掉落的物品卡片池
	UPROPERTY(EditAnywhere, Category = "Destruction|Drops", meta = (ToolTip = "可掉落的宝物数据资产池，随机从列表中选取。"))
	TArray<class UTreasureData*> PossibleDrops;

	UPROPERTY(EditAnywhere, Category = "Destruction|Drops", meta = (ToolTip = "最少掉落宝物数量。"))
	int32 MinDrops = 1;

	UPROPERTY(EditAnywhere, Category = "Destruction|Drops", meta = (ToolTip = "最多掉落宝物数量。"))
	int32 MaxDrops = 3;

private:
	// 1. 完整的静态网格体
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "完整的静态网格体，破碎前显示。"))
	UStaticMeshComponent* StaticMeshComp;

	// 2. 碎裂用的几何体组件 (初始隐藏)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "碎裂用的几何体组件，破碎后替换静态网格体。"))
	UGeometryCollectionComponent* GCComp;

public:
	FORCEINLINE UStaticMeshComponent* GetStaticMeshComp() const { return StaticMeshComp; }
	FORCEINLINE UGeometryCollectionComponent* GetGCComp() const { return GCComp; }
};
