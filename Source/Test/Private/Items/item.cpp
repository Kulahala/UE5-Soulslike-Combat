#include "Items/item.h"

#include "Character/MyCharacter.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"

// ==================== 生命周期 ====================

Aitem::Aitem()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->SetGenerateOverlapEvents(true);
	Sphere->SetupAttachment(RootComponent);
	Sphere->InitSphereRadius(30.f);
	Sphere->SetHiddenInGame(true);

	Effect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Effect"));
	Effect->SetupAttachment(RootComponent);
}

void Aitem::BeginPlay()
{
	Super::BeginPlay();

	StartLocation = GetActorLocation();

	Sphere->OnComponentEndOverlap.AddDynamic(this, &Aitem::SphereEndOverlap);
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &Aitem::SphereOverlap);
}

void Aitem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	RunningTime += DeltaTime;

	if (ItemState == EItemState::EIS_Spawning)
	{
		SpawnRunningTime += DeltaTime;
		float Alpha = FMath::Clamp(SpawnRunningTime / SpawnDuration, 0.f, 1.f);

		// XY 轴线性插值
		FVector CurrentLocation = FMath::Lerp(StartLocation, TargetLocation, Alpha);

		// Z 轴叠加抛物线 (正弦波 0->1->0)
		CurrentLocation.Z += FMath::Sin(Alpha * PI) * SpawnHeight;

		SetActorLocation(CurrentLocation);

		// 抛物线结束
		if (Alpha >= 1.f)
		{
			ItemState = EItemState::EIS_Dropped;
			StartLocation = TargetLocation; // 更新掉落后的浮动基准点
			RunningTime = 0.f; // 重置浮动时间
		}
	}
	else if (ItemState == EItemState::EIS_Dropped)
	{
		// 计算Z轴绝对偏移，+1 保证最低点为初始位置，浮动区间为 [0, 2*Amplitude]
		float ZOffset = Amplitude * (FMath::Sin(RunningTime * TimeConstant) + 1.f);

		// 基于初始位置进行绝对位置更新
		SetActorLocation(StartLocation + FVector(0.f, 0.f, ZOffset));
	}

	// 无论是在抛物线中还是浮动中，都保持自转
	if (ItemState == EItemState::EIS_Spawning || ItemState == EItemState::EIS_Dropped)
	{
		AddActorWorldRotation(FRotator(0.f, RotationRate * DeltaTime, 0.f));
	}
}

// ==================== 拾取碰撞 ====================

void Aitem::SphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                          int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AMyCharacter* SlashCharacter = Cast<AMyCharacter>(OtherActor);
	if (SlashCharacter)
	{
		// 无主武器优先：当前 OverLapItem 有主人而自己没有时，覆盖
		Aitem* Current = SlashCharacter->GetEquippedItem();
		if (!Current || !GetOwner() || Current->GetOwner())
		{
			SlashCharacter->SetEquippedItem(this);
		}
	}
}

void Aitem::SphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                             UPrimitiveComponent* OtherComp,
                             int32 OtherBodyIndex)
{
	if (AMyCharacter* SlashCharacter = Cast<AMyCharacter>(OtherActor))
	{
		if (SlashCharacter->GetEquippedItem() == this)
		{
			SlashCharacter->SetEquippedItem(nullptr);
		}
	}
}

void Aitem::DisablePickupCollision()
{
	if (Sphere)
	{
		Sphere->SetGenerateOverlapEvents(false);
		Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

// ==================== 生成 ====================

void Aitem::StartSpawning(const FVector& Target)
{
	ItemState = EItemState::EIS_Spawning;
	TargetLocation = Target;
	StartLocation = GetActorLocation(); // 重新记录起点为当前位置
	SpawnRunningTime = 0.f;
}

// ==================== 拾取 ====================

void Aitem::OnPickup_Implementation(AActor* Picker)
{
	// 默认空实现，由 Weapon/Treasure/Shield 等子类重写
}
