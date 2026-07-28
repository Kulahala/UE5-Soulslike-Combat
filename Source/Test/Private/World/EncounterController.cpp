#include "World/EncounterController.h"

#include "Character/MyCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemy/Enemy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/SoulslikeGameInstance.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "NavigationSystem.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"
#include "World/EncounterSpawnPoint.h"

namespace
{
	constexpr float BoundarySegmentOverlap = 2.f;
	constexpr float TransformTolerance = 0.01f;
	constexpr float SplinePlanarTolerance = 0.1f;
	constexpr float SplinePointTolerance = 0.1f;
	constexpr float SplineSafeRegionProbeMinHalfExtent = 1.f;
	constexpr int32 MaxSplineSafeRegionProbeCells = 16384;
	constexpr float SqrtTwo = 1.41421356237f;
	constexpr int32 MaxSpawnLocationAttempts = 8;
	constexpr float SpawnGroundProbeUp = 500.f;
	constexpr float SpawnGroundProbeDown = 1000.f;
	constexpr float SpawnGroundClearance = 1.f;
	constexpr float SpawnGroundHeightTolerance = 75.f;
	constexpr float SpawnGroundNormalMinZ = 0.5f;

	struct FSplineSafeRegionProbeCell
	{
		FVector2D Center = FVector2D::ZeroVector;
		float HalfExtent = 0.f;
		float SignedDistance = 0.f;
		float MaximumSignedDistance = 0.f;
	};

	struct FEncounterSpawnReservation
	{
		FVector Location = FVector::ZeroVector;
		float Radius = 0.f;
		float HalfHeight = 0.f;
	};

	bool DoEncounterSpawnReservationsOverlap(const FEncounterSpawnReservation& First,
		const FEncounterSpawnReservation& Second)
	{
		const float CombinedRadius = First.Radius + Second.Radius;
		if (FVector::DistSquared2D(First.Location, Second.Location) > FMath::Square(CombinedRadius))
		{
			return false;
		}

		const float FirstBottom = First.Location.Z - First.HalfHeight;
		const float FirstTop = First.Location.Z + First.HalfHeight;
		const float SecondBottom = Second.Location.Z - Second.HalfHeight;
		const float SecondTop = Second.Location.Z + Second.HalfHeight;
		return FirstBottom < SecondTop && SecondBottom < FirstTop;
	}

	void PushSplineSafeRegionProbeCell(TArray<FSplineSafeRegionProbeCell>& Heap,
		const FSplineSafeRegionProbeCell& NewCell)
	{
		int32 Index = Heap.Add(NewCell);
		while (Index > 0)
		{
			const int32 ParentIndex = (Index - 1) / 2;
			if (Heap[ParentIndex].MaximumSignedDistance >= Heap[Index].MaximumSignedDistance)
			{
				break;
			}

			Heap.Swap(ParentIndex, Index);
			Index = ParentIndex;
		}
	}

	FSplineSafeRegionProbeCell PopSplineSafeRegionProbeCell(TArray<FSplineSafeRegionProbeCell>& Heap)
	{
		check(!Heap.IsEmpty());

		const FSplineSafeRegionProbeCell Result = Heap[0];
		const FSplineSafeRegionProbeCell LastCell = Heap.Pop(EAllowShrinking::No);
		if (Heap.IsEmpty())
		{
			return Result;
		}

		Heap[0] = LastCell;
		int32 Index = 0;
		while (true)
		{
			const int32 LeftChildIndex = Index * 2 + 1;
			const int32 RightChildIndex = LeftChildIndex + 1;
			int32 LargestIndex = Index;

			if (LeftChildIndex < Heap.Num() &&
				Heap[LeftChildIndex].MaximumSignedDistance > Heap[LargestIndex].MaximumSignedDistance)
			{
				LargestIndex = LeftChildIndex;
			}

			if (RightChildIndex < Heap.Num() &&
				Heap[RightChildIndex].MaximumSignedDistance > Heap[LargestIndex].MaximumSignedDistance)
			{
				LargestIndex = RightChildIndex;
			}

			if (LargestIndex == Index)
			{
				break;
			}

			Heap.Swap(Index, LargestIndex);
			Index = LargestIndex;
		}

		return Result;
	}

	float CrossProduct2D(const FVector2D& A, const FVector2D& B, const FVector2D& C)
	{
		return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
	}

	bool IsPointOnSegment2D(const FVector2D& Point, const FVector2D& SegmentStart, const FVector2D& SegmentEnd)
	{
		if (!FMath::IsNearlyZero(CrossProduct2D(SegmentStart, SegmentEnd, Point), SplinePointTolerance))
		{
			return false;
		}

		return Point.X >= FMath::Min(SegmentStart.X, SegmentEnd.X) - SplinePointTolerance &&
			Point.X <= FMath::Max(SegmentStart.X, SegmentEnd.X) + SplinePointTolerance &&
			Point.Y >= FMath::Min(SegmentStart.Y, SegmentEnd.Y) - SplinePointTolerance &&
			Point.Y <= FMath::Max(SegmentStart.Y, SegmentEnd.Y) + SplinePointTolerance;
	}

	bool DoSegmentsIntersect2D(const FVector2D& FirstStart, const FVector2D& FirstEnd,
		const FVector2D& SecondStart, const FVector2D& SecondEnd)
	{
		const float FirstStartSide = CrossProduct2D(FirstStart, FirstEnd, SecondStart);
		const float FirstEndSide = CrossProduct2D(FirstStart, FirstEnd, SecondEnd);
		const float SecondStartSide = CrossProduct2D(SecondStart, SecondEnd, FirstStart);
		const float SecondEndSide = CrossProduct2D(SecondStart, SecondEnd, FirstEnd);

		const bool bFirstStraddlesSecond =
			(FirstStartSide > SplinePointTolerance && FirstEndSide < -SplinePointTolerance) ||
			(FirstStartSide < -SplinePointTolerance && FirstEndSide > SplinePointTolerance);
		const bool bSecondStraddlesFirst =
			(SecondStartSide > SplinePointTolerance && SecondEndSide < -SplinePointTolerance) ||
			(SecondStartSide < -SplinePointTolerance && SecondEndSide > SplinePointTolerance);
		if (bFirstStraddlesSecond && bSecondStraddlesFirst)
		{
			return true;
		}

		return IsPointOnSegment2D(SecondStart, FirstStart, FirstEnd) ||
			IsPointOnSegment2D(SecondEnd, FirstStart, FirstEnd) ||
			IsPointOnSegment2D(FirstStart, SecondStart, SecondEnd) ||
			IsPointOnSegment2D(FirstEnd, SecondStart, SecondEnd);
	}

	bool AreSplineEdgesAdjacent(int32 FirstEdgeIndex, int32 SecondEdgeIndex, int32 PointCount)
	{
		return FirstEdgeIndex == SecondEdgeIndex ||
			(FirstEdgeIndex + 1) % PointCount == SecondEdgeIndex ||
			(SecondEdgeIndex + 1) % PointCount == FirstEdgeIndex;
	}

	float GetPointToSegmentDistanceSquared2D(const FVector2D& Point, const FVector2D& SegmentStart,
		const FVector2D& SegmentEnd)
	{
		const FVector2D Segment = SegmentEnd - SegmentStart;
		const float SegmentLengthSquared = Segment.SizeSquared();
		if (SegmentLengthSquared <= KINDA_SMALL_NUMBER)
		{
			return FVector2D::DistSquared(Point, SegmentStart);
		}

		const float Projection = FMath::Clamp(FVector2D::DotProduct(Point - SegmentStart, Segment) / SegmentLengthSquared, 0.f, 1.f);
		return FVector2D::DistSquared(Point, SegmentStart + Segment * Projection);
	}

	float GetSignedPolygonArea(const TArray<FVector2D>& Points)
	{
		float TwiceArea = 0.f;
		for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
		{
			const FVector2D& CurrentPoint = Points[PointIndex];
			const FVector2D& NextPoint = Points[(PointIndex + 1) % Points.Num()];
			TwiceArea += CurrentPoint.X * NextPoint.Y - NextPoint.X * CurrentPoint.Y;
		}

		return TwiceArea * 0.5f;
	}
}

AEncounterController::AEncounterController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	RectangularCommitVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("RectangularCommitVolume"));
	RectangularCommitVolume->SetupAttachment(Root);
	RectangularCommitVolume->InitBoxExtent(FVector(1.f, 1.f, 1.f));
	RectangularCommitVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RectangularCommitVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	RectangularCommitVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RectangularCommitVolume->SetGenerateOverlapEvents(false);

	RadialCommitVolume = CreateDefaultSubobject<UCapsuleComponent>(TEXT("RadialCommitVolume"));
	RadialCommitVolume->SetupAttachment(Root);
	RadialCommitVolume->InitCapsuleSize(1.f, 1.f);
	RadialCommitVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RadialCommitVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	RadialCommitVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RadialCommitVolume->SetGenerateOverlapEvents(false);

	SplineCommitCandidateVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SplineCommitCandidateVolume"));
	SplineCommitCandidateVolume->SetupAttachment(Root);
	SplineCommitCandidateVolume->InitBoxExtent(FVector(1.f, 1.f, 1.f));
	SplineCommitCandidateVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SplineCommitCandidateVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	SplineCommitCandidateVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SplineCommitCandidateVolume->SetGenerateOverlapEvents(false);

	BoundarySpline = CreateDefaultSubobject<USplineComponent>(TEXT("BoundarySpline"));
	BoundarySpline->SetupAttachment(Root);
	BoundarySpline->ClearSplinePoints(false);
	BoundarySpline->AddSplinePoint(FVector(600.f, 600.f, 0.f), ESplineCoordinateSpace::Local, false);
	BoundarySpline->AddSplinePoint(FVector(-600.f, 600.f, 0.f), ESplineCoordinateSpace::Local, false);
	BoundarySpline->AddSplinePoint(FVector(-600.f, -600.f, 0.f), ESplineCoordinateSpace::Local, false);
	BoundarySpline->AddSplinePoint(FVector(600.f, -600.f, 0.f), ESplineCoordinateSpace::Local, false);
	for (int32 PointIndex = 0; PointIndex < BoundarySpline->GetNumberOfSplinePoints(); ++PointIndex)
	{
		BoundarySpline->SetSplinePointType(PointIndex, ESplinePointType::Linear, false);
	}
	BoundarySpline->SetClosedLoop(true, false);
	BoundarySpline->bSplineHasBeenEdited = true;
	BoundarySpline->bDrawDebug = true;
	BoundarySpline->UpdateSpline();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BoundaryCubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (BoundaryCubeFinder.Succeeded())
	{
		BoundaryVisualMesh = BoundaryCubeFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BoundaryMaterialFinder(
		TEXT("/Game/_GAME/BP/Effects/Materials/Gameplay/M_EncounterBoundary.M_EncounterBoundary"));
	if (BoundaryMaterialFinder.Succeeded())
	{
		BoundaryVisualMaterial = BoundaryMaterialFinder.Object;
	}
}

void AEncounterController::BeginPlay()
{
	Super::BeginPlay();

	bStaticConfigurationValid = ValidateConfiguration();
	if (!bStaticConfigurationValid)
	{
		SetCommitVolumeEnabled(RectangularCommitVolume, false);
		SetCommitVolumeEnabled(RadialCommitVolume, false);
		SetCommitVolumeEnabled(SplineCommitCandidateVolume, false);
		SetBoundariesClosed(false);
		return;
	}

	if (USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>())
	{
		if (GameInstance->HasEncounterCleared(EncounterId))
		{
			RestoreClearedStateFromSave();
			return;
		}
	}

	if (!BuildRuntimeBoundaries())
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' could not build its runtime boundaries and is disabled."),
			*EncounterId.ToString());
		bStaticConfigurationValid = false;
		SetBoundariesClosed(false);
		return;
	}

	SetBoundariesClosed(false);
	RectangularCommitVolume->OnComponentBeginOverlap.AddDynamic(this, &AEncounterController::OnCommitVolumeBeginOverlap);
	RectangularCommitVolume->OnComponentEndOverlap.AddDynamic(this, &AEncounterController::OnCommitVolumeEndOverlap);
	RadialCommitVolume->OnComponentBeginOverlap.AddDynamic(this, &AEncounterController::OnCommitVolumeBeginOverlap);
	RadialCommitVolume->OnComponentEndOverlap.AddDynamic(this, &AEncounterController::OnCommitVolumeEndOverlap);
	SplineCommitCandidateVolume->OnComponentBeginOverlap.AddDynamic(this, &AEncounterController::OnCommitVolumeBeginOverlap);
	SplineCommitCandidateVolume->OnComponentEndOverlap.AddDynamic(this, &AEncounterController::OnCommitVolumeEndOverlap);

	bAwaitingPlayerSetup = true;
	RefreshTickEnabled();
	TryInitializeForPlayer();
}

void AEncounterController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (RectangularCommitVolume)
	{
		RectangularCommitVolume->OnComponentBeginOverlap.RemoveDynamic(this, &AEncounterController::OnCommitVolumeBeginOverlap);
		RectangularCommitVolume->OnComponentEndOverlap.RemoveDynamic(this, &AEncounterController::OnCommitVolumeEndOverlap);
	}

	if (RadialCommitVolume)
	{
		RadialCommitVolume->OnComponentBeginOverlap.RemoveDynamic(this, &AEncounterController::OnCommitVolumeBeginOverlap);
		RadialCommitVolume->OnComponentEndOverlap.RemoveDynamic(this, &AEncounterController::OnCommitVolumeEndOverlap);
	}

	if (SplineCommitCandidateVolume)
	{
		SplineCommitCandidateVolume->OnComponentBeginOverlap.RemoveDynamic(this, &AEncounterController::OnCommitVolumeBeginOverlap);
		SplineCommitCandidateVolume->OnComponentEndOverlap.RemoveDynamic(this, &AEncounterController::OnCommitVolumeEndOverlap);
	}

	bAwaitingPlayerSetup = false;
	PendingCommitPlayer.Reset();
	InitialSafeRegionPlayer.Reset();
	SetActorTickEnabled(false);
	SetCommitVolumeEnabled(RectangularCommitVolume, false);
	SetCommitVolumeEnabled(RadialCommitVolume, false);
	SetCommitVolumeEnabled(SplineCommitCandidateVolume, false);
	SetBoundariesClosed(false);
	ReleaseParticipants(true);
	DestroyRuntimeBoundaries();

	Super::EndPlay(EndPlayReason);
}

void AEncounterController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bAwaitingPlayerSetup)
	{
		TryInitializeForPlayer();
		if (bAwaitingPlayerSetup)
		{
			return;
		}
	}

	if (!bConfigurationValid || EncounterState != EEncounterState::Idle)
	{
		RefreshTickEnabled();
		return;
	}

	if (AMyCharacter* InitialPlayer = InitialSafeRegionPlayer.Get())
	{
		if (IsPlayerSafelyInsideCommitRegion(InitialPlayer))
		{
			return;
		}

		bHasObservedPlayerOutsideCommitRegion = true;
		InitialSafeRegionPlayer.Reset();
		if (IsPlayerOverlappingCommitVolume(InitialPlayer))
		{
			BeginPendingCommit(InitialPlayer);
		}
	}

	AMyCharacter* Player = PendingCommitPlayer.Get();
	if (!Player)
	{
		RefreshTickEnabled();
		return;
	}

	if (!IsPlayerOverlappingCommitVolume(Player))
	{
		bHasObservedPlayerOutsideCommitRegion = true;
		ClearPendingCommit();
		return;
	}

	if (!IsPlayerSafelyInsideCommitRegion(Player))
	{
		bHasObservedPlayerOutsideCommitRegion = true;
		return;
	}

	TryActivate(Player);
	ClearPendingCommit();
}

bool AEncounterController::TryActivate(AMyCharacter* Player)
{
	if (!bConfigurationValid || EncounterState != EEncounterState::Idle || !Player ||
		!bHasObservedPlayerOutsideCommitRegion || !IsPlayerSafelyInsideCommitRegion(Player))
	{
		return false;
	}

	if (bUsesInitialSpawnBatch)
	{
		return SpawnInitialBatch(Player);
	}

	for (AEnemy* Participant : PreplacedParticipants)
	{
		if (!IsValid(Participant) || Participant->GetEnemyState() == EEnemyState::EES_Dead)
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' cannot activate because a participant is invalid or dead."),
				*EncounterId.ToString());
			return false;
		}
	}

	RemainingParticipants.Reset();
	for (AEnemy* Participant : PreplacedParticipants)
	{
		if (!Participant->ActivateForEncounter(this, Player))
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' failed to activate participant '%s'. Restoring Idle state."),
				*EncounterId.ToString(), *GetNameSafe(Participant));

			for (AEnemy* RollbackParticipant : PreplacedParticipants)
			{
				if (IsValid(RollbackParticipant))
				{
					RollbackParticipant->SetEncounterDormant(this);
				}
			}

			RemainingParticipants.Reset();
			return false;
		}

		RemainingParticipants.Add(Participant);
	}

	SetEncounterState(EEncounterState::Active);
	SetBoundariesClosed(true);
	return true;
}

void AEncounterController::OnCommitVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OverlappedComponent != GetActiveCommitVolume() || !bConfigurationValid || EncounterState != EEncounterState::Idle)
	{
		return;
	}

	if (AMyCharacter* Player = Cast<AMyCharacter>(OtherActor))
	{
		if (bHasObservedPlayerOutsideCommitRegion)
		{
			BeginPendingCommit(Player);
		}
	}
}

void AEncounterController::OnCommitVolumeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OverlappedComponent != GetActiveCommitVolume())
	{
		return;
	}

	if (AMyCharacter* Player = Cast<AMyCharacter>(OtherActor))
	{
		bHasObservedPlayerOutsideCommitRegion = true;
		if (InitialSafeRegionPlayer.Get() == Player)
		{
			InitialSafeRegionPlayer.Reset();
		}

		if (PendingCommitPlayer.Get() == Player)
		{
			ClearPendingCommit();
		}
		else
		{
			RefreshTickEnabled();
		}
	}
}

bool AEncounterController::ValidateConfiguration()
{
	bUsesInitialSpawnBatch = false;
	EncounterSpawnAnchors.Reset();

	if (EncounterId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter controller '%s' has no EncounterId and is disabled."), *GetName());
		return false;
	}

	if (!GetActorScale3D().Equals(FVector::OneVector, KINDA_SMALL_NUMBER))
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' must keep actor scale at (1, 1, 1); scale the BoundaryConfig or spline points instead."),
			*EncounterId.ToString());
		return false;
	}

	const FRotator ActorRotation = GetActorRotation();
	if (!FMath::IsNearlyZero(ActorRotation.Pitch, TransformTolerance) ||
		!FMath::IsNearlyZero(ActorRotation.Roll, TransformTolerance))
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' only supports yaw rotation because its origin defines the arena floor plane."),
			*EncounterId.ToString());
		return false;
	}

	if (BoundaryConfig.WallThickness <= 0.f || BoundaryConfig.WallHeight <= 0.f || BoundaryConfig.SealClearance < 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' has invalid boundary thickness, height, or seal clearance."),
			*EncounterId.ToString());
		return false;
	}

	switch (BoundaryConfig.Shape)
	{
	case EEncounterBoundaryShape::Rectangle:
		if (BoundaryConfig.InteriorHalfExtents.X <= 0.f || BoundaryConfig.InteriorHalfExtents.Y <= 0.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' has invalid rectangle interior half extents."), *EncounterId.ToString());
			return false;
		}
		break;

	case EEncounterBoundaryShape::Radial:
		if (BoundaryConfig.InteriorRadius <= 0.f || BoundaryConfig.RadialSegmentCount < 8 || BoundaryConfig.RadialSegmentCount > 32)
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' requires a positive radial interior radius and 8-32 boundary segments."),
				*EncounterId.ToString());
			return false;
		}
		break;

	case EEncounterBoundaryShape::Spline:
		if (!ValidateSplineBoundary())
		{
			return false;
		}
		break;

	default:
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' has an unsupported boundary shape."), *EncounterId.ToString());
		return false;
	}

	for (TActorIterator<AEncounterController> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->EncounterId == EncounterId)
		{
			UE_LOG(LogTemp, Warning, TEXT("EncounterId '%s' is duplicated by '%s' and '%s'. Both controllers stay disabled."),
				*EncounterId.ToString(), *GetName(), *It->GetName());
			return false;
		}
	}


	const bool bHasPreplacedParticipants = !PreplacedParticipants.IsEmpty();
	const bool bHasInitialSpawnBatch = !InitialSpawnBatch.Members.IsEmpty();
	if (bHasPreplacedParticipants == bHasInitialSpawnBatch)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' requires exactly one participant source: nonempty PreplacedParticipants or nonempty InitialSpawnBatch, but not both."),
			*EncounterId.ToString());
		return false;
	}

	if (bHasInitialSpawnBatch)
	{
		TMap<FName, AEncounterSpawnPoint*> DiscoveredSpawnAnchors;
		for (TActorIterator<AEncounterSpawnPoint> It(GetWorld()); It; ++It)
		{
			AEncounterSpawnPoint* SpawnPoint = *It;
			const FName SpawnPointId = SpawnPoint ? SpawnPoint->GetSpawnPointId() : NAME_None;
			if (SpawnPointId == NAME_None)
			{
				continue;
			}

			if (DiscoveredSpawnAnchors.Contains(SpawnPointId))
			{
				UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' found duplicate EncounterSpawnPoint ID '%s' in the world."),
					*EncounterId.ToString(), *SpawnPointId.ToString());
				return false;
			}

			DiscoveredSpawnAnchors.Add(SpawnPointId, SpawnPoint);
		}

		for (const FEncounterSpawnMember& Member : InitialSpawnBatch.Members)
		{
			if (!Member.EnemyClass || Member.Count <= 0 || Member.SpawnPointIds.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' has an invalid initial spawn member: class, positive Count, and at least one SpawnPointId are required."),
					*EncounterId.ToString());
				return false;
			}

			for (const FName SpawnPointId : Member.SpawnPointIds)
			{
				if (SpawnPointId == NAME_None || !DiscoveredSpawnAnchors.Contains(SpawnPointId))
				{
					UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' initial spawn member references missing or invalid SpawnPointId '%s'."),
						*EncounterId.ToString(), *SpawnPointId.ToString());
					return false;
				}
			}
		}

		EncounterSpawnAnchors = MoveTemp(DiscoveredSpawnAnchors);
		bUsesInitialSpawnBatch = true;
		return true;
	}

	TSet<AEnemy*> UniqueParticipants;
	for (AEnemy* Participant : PreplacedParticipants)
	{
		if (!IsValid(Participant))
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' has an invalid preplaced participant."), *EncounterId.ToString());
			return false;
		}

		if (UniqueParticipants.Contains(Participant))
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' lists participant '%s' more than once."),
				*EncounterId.ToString(), *Participant->GetName());
			return false;
		}

		UniqueParticipants.Add(Participant);
	}

	return true;
}

bool AEncounterController::ValidateSplineBoundary() const
{
	if (!BoundarySpline)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' has no BoundarySpline and is disabled."), *EncounterId.ToString());
		return false;
	}

	if (!BoundarySpline->IsClosedLoop())
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' requires BoundarySpline to be a closed loop."), *EncounterId.ToString());
		return false;
	}

	const int32 PointCount = BoundarySpline->GetNumberOfSplinePoints();
	if (PointCount < 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' requires at least three spline points."), *EncounterId.ToString());
		return false;
	}

	TArray<FVector2D> Points;
	Points.Reserve(PointCount);
	for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
	{
		if (BoundarySpline->GetSplinePointType(PointIndex) != ESplinePointType::Linear)
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' only supports Linear BoundarySpline points. Point %d is curved."),
				*EncounterId.ToString(), PointIndex);
			return false;
		}

		const FVector SplineLocalPoint = BoundarySpline->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::Local);
		const FVector WorldPoint = BoundarySpline->GetComponentTransform().TransformPosition(SplineLocalPoint);
		const FVector ControllerLocalPoint = GetActorTransform().InverseTransformPositionNoScale(WorldPoint);
		if (!FMath::IsNearlyZero(ControllerLocalPoint.Z, SplinePlanarTolerance))
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' only supports BoundarySpline points on local Z=0. Point %d has Z=%.2f."),
				*EncounterId.ToString(), PointIndex, ControllerLocalPoint.Z);
			return false;
		}

		Points.Emplace(ControllerLocalPoint.X, ControllerLocalPoint.Y);
	}

	for (int32 FirstPointIndex = 0; FirstPointIndex < PointCount; ++FirstPointIndex)
	{
		for (int32 SecondPointIndex = FirstPointIndex + 1; SecondPointIndex < PointCount; ++SecondPointIndex)
		{
			if (FVector2D::DistSquared(Points[FirstPointIndex], Points[SecondPointIndex]) <= FMath::Square(SplinePointTolerance))
			{
				UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' has repeated BoundarySpline points (%d and %d). Remove duplicate or zero-length edges."),
					*EncounterId.ToString(), FirstPointIndex, SecondPointIndex);
				return false;
			}
		}
	}

	for (int32 EdgeIndex = 0; EdgeIndex < PointCount; ++EdgeIndex)
	{
		const FVector2D& EdgeStart = Points[EdgeIndex];
		const FVector2D& EdgeEnd = Points[(EdgeIndex + 1) % PointCount];
		if (FVector2D::DistSquared(EdgeStart, EdgeEnd) <= FMath::Square(SplinePointTolerance))
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' has a zero-length BoundarySpline edge at point %d."),
				*EncounterId.ToString(), EdgeIndex);
			return false;
		}

		for (int32 OtherEdgeIndex = EdgeIndex + 1; OtherEdgeIndex < PointCount; ++OtherEdgeIndex)
		{
			if (AreSplineEdgesAdjacent(EdgeIndex, OtherEdgeIndex, PointCount))
			{
				continue;
			}

			const FVector2D& OtherStart = Points[OtherEdgeIndex];
			const FVector2D& OtherEnd = Points[(OtherEdgeIndex + 1) % PointCount];
			if (DoSegmentsIntersect2D(EdgeStart, EdgeEnd, OtherStart, OtherEnd))
			{
				UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' BoundarySpline self-intersects between edges %d and %d."),
					*EncounterId.ToString(), EdgeIndex, OtherEdgeIndex);
				return false;
			}
		}
	}

	if (FMath::IsNearlyZero(GetSignedPolygonArea(Points), FMath::Square(SplinePointTolerance)))
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' BoundarySpline encloses no usable area."), *EncounterId.ToString());
		return false;
	}

	return true;
}

bool AEncounterController::GetSplineBoundaryPoints(TArray<FVector2D>& OutPoints) const
{
	OutPoints.Reset();
	if (!BoundarySpline)
	{
		return false;
	}

	const int32 PointCount = BoundarySpline->GetNumberOfSplinePoints();
	if (PointCount < 3)
	{
		return false;
	}

	OutPoints.Reserve(PointCount);
	for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
	{
		const FVector SplineLocalPoint = BoundarySpline->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::Local);
		const FVector WorldPoint = BoundarySpline->GetComponentTransform().TransformPosition(SplineLocalPoint);
		const FVector ControllerLocalPoint = GetActorTransform().InverseTransformPositionNoScale(WorldPoint);
		OutPoints.Emplace(ControllerLocalPoint.X, ControllerLocalPoint.Y);
	}

	return true;
}

bool AEncounterController::ConfigureCommitVolumes(AMyCharacter* Player)
{
	if (!Player || !Player->GetCapsuleComponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' could not read the player capsule for its safe commit region."),
			*EncounterId.ToString());
		return false;
	}

	const UCapsuleComponent* PlayerCapsule = Player->GetCapsuleComponent();
	const float PlayerRadius = PlayerCapsule->GetScaledCapsuleRadius();
	const float PlayerHalfHeight = PlayerCapsule->GetScaledCapsuleHalfHeight();
	CommitSafetyInset = PlayerRadius + BoundaryConfig.SealClearance;
	const float CommitVolumeHalfHeight = FMath::Max(BoundaryConfig.WallHeight * 0.5f, PlayerHalfHeight);
	if (BoundaryConfig.WallHeight < PlayerHalfHeight * 2.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' wall height must cover the full player capsule to form a valid boundary."),
			*EncounterId.ToString());
		return false;
	}

	CommitHalfExtents = FVector2D::ZeroVector;
	CommitRadius = 0.f;
	SplineBoundaryPoints.Reset();
	SetCommitVolumeEnabled(RectangularCommitVolume, false);
	SetCommitVolumeEnabled(RadialCommitVolume, false);
	SetCommitVolumeEnabled(SplineCommitCandidateVolume, false);

	if (BoundaryConfig.Shape == EEncounterBoundaryShape::Rectangle)
	{
		CommitHalfExtents = BoundaryConfig.InteriorHalfExtents - FVector2D(CommitSafetyInset, CommitSafetyInset);
		if (CommitHalfExtents.X <= 0.f || CommitHalfExtents.Y <= 0.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' rectangle is too small for the player capsule plus SealClearance."),
				*EncounterId.ToString());
			return false;
		}

		RectangularCommitVolume->SetBoxExtent(FVector(CommitHalfExtents.X, CommitHalfExtents.Y, CommitVolumeHalfHeight), false);
		RectangularCommitVolume->SetRelativeLocation(FVector(0.f, 0.f, PlayerHalfHeight));
		SetCommitVolumeEnabled(RectangularCommitVolume, true);
		return true;
	}

	if (BoundaryConfig.Shape == EEncounterBoundaryShape::Radial)
	{
		CommitRadius = BoundaryConfig.InteriorRadius - CommitSafetyInset;
		if (CommitRadius <= 0.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' radial arena is too small for the player capsule plus SealClearance."),
				*EncounterId.ToString());
			return false;
		}

		RadialCommitVolume->SetCapsuleSize(CommitRadius, FMath::Max(CommitRadius, CommitVolumeHalfHeight), false);
		RadialCommitVolume->SetRelativeLocation(FVector(0.f, 0.f, PlayerHalfHeight));
		SetCommitVolumeEnabled(RadialCommitVolume, true);
		return true;
	}

	return ConfigureSplineCommitVolume(Player, CommitVolumeHalfHeight);
}

bool AEncounterController::ConfigureSplineCommitVolume(AMyCharacter* Player, float CommitVolumeHalfHeight)
{
	if (!GetSplineBoundaryPoints(SplineBoundaryPoints))
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' could not read BoundarySpline points for its safe commit region."),
			*EncounterId.ToString());
		return false;
	}

	FVector2D Minimum = SplineBoundaryPoints[0];
	FVector2D Maximum = SplineBoundaryPoints[0];
	for (const FVector2D& Point : SplineBoundaryPoints)
	{
		Minimum.X = FMath::Min(Minimum.X, Point.X);
		Minimum.Y = FMath::Min(Minimum.Y, Point.Y);
		Maximum.X = FMath::Max(Maximum.X, Point.X);
		Maximum.Y = FMath::Max(Maximum.Y, Point.Y);
	}

	const FVector2D BoundsSize = Maximum - Minimum;
	if (BoundsSize.X <= CommitSafetyInset * 2.f || BoundsSize.Y <= CommitSafetyInset * 2.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' BoundarySpline is too narrow for the player capsule plus SealClearance; no usable safe commit region exists."),
			*EncounterId.ToString());
		return false;
	}

	const FVector2D BoundsCenter = (Minimum + Maximum) * 0.5f;
	const FVector2D BoundsHalfExtents = BoundsSize * 0.5f;
	if (!HasUsableSplineCommitRegion(BoundsCenter, BoundsSize))
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' BoundarySpline has no verified safe commit region for the current player capsule plus SealClearance; the controller is disabled before it claims participants."),
			*EncounterId.ToString());
		return false;
	}

	SplineCommitCandidateVolume->SetBoxExtent(FVector(BoundsHalfExtents.X, BoundsHalfExtents.Y, CommitVolumeHalfHeight), false);
	SplineCommitCandidateVolume->SetRelativeLocation(FVector(BoundsCenter.X, BoundsCenter.Y, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	SetCommitVolumeEnabled(SplineCommitCandidateVolume, true);
	return true;
}

bool AEncounterController::HasUsableSplineCommitRegion(const FVector2D& BoundsCenter, const FVector2D& BoundsSize) const
{
	const float RootHalfExtent = FMath::Max(BoundsSize.X, BoundsSize.Y) * 0.5f;
	if (RootHalfExtent <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	TArray<FSplineSafeRegionProbeCell> ProbeHeap;
	ProbeHeap.Reserve(MaxSplineSafeRegionProbeCells);

	auto MakeProbeCell = [this](const FVector2D& Center, float HalfExtent)
	{
		FSplineSafeRegionProbeCell Cell;
		Cell.Center = Center;
		Cell.HalfExtent = HalfExtent;
		Cell.SignedDistance = GetSignedDistanceToSplineBoundary(Center);
		Cell.MaximumSignedDistance = Cell.SignedDistance + HalfExtent * SqrtTwo;
		return Cell;
	};

	PushSplineSafeRegionProbeCell(ProbeHeap, MakeProbeCell(BoundsCenter, RootHalfExtent));
	int32 CreatedCellCount = 1;

	// Only an actual point satisfying the same clearance rule can validate the configuration.
	while (!ProbeHeap.IsEmpty())
	{
		const FSplineSafeRegionProbeCell Cell = PopSplineSafeRegionProbeCell(ProbeHeap);
		if (Cell.SignedDistance > CommitSafetyInset)
		{
			return true;
		}

		if (Cell.MaximumSignedDistance <= CommitSafetyInset ||
			Cell.HalfExtent <= SplineSafeRegionProbeMinHalfExtent)
		{
			continue;
		}

		if (CreatedCellCount + 4 > MaxSplineSafeRegionProbeCells)
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' could not verify a usable Spline safe region within the %d-cell probe budget. Simplify or widen the boundary."),
				*EncounterId.ToString(), MaxSplineSafeRegionProbeCells);
			return false;
		}

		const float ChildHalfExtent = Cell.HalfExtent * 0.5f;
		for (const float XSign : {-1.f, 1.f})
		{
			for (const float YSign : {-1.f, 1.f})
			{
				const FVector2D ChildCenter = Cell.Center + FVector2D(XSign * ChildHalfExtent, YSign * ChildHalfExtent);
				PushSplineSafeRegionProbeCell(ProbeHeap, MakeProbeCell(ChildCenter, ChildHalfExtent));
				++CreatedCellCount;
			}
		}
	}

	return false;
}

bool AEncounterController::TryInitializeForPlayer()
{
	if (!bAwaitingPlayerSetup)
	{
		return bConfigurationValid;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		return false;
	}

	bAwaitingPlayerSetup = false;
	AMyCharacter* Player = Cast<AMyCharacter>(PlayerPawn);
	if (!Player)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' requires AMyCharacter as player pawn and is disabled."), *EncounterId.ToString());
		SetBoundariesClosed(false);
		DestroyRuntimeBoundaries();
		RefreshTickEnabled();
		return false;
	}

	if (!ConfigureCommitVolumes(Player))
	{
		SetCommitVolumeEnabled(RectangularCommitVolume, false);
		SetCommitVolumeEnabled(RadialCommitVolume, false);
		SetCommitVolumeEnabled(SplineCommitCandidateVolume, false);
		SetBoundariesClosed(false);
		DestroyRuntimeBoundaries();
		RefreshTickEnabled();
		return false;
	}

	if (!InitializeParticipants())
	{
		SetCommitVolumeEnabled(RectangularCommitVolume, false);
		SetCommitVolumeEnabled(RadialCommitVolume, false);
		SetCommitVolumeEnabled(SplineCommitCandidateVolume, false);
		SetBoundariesClosed(false);
		DestroyRuntimeBoundaries();
		RefreshTickEnabled();
		return false;
	}

	bConfigurationValid = true;
	PendingCommitPlayer.Reset();
	InitialSafeRegionPlayer.Reset();
	bHasObservedPlayerOutsideCommitRegion = !IsPlayerSafelyInsideCommitRegion(Player);
	if (!bHasObservedPlayerOutsideCommitRegion)
	{
		InitialSafeRegionPlayer = Player;
	}
	else if (IsPlayerOverlappingCommitVolume(Player))
	{
		BeginPendingCommit(Player);
	}

	RefreshTickEnabled();
	UE_LOG(LogTemp, Display, TEXT("Encounter '%s' initialized in Idle state with %d configured participant(s) from %s."),
		*EncounterId.ToString(), bUsesInitialSpawnBatch ? InitialSpawnBatch.Members.Num() : PreplacedParticipants.Num(),
		bUsesInitialSpawnBatch ? TEXT("InitialSpawnBatch") : TEXT("PreplacedParticipants"));
	return true;
}

bool AEncounterController::InitializeParticipants()
{
	if (bUsesInitialSpawnBatch)
	{
		return true;
	}

	TArray<AEnemy*> ClaimedParticipants;
	for (AEnemy* Participant : PreplacedParticipants)
	{
		if (!Participant->ClaimEncounterOwner(this))
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' could not claim participant '%s'."),
				*EncounterId.ToString(), *GetNameSafe(Participant));

			for (AEnemy* ClaimedParticipant : ClaimedParticipants)
			{
				ClaimedParticipant->ReleaseEncounterOwner(this);
			}

			return false;
		}

		ClaimedParticipants.Add(Participant);
	}

	for (AEnemy* Participant : ClaimedParticipants)
	{
		Participant->GetOnEnemyDied().AddUObject(this, &AEncounterController::HandleParticipantDied);
		Participant->SetEncounterDormant(this);
	}

	return true;
}

bool AEncounterController::ResolveInitialSpawnTransforms(TArray<FTransform>& OutTransforms) const
{
	OutTransforms.Reset();

	if (!bUsesInitialSpawnBatch || EncounterSpawnAnchors.IsEmpty())
	{
		return false;
	}

	UWorld* World = GetWorld();
	UNavigationSystemV1* NavigationSystem = World
		? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)
		: nullptr;
	if (!World || !NavigationSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' cannot resolve its initial spawn batch because World or NavigationSystem is unavailable."),
			*EncounterId.ToString());
		return false;
	}

	FRandomStream RandomStream(FMath::Rand());
	TArray<FEncounterSpawnReservation> PlannedReservations;
	for (const FEncounterSpawnMember& Member : InitialSpawnBatch.Members)
	{
		AEnemy* EnemyCDO = Member.EnemyClass ? Member.EnemyClass->GetDefaultObject<AEnemy>() : nullptr;
		const UCapsuleComponent* Capsule = EnemyCDO ? EnemyCDO->GetCapsuleComponent() : nullptr;
		if (!EnemyCDO || !Capsule)
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' cannot resolve an initial spawn member with an invalid EnemyClass CDO or capsule."),
				*EncounterId.ToString());
			return false;
		}

		const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
		const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		if (!FMath::IsFinite(CapsuleRadius) || !FMath::IsFinite(CapsuleHalfHeight) ||
			CapsuleRadius <= 0.f || CapsuleHalfHeight < CapsuleRadius)
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' cannot resolve EnemyClass '%s' because its CDO capsule dimensions are invalid."),
				*EncounterId.ToString(), *GetNameSafe(Member.EnemyClass));
			return false;
		}

		for (int32 SpawnIndex = 0; SpawnIndex < Member.Count; ++SpawnIndex)
		{
			bool bResolved = false;
			for (int32 Attempt = 0; Attempt < MaxSpawnLocationAttempts; ++Attempt)
			{
				const int32 AnchorIndex = RandomStream.RandRange(0, Member.SpawnPointIds.Num() - 1);
				AEncounterSpawnPoint* SpawnPoint = EncounterSpawnAnchors.FindRef(Member.SpawnPointIds[AnchorIndex]);
				if (!IsValid(SpawnPoint))
				{
					continue;
				}

				FTransform CandidateTransform;
				if (!SpawnPoint->TryGetCandidateSpawnTransform(RandomStream, CandidateTransform) ||
					CandidateTransform.ContainsNaN())
				{
					continue;
				}

				FVector SpawnScale = CandidateTransform.GetScale3D();
				SpawnScale.X = FMath::Abs(SpawnScale.X);
				SpawnScale.Y = FMath::Abs(SpawnScale.Y);
				SpawnScale.Z = FMath::Abs(SpawnScale.Z);
				const float EffectiveRadius = CapsuleRadius * FMath::Max(SpawnScale.X, SpawnScale.Y);
				const float EffectiveHalfHeight = CapsuleHalfHeight * SpawnScale.Z;
				if (!FMath::IsFinite(EffectiveRadius) || !FMath::IsFinite(EffectiveHalfHeight) ||
					EffectiveRadius <= 0.f || EffectiveHalfHeight < EffectiveRadius)
				{
					continue;
				}

				FNavLocation NavLocation;
				const FVector NavigationExtent(
					EffectiveRadius,
					EffectiveRadius,
					FMath::Max(EffectiveHalfHeight + 50.f, 200.f));
				if (!NavigationSystem->ProjectPointToNavigation(CandidateTransform.GetLocation(), NavLocation, NavigationExtent) ||
					NavLocation.Location.ContainsNaN())
				{
					continue;
				}

				FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EncounterInitialSpawn), false, this);
				FHitResult GroundHit;
				const FVector GroundTraceStart = NavLocation.Location + FVector(0.f, 0.f, SpawnGroundProbeUp);
				const FVector GroundTraceEnd = NavLocation.Location - FVector(0.f, 0.f, SpawnGroundProbeDown);
				if (!World->LineTraceSingleByChannel(GroundHit, GroundTraceStart, GroundTraceEnd, ECC_Visibility, QueryParams) ||
					GroundHit.ImpactNormal.Z < SpawnGroundNormalMinZ ||
					FMath::Abs(GroundHit.ImpactPoint.Z - NavLocation.Location.Z) > SpawnGroundHeightTolerance)
				{
					continue;
				}

				const FVector SpawnLocation = GroundHit.ImpactPoint + FVector(0.f, 0.f,
					EffectiveHalfHeight + SpawnGroundClearance);
				if (!IsSpawnCapsuleInsideBoundary(SpawnLocation, EffectiveRadius))
				{
					continue;
				}

				const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(EffectiveRadius, EffectiveHalfHeight);
				if (World->OverlapBlockingTestByChannel(SpawnLocation, CandidateTransform.GetRotation(), ECC_Pawn,
					CapsuleShape, QueryParams))
				{
					continue;
				}

				const FEncounterSpawnReservation CandidateReservation{SpawnLocation, EffectiveRadius, EffectiveHalfHeight};
				bool bOverlapsPlannedReservation = false;
				for (const FEncounterSpawnReservation& PlannedReservation : PlannedReservations)
				{
					if (DoEncounterSpawnReservationsOverlap(CandidateReservation, PlannedReservation))
					{
						bOverlapsPlannedReservation = true;
						break;
					}
				}

				if (bOverlapsPlannedReservation)
				{
					continue;
				}

				CandidateTransform.SetLocation(SpawnLocation);
				OutTransforms.Add(CandidateTransform);
				PlannedReservations.Add(CandidateReservation);
				bResolved = true;
				break;
			}

			if (!bResolved)
			{
				UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' could not resolve a safe transform for initial spawn member '%s' after %d bounded attempts."),
					*EncounterId.ToString(), *GetNameSafe(Member.EnemyClass), MaxSpawnLocationAttempts);
				OutTransforms.Reset();
				return false;
			}
		}
	}

	return !OutTransforms.IsEmpty();
}

bool AEncounterController::IsSpawnCapsuleInsideBoundary(const FVector& SpawnLocation, float CapsuleRadius) const
{
	if (!FMath::IsFinite(CapsuleRadius) || CapsuleRadius <= 0.f)
	{
		return false;
	}

	const FVector LocalSpawnLocation = GetActorTransform().InverseTransformPositionNoScale(SpawnLocation);
	if (BoundaryConfig.Shape == EEncounterBoundaryShape::Rectangle)
	{
		return FMath::Abs(LocalSpawnLocation.X) + CapsuleRadius <= BoundaryConfig.InteriorHalfExtents.X &&
			FMath::Abs(LocalSpawnLocation.Y) + CapsuleRadius <= BoundaryConfig.InteriorHalfExtents.Y;
	}

	if (BoundaryConfig.Shape == EEncounterBoundaryShape::Radial)
	{
		const float MaxCenterRadius = BoundaryConfig.InteriorRadius - CapsuleRadius;
		return MaxCenterRadius >= 0.f && FVector2D(LocalSpawnLocation.X, LocalSpawnLocation.Y).SizeSquared() <=
			FMath::Square(MaxCenterRadius);
	}

	if (BoundaryConfig.Shape == EEncounterBoundaryShape::Spline)
	{
		const FVector2D LocalSpawnPoint(LocalSpawnLocation.X, LocalSpawnLocation.Y);
		return IsPointInsideSplineBoundary(LocalSpawnPoint) &&
			GetMinSquaredDistanceToSplineBoundary(LocalSpawnPoint) >= FMath::Square(CapsuleRadius);
	}

	return false;
}

bool AEncounterController::SpawnInitialBatch(AMyCharacter* Player)
{
	if (!Player || !RuntimeSpawnedParticipants.IsEmpty() || !RemainingParticipants.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' rejected initial batch activation because runtime participant tracking was not empty."),
			*EncounterId.ToString());
		return false;
	}

	TArray<FTransform> SpawnTransforms;
	if (!ResolveInitialSpawnTransforms(SpawnTransforms))
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' rejected initial batch activation before spawning any actor."),
			*EncounterId.ToString());
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	int32 TransformIndex = 0;
	for (const FEncounterSpawnMember& Member : InitialSpawnBatch.Members)
	{
		for (int32 SpawnIndex = 0; SpawnIndex < Member.Count; ++SpawnIndex)
		{
			if (!SpawnTransforms.IsValidIndex(TransformIndex))
			{
				RollbackSpawnedParticipants();
				return false;
			}

			const FTransform& SpawnTransform = SpawnTransforms[TransformIndex++];
			AEnemy* SpawnedEnemy = World->SpawnActorDeferred<AEnemy>(Member.EnemyClass, SpawnTransform, nullptr, nullptr,
				ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
			if (!IsValid(SpawnedEnemy))
			{
				UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' failed to deferred-spawn initial enemy class '%s'; rolling back the batch."),
					*EncounterId.ToString(), *GetNameSafe(Member.EnemyClass));
				RollbackSpawnedParticipants();
				return false;
			}

			RuntimeSpawnedParticipants.Add(SpawnedEnemy);
		}
	}

	for (AEnemy* Participant : RuntimeSpawnedParticipants)
	{
		if (!IsValid(Participant) || !Participant->ClaimEncounterOwner(this))
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' could not claim a deferred spawned participant; rolling back the batch."),
				*EncounterId.ToString());
			RollbackSpawnedParticipants();
			return false;
		}

		Participant->GetOnEnemyDied().AddUObject(this, &AEncounterController::HandleParticipantDied);
		Participant->SetEncounterDormant(this);
	}

	int32 FinishedTransformIndex = 0;
	for (AEnemy* Participant : RuntimeSpawnedParticipants)
	{
		if (!SpawnTransforms.IsValidIndex(FinishedTransformIndex))
		{
			RollbackSpawnedParticipants();
			return false;
		}

		const FTransform& SpawnTransform = SpawnTransforms[FinishedTransformIndex++];
		AActor* FinishedActor = UGameplayStatics::FinishSpawningActor(Participant, SpawnTransform);
		if (!IsValid(FinishedActor) || FinishedActor != Participant)
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' failed to finish-spawn a staged participant; rolling back the batch."),
				*EncounterId.ToString());
			RollbackSpawnedParticipants();
			return false;
		}
	}

	for (AEnemy* Participant : RuntimeSpawnedParticipants)
	{
		if (!IsValid(Participant) || !Participant->ActivateForEncounter(this, Player))
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' failed to activate a staged participant; rolling back the batch."),
				*EncounterId.ToString());
			RollbackSpawnedParticipants();
			return false;
		}
	}

	for (AEnemy* Participant : RuntimeSpawnedParticipants)
	{
		RemainingParticipants.Add(Participant);
	}

	SetEncounterState(EEncounterState::Active);
	SetBoundariesClosed(true);
	return true;
}

void AEncounterController::RollbackSpawnedParticipants()
{
	for (AEnemy* Participant : RuntimeSpawnedParticipants)
	{
		if (!IsValid(Participant))
		{
			continue;
		}

		Participant->GetOnEnemyDied().RemoveAll(this);
		Participant->ReleaseEncounterOwner(this);
		Participant->Destroy();
	}

	RuntimeSpawnedParticipants.Reset();
	RemainingParticipants.Reset();
}

bool AEncounterController::BuildRuntimeBoundaries()
{
	DestroyRuntimeBoundaries();

	const float HalfWallThickness = BoundaryConfig.WallThickness * 0.5f;
	const float WallHalfHeight = BoundaryConfig.WallHeight * 0.5f;
	bool bBuilt = false;

	if (BoundaryConfig.Shape == EEncounterBoundaryShape::Rectangle)
	{
		const FVector2D HalfExtents = BoundaryConfig.InteriorHalfExtents;
		bBuilt =
			CreateBoundarySegment(FVector(HalfExtents.X + HalfWallThickness, 0.f, WallHalfHeight), FRotator::ZeroRotator,
				FVector(HalfWallThickness, HalfExtents.Y + HalfWallThickness, WallHalfHeight)) &&
			CreateBoundarySegment(FVector(-HalfExtents.X - HalfWallThickness, 0.f, WallHalfHeight), FRotator::ZeroRotator,
				FVector(HalfWallThickness, HalfExtents.Y + HalfWallThickness, WallHalfHeight)) &&
			CreateBoundarySegment(FVector(0.f, HalfExtents.Y + HalfWallThickness, WallHalfHeight), FRotator::ZeroRotator,
				FVector(HalfExtents.X + HalfWallThickness, HalfWallThickness, WallHalfHeight)) &&
			CreateBoundarySegment(FVector(0.f, -HalfExtents.Y - HalfWallThickness, WallHalfHeight), FRotator::ZeroRotator,
				FVector(HalfExtents.X + HalfWallThickness, HalfWallThickness, WallHalfHeight));
	}
	else if (BoundaryConfig.Shape == EEncounterBoundaryShape::Radial)
	{
		bBuilt = true;
		const float SegmentAngleRadians = 2.f * PI / static_cast<float>(BoundaryConfig.RadialSegmentCount);
		const float SegmentHalfLength = BoundaryConfig.InteriorRadius * FMath::Tan(SegmentAngleRadians * 0.5f) + BoundarySegmentOverlap;

		for (int32 SegmentIndex = 0; SegmentIndex < BoundaryConfig.RadialSegmentCount; ++SegmentIndex)
		{
			const float AngleRadians = SegmentIndex * SegmentAngleRadians;
			const FVector RadialDirection(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.f);
			const FVector SegmentLocation = RadialDirection * (BoundaryConfig.InteriorRadius + HalfWallThickness) +
				FVector(0.f, 0.f, WallHalfHeight);
			const FRotator SegmentRotation(0.f, FMath::RadiansToDegrees(AngleRadians), 0.f);

			if (!CreateBoundarySegment(SegmentLocation, SegmentRotation,
				FVector(HalfWallThickness, SegmentHalfLength, WallHalfHeight)))
			{
				bBuilt = false;
				break;
			}
		}
	}
	else
	{
		bBuilt = BuildSplineBoundarySegments();
	}

	if (!bBuilt)
	{
		DestroyRuntimeBoundaries();
	}

	return bBuilt;
}

bool AEncounterController::BuildSplineBoundarySegments()
{
	TArray<FVector2D> Points;
	if (!GetSplineBoundaryPoints(Points) || Points.Num() < 3)
	{
		return false;
	}

	const float SignedArea = GetSignedPolygonArea(Points);
	if (FMath::IsNearlyZero(SignedArea, FMath::Square(SplinePointTolerance)))
	{
		return false;
	}

	const bool bCounterClockwise = SignedArea > 0.f;
	const float HalfWallThickness = BoundaryConfig.WallThickness * 0.5f;
	const float WallHalfHeight = BoundaryConfig.WallHeight * 0.5f;
	for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
	{
		const FVector2D& Start = Points[PointIndex];
		const FVector2D& End = Points[(PointIndex + 1) % Points.Num()];
		const FVector2D Edge = End - Start;
		const float EdgeLength = Edge.Size();
		if (EdgeLength <= SplinePointTolerance)
		{
			return false;
		}

		const FVector2D EdgeDirection = Edge / EdgeLength;
		const FVector2D OutwardNormal = bCounterClockwise
			? FVector2D(EdgeDirection.Y, -EdgeDirection.X)
			: FVector2D(-EdgeDirection.Y, EdgeDirection.X);
		const FVector2D SegmentCenter = (Start + End) * 0.5f + OutwardNormal * HalfWallThickness;
		const FRotator SegmentRotation(0.f, FMath::RadiansToDegrees(FMath::Atan2(EdgeDirection.Y, EdgeDirection.X)), 0.f);
		if (!CreateBoundarySegment(FVector(SegmentCenter.X, SegmentCenter.Y, WallHalfHeight), SegmentRotation,
			FVector(EdgeLength * 0.5f + BoundarySegmentOverlap, HalfWallThickness, WallHalfHeight)))
		{
			return false;
		}
	}

	return true;
}

bool AEncounterController::CreateBoundarySegment(const FVector& RelativeLocation, const FRotator& RelativeRotation,
	const FVector& BoxExtent)
{
	if (!Root)
	{
		return false;
	}

	const FName ComponentName = MakeUniqueObjectName(this, UBoxComponent::StaticClass(), TEXT("EncounterBoundarySegment"));
	UBoxComponent* BoundarySegment = NewObject<UBoxComponent>(this, ComponentName, RF_Transient);
	if (!BoundarySegment)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' failed to allocate a runtime boundary segment."), *EncounterId.ToString());
		return false;
	}

	AddInstanceComponent(BoundarySegment);
	BoundarySegment->SetupAttachment(Root);
	BoundarySegment->SetRelativeLocation(RelativeLocation);
	BoundarySegment->SetRelativeRotation(RelativeRotation);
	BoundarySegment->SetBoxExtent(BoxExtent, false);
	BoundarySegment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundarySegment->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoundarySegment->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	BoundarySegment->SetGenerateOverlapEvents(false);
	BoundarySegment->RegisterComponent();
	RuntimeBoundarySegments.Add(BoundarySegment);

	if (!CreateBoundaryVisualSegment(RelativeLocation, RelativeRotation, BoxExtent))
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' created collision for a boundary segment but could not create its fog visual."),
			*EncounterId.ToString());
	}

	return true;
}

bool AEncounterController::CreateBoundaryVisualSegment(const FVector& RelativeLocation, const FRotator& RelativeRotation,
	const FVector& BoxExtent)
{
	if (!Root || !BoundaryVisualMesh || !BoundaryVisualMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' is missing the Engine Cube or BoundaryVisualMaterial; fog presentation is disabled while collision remains available."),
			*EncounterId.ToString());
		return false;
	}

	const FName ComponentName = MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), TEXT("EncounterBoundaryVisual"));
	UStaticMeshComponent* BoundaryVisual = NewObject<UStaticMeshComponent>(this, ComponentName, RF_Transient);
	if (!BoundaryVisual)
	{
		return false;
	}

	AddInstanceComponent(BoundaryVisual);
	BoundaryVisual->SetupAttachment(Root);
	BoundaryVisual->SetStaticMesh(BoundaryVisualMesh);
	BoundaryVisual->SetMaterial(0, BoundaryVisualMaterial);
	BoundaryVisual->SetRelativeLocation(RelativeLocation);
	BoundaryVisual->SetRelativeRotation(RelativeRotation);
	BoundaryVisual->SetRelativeScale3D(BoxExtent * (2.f / 100.f));
	BoundaryVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundaryVisual->SetGenerateOverlapEvents(false);
	BoundaryVisual->SetCastShadow(false);
	BoundaryVisual->SetCastHiddenShadow(false);
	BoundaryVisual->SetVisibility(false, true);
	BoundaryVisual->SetHiddenInGame(true, true);
	BoundaryVisual->RegisterComponent();
	RuntimeBoundaryVisualSegments.Add(BoundaryVisual);
	return true;
}

bool AEncounterController::IsPlayerSafelyInsideCommitRegion(const AMyCharacter* Player) const
{
	if (!Player)
	{
		return false;
	}

	const FVector LocalPlayerLocation = GetActorTransform().InverseTransformPositionNoScale(Player->GetActorLocation());
	if (BoundaryConfig.Shape == EEncounterBoundaryShape::Rectangle)
	{
		return FMath::Abs(LocalPlayerLocation.X) <= CommitHalfExtents.X &&
			FMath::Abs(LocalPlayerLocation.Y) <= CommitHalfExtents.Y;
	}

	if (BoundaryConfig.Shape == EEncounterBoundaryShape::Radial)
	{
		return FVector2D(LocalPlayerLocation.X, LocalPlayerLocation.Y).SizeSquared() <= FMath::Square(CommitRadius);
	}

	const FVector2D LocalPlayerPoint(LocalPlayerLocation.X, LocalPlayerLocation.Y);
	return IsPointInsideSplineBoundary(LocalPlayerPoint) &&
		GetMinSquaredDistanceToSplineBoundary(LocalPlayerPoint) > FMath::Square(CommitSafetyInset);
}

bool AEncounterController::IsPlayerOverlappingCommitVolume(const AMyCharacter* Player) const
{
	const UPrimitiveComponent* CommitVolume = GetActiveCommitVolume();
	return Player && CommitVolume && CommitVolume->IsOverlappingActor(Player);
}

bool AEncounterController::IsPointInsideSplineBoundary(const FVector2D& LocalPoint) const
{
	if (SplineBoundaryPoints.Num() < 3 ||
		GetMinSquaredDistanceToSplineBoundary(LocalPoint) <= FMath::Square(SplinePointTolerance))
	{
		return false;
	}

	bool bIsInside = false;
	for (int32 PointIndex = 0, PreviousPointIndex = SplineBoundaryPoints.Num() - 1;
		PointIndex < SplineBoundaryPoints.Num(); PreviousPointIndex = PointIndex++)
	{
		const FVector2D& CurrentPoint = SplineBoundaryPoints[PointIndex];
		const FVector2D& PreviousPoint = SplineBoundaryPoints[PreviousPointIndex];
		const bool bStraddlesHorizontalRay = (CurrentPoint.Y > LocalPoint.Y) != (PreviousPoint.Y > LocalPoint.Y);
		if (bStraddlesHorizontalRay)
		{
			const float IntersectionX = (PreviousPoint.X - CurrentPoint.X) * (LocalPoint.Y - CurrentPoint.Y) /
				(PreviousPoint.Y - CurrentPoint.Y) + CurrentPoint.X;
			if (LocalPoint.X < IntersectionX)
			{
				bIsInside = !bIsInside;
			}
		}
	}

	return bIsInside;
}

float AEncounterController::GetSignedDistanceToSplineBoundary(const FVector2D& LocalPoint) const
{
	const float MinimumDistanceSquared = GetMinSquaredDistanceToSplineBoundary(LocalPoint);
	if (MinimumDistanceSquared == TNumericLimits<float>::Max())
	{
		return -TNumericLimits<float>::Max();
	}

	const float Distance = FMath::Sqrt(FMath::Max(0.f, MinimumDistanceSquared));
	return IsPointInsideSplineBoundary(LocalPoint) ? Distance : -Distance;
}

float AEncounterController::GetMinSquaredDistanceToSplineBoundary(const FVector2D& LocalPoint) const
{
	if (SplineBoundaryPoints.Num() < 2)
	{
		return TNumericLimits<float>::Max();
	}

	float MinimumDistanceSquared = TNumericLimits<float>::Max();
	for (int32 PointIndex = 0; PointIndex < SplineBoundaryPoints.Num(); ++PointIndex)
	{
		const FVector2D& Start = SplineBoundaryPoints[PointIndex];
		const FVector2D& End = SplineBoundaryPoints[(PointIndex + 1) % SplineBoundaryPoints.Num()];
		MinimumDistanceSquared = FMath::Min(MinimumDistanceSquared,
			GetPointToSegmentDistanceSquared2D(LocalPoint, Start, End));
	}

	return MinimumDistanceSquared;
}

UPrimitiveComponent* AEncounterController::GetActiveCommitVolume() const
{
	switch (BoundaryConfig.Shape)
	{
	case EEncounterBoundaryShape::Rectangle:
		return RectangularCommitVolume;

	case EEncounterBoundaryShape::Radial:
		return RadialCommitVolume;

	case EEncounterBoundaryShape::Spline:
		return SplineCommitCandidateVolume;

	default:
		return nullptr;
	}
}

void AEncounterController::SetCommitVolumeEnabled(UPrimitiveComponent* CommitVolume, bool bShouldEnable)
{
	if (!CommitVolume)
	{
		return;
	}

	CommitVolume->SetCollisionEnabled(bShouldEnable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	CommitVolume->SetGenerateOverlapEvents(bShouldEnable);
}

void AEncounterController::BeginPendingCommit(AMyCharacter* Player)
{
	if (!Player || !bConfigurationValid || EncounterState != EEncounterState::Idle || !bHasObservedPlayerOutsideCommitRegion)
	{
		return;
	}

	PendingCommitPlayer = Player;
	RefreshTickEnabled();
}

void AEncounterController::ClearPendingCommit()
{
	PendingCommitPlayer.Reset();
	RefreshTickEnabled();
}

void AEncounterController::RefreshTickEnabled()
{
	const bool bShouldTick = bAwaitingPlayerSetup ||
		(bConfigurationValid && EncounterState == EEncounterState::Idle &&
			(PendingCommitPlayer.IsValid() || InitialSafeRegionPlayer.IsValid()));
	SetActorTickEnabled(bShouldTick);
}

void AEncounterController::RestoreClearedStateFromSave()
{
	SetEncounterState(EEncounterState::Cleared);
	SetCommitVolumeEnabled(RectangularCommitVolume, false);
	SetCommitVolumeEnabled(RadialCommitVolume, false);
	SetCommitVolumeEnabled(SplineCommitCandidateVolume, false);
	SetBoundariesClosed(false);

	bAwaitingPlayerSetup = false;
	bConfigurationValid = false;
	bHasObservedPlayerOutsideCommitRegion = false;
	PendingCommitPlayer.Reset();
	InitialSafeRegionPlayer.Reset();
	RemainingParticipants.Reset();
	RuntimeSpawnedParticipants.Reset();
	SetActorTickEnabled(false);

	DestroyPreplacedParticipantsForClearedState();
	UE_LOG(LogTemp, Display, TEXT("Encounter '%s' restored as Cleared from the current save."), *EncounterId.ToString());
}

void AEncounterController::DestroyPreplacedParticipantsForClearedState()
{
	for (AEnemy* Participant : PreplacedParticipants)
	{
		if (IsValid(Participant))
		{
			Participant->Destroy();
		}
	}
}

void AEncounterController::ReleaseParticipants(bool bDestroySpawnedParticipants)
{
	for (AEnemy* Participant : PreplacedParticipants)
	{
		if (!IsValid(Participant))
		{
			continue;
		}

		Participant->GetOnEnemyDied().RemoveAll(this);
		Participant->ReleaseEncounterOwner(this);
	}

	for (AEnemy* Participant : RuntimeSpawnedParticipants)
	{
		if (!IsValid(Participant))
		{
			continue;
		}

		Participant->GetOnEnemyDied().RemoveAll(this);
		Participant->ReleaseEncounterOwner(this);
		if (bDestroySpawnedParticipants)
		{
			Participant->Destroy();
		}
	}

	RemainingParticipants.Reset();
	RuntimeSpawnedParticipants.Reset();
}

void AEncounterController::DestroyRuntimeBoundaries()
{
	for (UBoxComponent* BoundarySegment : RuntimeBoundarySegments)
	{
		if (IsValid(BoundarySegment))
		{
			BoundarySegment->DestroyComponent();
		}
	}

	RuntimeBoundarySegments.Reset();

	for (UStaticMeshComponent* BoundaryVisual : RuntimeBoundaryVisualSegments)
	{
		if (IsValid(BoundaryVisual))
		{
			BoundaryVisual->DestroyComponent();
		}
	}

	RuntimeBoundaryVisualSegments.Reset();
}

void AEncounterController::SetBoundariesClosed(bool bShouldClose)
{
	for (UBoxComponent* BoundarySegment : RuntimeBoundarySegments)
	{
		if (IsValid(BoundarySegment))
		{
			BoundarySegment->SetCollisionEnabled(bShouldClose ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}

	for (UStaticMeshComponent* BoundaryVisual : RuntimeBoundaryVisualSegments)
	{
		if (IsValid(BoundaryVisual))
		{
			BoundaryVisual->SetVisibility(bShouldClose, true);
			BoundaryVisual->SetHiddenInGame(!bShouldClose, true);
		}
	}
}

void AEncounterController::SetEncounterState(EEncounterState NewState)
{
	if (EncounterState == NewState)
	{
		return;
	}

	EncounterState = NewState;
	const TCHAR* StateName = NewState == EEncounterState::Idle ? TEXT("Idle") :
		(NewState == EEncounterState::Active ? TEXT("Active") : TEXT("Cleared"));
	UE_LOG(LogTemp, Display, TEXT("Encounter '%s' entered %s state."), *EncounterId.ToString(), StateName);
}

void AEncounterController::HandleParticipantDied(AEnemy* DefeatedEnemy)
{
	if (EncounterState != EEncounterState::Active || !IsValid(DefeatedEnemy))
	{
		return;
	}

	if (RemainingParticipants.Remove(DefeatedEnemy) == 0)
	{
		return;
	}

	if (!RemainingParticipants.IsEmpty())
	{
		return;
	}

	bool bPersistedClear = false;
	if (USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>())
	{
		bPersistedClear = GameInstance->TryMarkEncounterCleared(EncounterId);
	}

	if (!bPersistedClear)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Encounter '%s' could not persist its cleared state; continuing with current-session clear only."),
			*EncounterId.ToString());
	}

	SetEncounterState(EEncounterState::Cleared);
	SetBoundariesClosed(false);
	SetCommitVolumeEnabled(GetActiveCommitVolume(), false);
	ClearPendingCommit();
	ReleaseParticipants(false);
}
