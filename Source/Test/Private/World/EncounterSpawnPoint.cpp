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
		UE_LOG(LogTemp, Warning, TEXT("Encounter spawn point '%s' has no SpawnPointId and cannot be referenced by an initial spawn batch."), *GetName());
	}

	if (CircleRadius < 0.f || BoxHalfExtents.X < 0.f || BoxHalfExtents.Y < 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter spawn point '%s' has invalid negative candidate area dimensions."), *GetName());
	}
}

FTransform AEncounterSpawnPoint::GetSpawnTransform() const
{
	return SpawnArrow ? SpawnArrow->GetComponentTransform() : GetActorTransform();
}

bool AEncounterSpawnPoint::TryGetCandidateSpawnTransform(FRandomStream& RandomStream, FTransform& OutTransform) const
{
	if (!SpawnArrow || CircleRadius < 0.f || BoxHalfExtents.X < 0.f || BoxHalfExtents.Y < 0.f)
	{
		return false;
	}

	const FTransform ArrowTransform = SpawnArrow->GetComponentTransform();
	FVector LocalOffset = FVector::ZeroVector;

	switch (Shape)
	{
	case EEncounterSpawnAreaShape::Point:
		break;

	case EEncounterSpawnAreaShape::Circle:
		if (CircleRadius > KINDA_SMALL_NUMBER)
		{
			const float Angle = RandomStream.FRandRange(0.f, 2.f * PI);
			const float Radius = FMath::Sqrt(RandomStream.FRand()) * CircleRadius;
			LocalOffset = FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
		}
		break;

	case EEncounterSpawnAreaShape::Box:
		if (!BoxHalfExtents.IsNearlyZero())
		{
			LocalOffset = FVector(
				RandomStream.FRandRange(-BoxHalfExtents.X, BoxHalfExtents.X),
				RandomStream.FRandRange(-BoxHalfExtents.Y, BoxHalfExtents.Y),
				0.f);
		}
		break;

	default:
		return false;
	}

	OutTransform = ArrowTransform;
	OutTransform.SetLocation(ArrowTransform.TransformPosition(LocalOffset));
	return true;
}
