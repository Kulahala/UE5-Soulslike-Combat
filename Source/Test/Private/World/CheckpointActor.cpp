// Fill out your copyright notice in the Description page of Project Settings.

#include "World/CheckpointActor.h"

#include "Character/MyCharacter.h"
#include "Components/ArrowComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Game/SoulslikeGameInstance.h"
#include "Game/TestGameMode.h"

ACheckpointActor::ACheckpointActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(Root);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(Root);
	InteractionSphere->InitSphereRadius(150.f);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphere->SetGenerateOverlapEvents(true);

	SpawnArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnArrow"));
	SpawnArrow->SetupAttachment(Root);
	// Character 的根点位于胶囊中心，出生点必须抬离地面和火堆碰撞体。
	SpawnArrow->SetRelativeLocation(FVector(100.f, 0.f, 100.f));
}

void ACheckpointActor::BeginPlay()
{
	Super::BeginPlay();

	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ACheckpointActor::OnInteractionBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &ACheckpointActor::OnInteractionEndOverlap);

	if (PersistentId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("Checkpoint '%s' has no PersistentId and cannot be used for save/respawn."), *GetName());
	}
}

bool ACheckpointActor::CanInteract_Implementation(AActor* Interactor) const
{
	const AMyCharacter* Player = Cast<AMyCharacter>(Interactor);
	const ATestGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATestGameMode>() : nullptr;
	return GameMode && GameMode->CanUseCheckpoint(this, Player);
}

FText ACheckpointActor::GetInteractionPrompt_Implementation() const
{
	if (USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>())
	{
		if (GameInstance->HasActivatedCheckpoint(PersistentId))
		{
			return FText::FromString(TEXT("使用火堆"));
		}
	}

	return FText::FromString(TEXT("休息"));
}

int32 ACheckpointActor::GetInteractionPriority_Implementation() const
{
	return 100;
}

void ACheckpointActor::Interact_Implementation(AActor* Interactor)
{
	AMyCharacter* Player = Cast<AMyCharacter>(Interactor);
	ATestGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATestGameMode>() : nullptr;
	if (!Player || !GameMode || !CanInteract_Implementation(Player))
	{
		return;
	}

	GameMode->RequestUseCheckpoint(this, Player);
}

FTransform ACheckpointActor::GetSpawnTransform() const
{
	return SpawnArrow ? SpawnArrow->GetComponentTransform() : GetActorTransform();
}

void ACheckpointActor::OnInteractionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (AMyCharacter* Player = Cast<AMyCharacter>(OtherActor))
	{
		Player->RegisterInteractable(this);
	}
}

void ACheckpointActor::OnInteractionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (AMyCharacter* Player = Cast<AMyCharacter>(OtherActor))
	{
		Player->UnregisterInteractable(this);
	}
}
