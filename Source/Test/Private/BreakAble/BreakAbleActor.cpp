// ABreakAbleActor.cpp
#include "BreakAble/BreakAbleActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Items/Treasures/Treasure.h"
#include "Kismet/GameplayStatics.h"

ABreakAbleActor::ABreakAbleActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	// 创建完整模型
	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComp"));
	StaticMeshComp->SetupAttachment(RootComponent);
	StaticMeshComp->SetCollisionObjectType(ECC_Destructible);

	// 创建碎裂模型
	GCComp = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GCComp"));
	GCComp->SetupAttachment(RootComponent);
	GCComp->SetGenerateOverlapEvents(true);
	GCComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GCComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	// 初始状态：隐藏且关闭物理
	GCComp->SetVisibility(false);
	GCComp->SetSimulatePhysics(false);
	GCComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABreakAbleActor::BeginPlay()
{
	Super::BeginPlay();
}

void ABreakAbleActor::BreakReplaced(const FVector& ImpactPoint)
{
	// --- 1. 状态切换 ---
	// 隐藏并关闭完整模型的碰撞
	StaticMeshComp->SetVisibility(false);
	StaticMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 显示碎裂模型并开启物理
	GCComp->SetVisibility(true);
	GCComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GCComp->SetSimulatePhysics(true);

	// --- 2. 施加冲击力 ---
	GCComp->AddRadialImpulse(ImpactPoint, ExplosionRadius, ExplosionForce, RIF_Linear, true);

	// --- 3. 播放效果 ---
	if (ImpactParticle)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactParticle, ImpactPoint);
	}

	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, ImpactPoint);
	}
}

void ABreakAbleActor::SpawnSingleTreasure(UWorld* World)
{
	if (World && BaseTreasureClass && PossibleDrops.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, PossibleDrops.Num() - 1);
		UTreasureData* SelectedData = PossibleDrops[RandomIndex];
		if (ATreasure* SpawnedTreasure = World->SpawnActor<ATreasure>(BaseTreasureClass, GetActorLocation(),
		                                                              GetActorRotation()))
		{
			SpawnedTreasure->InitializeFromData(SelectedData);
			// 计算随机落点 (在距离中心 50 到 150 范围内)
			FVector2D RandomCircle = FMath::RandPointInCircle(150.f);
			FVector TargetLocation = GetActorLocation() + FVector(RandomCircle.X, RandomCircle.Y, 0.f);

			SpawnedTreasure->StartSpawning(TargetLocation);
		}
	}
}

void ABreakAbleActor::GetHit_Implementation(const FVector& ImpactPoint, AActor* HitInstigator)
{
	if (bBroken)
	{
		return;
	}

	bBroken = true;
	BreakReplaced(ImpactPoint);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	int32 DropCount = FMath::RandRange(MinDrops, MaxDrops);
	for (int32 i = 0; i < DropCount; ++i)
	{
		SpawnSingleTreasure(World);
	}
	//延迟销毁
	SetLifeSpan(DestructionLifeSpan);
}
