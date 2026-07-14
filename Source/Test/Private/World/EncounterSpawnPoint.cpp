#include "World/EncounterSpawnPoint.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"

AEncounterSpawnPoint::AEncounterSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SpawnArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnArrow"));
	SpawnArrow->SetupAttachment(Root);
	SpawnArrow->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
}

void AEncounterSpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	if (SpawnPointId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter spawn point '%s' has no SpawnPointId and cannot be referenced by future waves."), *GetName());
	}
}

FTransform AEncounterSpawnPoint::GetSpawnTransform() const
{
	return SpawnArrow ? SpawnArrow->GetComponentTransform() : GetActorTransform();
}
