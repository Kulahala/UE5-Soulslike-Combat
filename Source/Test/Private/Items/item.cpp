#include "Items/item.h"

#include "Character/MyCharacter.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"
#include "Game/SoulslikeGameInstance.h"
#include "Kismet/GameplayStatics.h"
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
	Sphere->SetupAttachment(RootComponent);
	Sphere->InitSphereRadius(30.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Sphere->SetGenerateOverlapEvents(true);
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
	InitializePersistentWorldPickup();
	if (IsActorBeingDestroyed())
	{
		return;
	}

	if (ItemState == EItemState::EIS_Spawning)
	{
		DisablePickupCollision();
	}
	else
	{
		EnablePickupCollision();
	}
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
			EnablePickupCollision();
		}
	}
	else if (ItemState == EItemState::EIS_Dropped)
	{
		// 计算Z轴绝对偏移，+1 保证最低点为初始位置，浮动区间为 [0, 2*Amplitude]
		float ZOffset = Amplitude * (FMath::Sin(RunningTime * TimeConstant) + 1.f);

		// 基于初始位置进行绝对位置更新
		SetActorLocation(StartLocation + FVector(0.f, 0.f, ZOffset));
		TryResolveTrackedAutoOverlap();
		if (IsActorBeingDestroyed())
		{
			return;
		}
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
	if (bReconcilingOverlaps || ItemState == EItemState::EIS_Spawning)
	{
		return;
	}

	if (AMyCharacter* Character = Cast<AMyCharacter>(OtherActor))
	{
		if (PickupTriggerPolicy == EItemPickupTriggerPolicy::AutoOverlap)
		{
			TrackAutoOverlapPicker(Character);
		}
		else
		{
			// 交互物应先保留候选；角色恢复可交互后由候选刷新显示提示。
			Character->RegisterInteractable(this);
		}
	}
}

void Aitem::SphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                             UPrimitiveComponent* OtherComp,
                             int32 OtherBodyIndex)
{
	if (AMyCharacter* Character = Cast<AMyCharacter>(OtherActor))
	{
		// 一个角色的多个碰撞组件可能与同一拾取球重叠；只有完全离开后才清理领取状态。
		if (Sphere && Sphere->IsOverlappingActor(Character))
		{
			return;
		}

		if (PickupTriggerPolicy == EItemPickupTriggerPolicy::AutoOverlap)
		{
			ClearAutoOverlapPicker(Character);
		}
		else
		{
			Character->UnregisterInteractable(this);
		}
	}
}

void Aitem::DisablePickupCollision()
{
	AutoOverlapPicker.Reset();
	bAutoOverlapClaimAttempted = false;

	if (Sphere)
	{
		Sphere->SetGenerateOverlapEvents(false);
		Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void Aitem::EnablePickupCollision()
{
	if (!Sphere || ItemState != EItemState::EIS_Dropped || IsActorBeingDestroyed())
	{
		return;
	}

	Sphere->SetGenerateOverlapEvents(false);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	bReconcilingOverlaps = true;
	Sphere->SetGenerateOverlapEvents(true);
	Sphere->UpdateOverlaps();
	HandleCurrentOverlaps();
	bReconcilingOverlaps = false;
}

// ==================== 生成 ====================

void Aitem::StartSpawning(const FVector& Target)
{
	ItemState = EItemState::EIS_Spawning;
	DisablePickupCollision();
	TargetLocation = Target;
	StartLocation = GetActorLocation(); // 重新记录起点为当前位置
	SpawnRunningTime = 0.f;
}

// ==================== 拾取 ====================

void Aitem::OnPickup_Implementation(AActor* Picker)
{
	ResolvePickup(Cast<AMyCharacter>(Picker));
}

bool Aitem::CanInteract_Implementation(AActor* Interactor) const
{
	const AMyCharacter* Character = Cast<AMyCharacter>(Interactor);
	return PickupTriggerPolicy == EItemPickupTriggerPolicy::Interact && Character && Character->CanInteractWithWorld()
		&& !GetOwner() && ItemState == EItemState::EIS_Dropped
		&& (!RequiresPersistentWorldClaim() || bPersistentWorldPickupAvailable);
}

FText Aitem::GetInteractionPrompt_Implementation() const
{
	return FText::FromString(TEXT("拾取"));
}

int32 Aitem::GetInteractionPriority_Implementation() const
{
	return 10;
}

void Aitem::Interact_Implementation(AActor* Interactor)
{
	if (!CanInteract_Implementation(Interactor))
	{
		return;
	}

	IPickupInterface::Execute_OnPickup(this, Interactor);
}

void Aitem::SetPickupTriggerPolicy(EItemPickupTriggerPolicy NewPolicy)
{
	if (PickupTriggerPolicy == NewPolicy)
	{
		return;
	}

	PickupTriggerPolicy = NewPolicy;
	AutoOverlapPicker.Reset();
	bAutoOverlapClaimAttempted = false;
}

bool Aitem::TryGrantPickup(AMyCharacter* Picker, USoundBase*& OutPickupSound)
{
	OutPickupSound = nullptr;
	return TryClaimPersistentWorldPickup(Picker, OutPickupSound);
}

bool Aitem::TryClaimPersistentWorldPickup(AMyCharacter* Picker, USoundBase*& OutPickupSound)
{
	OutPickupSound = nullptr;
	if (!Picker || !RequiresPersistentWorldClaim() || !bPersistentWorldPickupAvailable)
	{
		return false;
	}

	FName InstanceId = NAME_None;
	if (!Picker->TryClaimWorldItemPickup(PersistentId, ItemDefinitionId, PickupQuantity, InstanceId, OutPickupSound))
	{
		return false;
	}

	bPersistentWorldPickupAvailable = false;
	return true;
}

bool Aitem::CanResolvePickup(const AMyCharacter* Picker) const
{
	if (!Picker || GetOwner() || ItemState != EItemState::EIS_Dropped
		|| (RequiresPersistentWorldClaim() && !bPersistentWorldPickupAvailable))
	{
		return false;
	}

	return PickupTriggerPolicy == EItemPickupTriggerPolicy::AutoOverlap
		? Picker->CanAutoCollectWorldPickup()
		: Picker->CanInteractWithWorld();
}

void Aitem::ResolvePickup(AMyCharacter* Picker)
{
	if (bPickupResolutionInProgress || !CanResolvePickup(Picker))
	{
		return;
	}

	bPickupResolutionInProgress = true;
	USoundBase* PickupSound = nullptr;
	const bool bGranted = TryGrantPickup(Picker, PickupSound);
	if (bGranted)
	{
		FinalizePickup(Picker, PickupSound);
	}
	bPickupResolutionInProgress = false;
}

void Aitem::FinalizePickup(AMyCharacter* Picker, USoundBase* PickupSound)
{
	if (Picker)
	{
		Picker->UnregisterInteractable(this);
	}

	DisablePickupCollision();
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	if (Effect)
	{
		Effect->Deactivate();
	}
	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	}

	Destroy();
}

void Aitem::HandleCurrentOverlaps()
{
	if (!Sphere || ItemState != EItemState::EIS_Dropped || IsActorBeingDestroyed())
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	Sphere->GetOverlappingActors(OverlappingActors, AMyCharacter::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		AMyCharacter* Character = Cast<AMyCharacter>(OverlappingActor);
		if (!Character)
		{
			continue;
		}

		if (PickupTriggerPolicy == EItemPickupTriggerPolicy::AutoOverlap)
		{
			TrackAutoOverlapPicker(Character);
			if (IsActorBeingDestroyed())
			{
				break;
			}
		}
		else
		{
			// 与 BeginOverlap 保持一致：落地时即使玩家仍在动作中，也不能丢失候选。
			Character->RegisterInteractable(this);
		}
	}
}

void Aitem::TrackAutoOverlapPicker(AMyCharacter* Picker)
{
	if (!Picker)
	{
		return;
	}

	if (AutoOverlapPicker.Get() != Picker)
	{
		AutoOverlapPicker = Picker;
		bAutoOverlapClaimAttempted = false;
	}

	TryResolveTrackedAutoOverlap();
}

void Aitem::ClearAutoOverlapPicker(AMyCharacter* Picker)
{
	if (!Picker || AutoOverlapPicker.Get() == Picker)
	{
		AutoOverlapPicker.Reset();
		bAutoOverlapClaimAttempted = false;
	}
}

void Aitem::TryResolveTrackedAutoOverlap()
{
	if (PickupTriggerPolicy != EItemPickupTriggerPolicy::AutoOverlap || bAutoOverlapClaimAttempted
		|| ItemState != EItemState::EIS_Dropped || IsActorBeingDestroyed())
	{
		return;
	}

	AMyCharacter* Picker = AutoOverlapPicker.Get();
	if (!Picker)
	{
		return;
	}

	if (!Picker->CanAutoCollectWorldPickup())
	{
		return;
	}

	// 同一连续重叠只尝试一次；保存失败后保留物品，离开并重新进入才允许再次尝试。
	bAutoOverlapClaimAttempted = true;
	ResolvePickup(Picker);
}

void Aitem::InitializePersistentWorldPickup()
{
	if (!RequiresPersistentWorldClaim() || GetOwner())
	{
		return;
	}

	if (PersistentId == NAME_None || ItemDefinitionId == NAME_None || PickupQuantity <= 0)
	{
		bPersistentWorldPickupAvailable = false;
		UE_LOG(LogTemp, Warning, TEXT("World item pickup '%s' requires PersistentId, ItemDefinitionId, and a positive PickupQuantity."), *GetName());
		return;
	}

	if (HasDuplicatePersistentWorldPickupId())
	{
		bPersistentWorldPickupAvailable = false;
		UE_LOG(LogTemp, Warning, TEXT("World item pickup '%s' has duplicate PersistentId '%s' in this map."),
			*GetName(), *PersistentId.ToString());
		return;
	}

	if (USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
		GameInstance && GameInstance->HasClaimedReward(PersistentId))
	{
		Destroy();
	}
}

bool Aitem::HasDuplicatePersistentWorldPickupId() const
{
	if (!GetWorld() || PersistentId == NAME_None)
	{
		return false;
	}

	for (TActorIterator<Aitem> It(GetWorld()); It; ++It)
	{
		const Aitem* Candidate = *It;
		if (!Candidate || Candidate == this || Candidate->GetOwner() || !Candidate->RequiresPersistentWorldClaim())
		{
			continue;
		}

		if (Candidate->PersistentId == PersistentId)
		{
			return true;
		}
	}

	return false;
}
